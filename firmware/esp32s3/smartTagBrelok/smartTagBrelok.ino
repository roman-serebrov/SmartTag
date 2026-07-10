#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "TouchDrvCSTXXX.hpp"
#include "lv_conf.h"
#include <SensorQMI8658.hpp>
#include <Wire.h>
#include "XPowersLib.h"    // AXP2101 — заряд/зарядка батареи
#include "SensorPCF85063.hpp"  // RTC — часы для локскрина
#include "NimBLEDevice.h"

// ============================================================
//  БИСЕКЦИЯ ФИЧ (тач сломался после их включения — ищем какая виновата).
//  Сейчас ВСЁ 0 = только правка памяти (буферы /8 + опц. PSRAM-пул).
//  ШАГ 1: прошей как есть, проверь тач. Работает → память безопасна.
//  ШАГ 2: включай ПО ОДНОМУ (0→1), прошивай, проверяй тач:
//     сначала FEATURE_PMU, потом FEATURE_RTC, потом SCREEN_DIMMING_ENABLED.
//     На каком включении тач сломается — тот блок и виноват.
//  FEATURE_CTS (BLE-часы) — уже 0, из подозреваемых исключён.
// ============================================================
#define FEATURE_PMU        1   // AXP2101: батарея (индикатор) + Power Key — ФИКС спина опроса
#define FEATURE_RTC        1   // часы PCF85063 (локскрин по BOOT)
#define FEATURE_CTS        0   // BLE-синк времени (включать самым последним)
#define FEATURE_WATCHDOG   0   // BLE-watchdog рестарта рекламы (перенесён в задачу)

// ============================================================
//  ДИАГНОСТИКА. 0 = боевая прошивка: НИКАКИХ Serial-принтов в горячих путях.
//  Принт в my_refr_monitor_cb шёл после КАЖДОГО рефреша: на 115200 бод строка
//  ~3мс, а при заполнении TX-буфера Serial.printf БЛОКИРУЕТ — диагностика
//  сама создавала лаги, которые искала. Включать только на время отладки.
// ============================================================
#define DIAG 0

// ============================================================
//  Брелок: фундамент 06 Waveshare + иконки по паттерну track_load
//  Анимация смены как в lv_demo_music:
//   - старая картинка fade_out + едет в сторону + zoom уменьш.
//   - параллельно создаётся новая, fade_in + zoom overshoot
//   - старая удаляется по завершении
// ============================================================

extern const uint8_t inst_logo_map[];
extern const uint8_t telega_logo_map[];

// ============================================================
//  РУССКИЙ ШРИФТ (кириллица)
//  1) Сгенерируй font_ru_28.c на https://lvgl.io/tools/fontconverter
//     (Montserrat-Medium.ttf, size 28, bpp 4, range 0x20-0x7F,0x400-0x45F)
//  2) Положи файл в папку скетча
//  3) Раскомментируй строку ниже — русский заработает во всём UI
//  ФАЙЛ font_ru_28.c СГЕНЕРИРОВАН и лежит рядом со скетчем — define включён.
// ============================================================
#define USE_RU_FONT

#ifdef USE_RU_FONT
extern const lv_font_t font_ru_28;
#define APP_FONT &font_ru_28
#else
#define APP_FONT &lv_font_montserrat_28
#endif

// ============================================================
//  Маппинг bundle ID -> человеческое имя приложения
//  Неизвестные показываются как есть (сырой ID) — по ним пополняем таблицу.
// ============================================================
struct AppName { const char* bundle; const char* name; };
const AppName appNames[] = {
  { "com.apple.mobilephone",   "Phone" },
  { "com.apple.MobileSMS",     "Messages" },
  { "com.apple.mobilemail",    "Mail" },
  { "com.apple.facetime",      "FaceTime" },
  { "com.apple.fitness",       "Fitness" },
  { "com.burbn.instagram",     "Instagram" },
  { "ph.telegra.Telegraph",    "Telegram" },
  { "net.whatsapp.WhatsApp",   "WhatsApp" },
  { "com.google.ios.youtube",  "YouTube" },
  { "com.vk.vkclient",         "VK" },
  { "ru.sberbankmobile",       "Sber" },
  { "com.tinkoff.mobile",      "T-Bank" },
};
const uint8_t NUM_APP_NAMES = sizeof(appNames) / sizeof(appNames[0]);

// Вернуть красивое имя приложения по bundle ID (или сам ID, если неизвестно)
const char* appDisplayName(const char* bundle) {
  if (!bundle || !bundle[0]) return "";
  for (uint8_t i = 0; i < NUM_APP_NAMES; i++)
    if (strcmp(appNames[i].bundle, bundle) == 0) return appNames[i].name;
  return bundle;   // неизвестное — показываем сырой ID, потом добавим в таблицу
}

const lv_img_dsc_t inst_logo = {
  { LV_IMG_CF_TRUE_COLOR_ALPHA, 0, 0, 160, 160 },
  160 * 160 * LV_IMG_PX_SIZE_ALPHA_BYTE,
  inst_logo_map,
};
const lv_img_dsc_t telega_logo = {
  { LV_IMG_CF_TRUE_COLOR_ALPHA, 0, 0, 160, 160 },
  160 * 160 * LV_IMG_PX_SIZE_ALPHA_BYTE,
  telega_logo_map,
};

SensorQMI8658 qmi;
IMUdata acc;
float angleX = 1;
float angleY = 0;
bool rotation = false;

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

uint32_t screenWidth;
uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

TouchDrvCST92xx touch;
int16_t x[5], y[5];
// isPressed удалён — прерывание тача больше не используется (мешало жестам)

// АРХИТЕКТУРНОЕ ПРАВИЛО ШИНЫ WIRE (выстрадано): шину трогает ТОЛЬКО loop
// (ядро 1). Попытка выносить опрос в задачу на ядре 0 + мьютекс дала
// инверсию приоритетов: NimBLE вытеснял задачу ПОСРЕДИ транзакции с
// зажатым мьютексом, и тач ждал — деградация росла с BLE-трафиком.
// Один владелец шины = нет ожиданий вообще.

// --- Батарея (AXP2101 на общей шине Wire) ---
XPowersPMU pmu;
static bool pmuOnline = false;
static int  battPercent = -1;       // -1 = ещё не читали / нет батареи
static bool battCharging = false;

// --- Гашение экрана (энергосбережение) ---
// Power Key гасит/будит экран, авто-гашение по таймауту. Пробуждение ТОЛЬКО
// кнопками (Power Key / BOOT) — касание экрана НЕ будит (просьба Романа).
#define SCREEN_DIMMING_ENABLED 1
#define SCREEN_BRIGHTNESS   200            // рабочая яркость AMOLED
#define SCREEN_TIMEOUT_MS   20000          // автогашение после 20с бездействия
static bool     screenOn = true;
static uint32_t lastActivity = 0;          // millis() последнего касания/действия
static volatile bool wakeRequested = false; // из callback/IRQ -> loop разбудит

// --- [diag] приборы для поиска тормозов тача (печать раз в 5с в loop) ---
#if DIAG
static uint32_t diagMaxLoopMs = 0;     // худшая итерация loop за окно
static uint32_t diagMaxTouchGap = 0;   // худший разрыв между опросами тача
static uint32_t diagMaxPmuMs = 0;      // худший опрос Power Key/батареи
static uint32_t diagMaxImuMs = 0;      // худший опрос IMU (каждый цикл на шине)
static uint32_t diagMaxTouchRead = 0;  // худший ОДИН вызов touch.getPoint (контроллер)
static uint32_t diagMaxRender = 0;     // худший lv_timer_handler (отрисовка+ввод)
#endif

// Погасить экран: яркость 0 (AMOLED почти не ест энергию на выключенном)
static void screenSleep() {
  if (!screenOn) return;
  screenOn = false;
  gfx->setBrightness(0);
}
// Разбудить экран
static void screenWake() {
  lastActivity = millis();
  if (screenOn) return;
  screenOn = true;
  gfx->setBrightness(SCREEN_BRIGHTNESS);
}

// ============================================================
//  NFC (отдельная шина Wire1, пины 17/18)
// ============================================================
#define NFC_SDA 17
#define NFC_SCL 18
#define ST25DV_DATA_ADDR 0x53
#define ST25DV_SYS_ADDR  0x57
#define URI_PREFIX_HTTPS 0x04

typedef struct {
  const char* name;
  const char* url;
  const lv_img_dsc_t* img;
} Profile_t;

Profile_t profiles[] = {
  { "Instagram", "instagram.com/nfcsmarttag", &inst_logo },
  { "Telegram",  "t.me/roman_serebrov",        &telega_logo },
};
const uint8_t NUM_PROFILES = sizeof(profiles) / sizeof(profiles[0]);
uint8_t current = 0;

lv_obj_t *album_img = NULL;     // текущая активная картинка (как в demo_music)
lv_obj_t *name_label = NULL;
lv_obj_t *dots[8];
lv_obj_t *qr_obj = NULL;        // белая рамка QR (постоянная, скрыта)
lv_obj_t *qr_label = NULL;      // подсказка "Scan to open"
// По одному lv_qrcode на профиль, предсозданы при старте (в splash), скрыты.
// Показ = переключение видимости нужного (мгновенно, без перекодирования).
#define QR_MAX 8
lv_obj_t *qr_codes[QR_MAX] = {0};
bool animating = false;
bool qr_mode = false;           // true когда показан QR
uint32_t press_start = 0;       // момент начала касания
bool long_press_handled = false;
bool nfc_write_pending = false; // отложенная запись NFC (после анимации, не блокируя тач)

// ============================================================
//  ANCS — приём уведомлений с iPhone (Шаг А: в фоне + плашка-toast)
//  Карусель НЕ трогаем; всё новое живёт отдельно.
// ============================================================
static NimBLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");

// Current Time Service (штатный Apple/BLE): точное время с iPhone.
static NimBLEUUID ctsServiceUUID((uint16_t)0x1805);
static NimBLEUUID ctsCharUUID((uint16_t)0x2A2B);
static bool ctsSynced = false;      // время подведено в этой сессии подключения

// forward: локскрин и RTC определены ниже, а CTS-синк (выше) их использует
extern lv_obj_t *lock_overlay;
static void updateLockTime();
extern SensorPCF85063 rtc;
extern bool rtcOnline;
static NimBLEUUID nsCharUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");   // Notification Source
static NimBLEUUID cpCharUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");   // Control Point
static NimBLEUUID dsCharUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");   // Data Source

static NimBLEClient* pClient = nullptr;
bool ancsSubscribed = false;
uint32_t ancsSubscribedAt = 0;   // момент подписки: стартовую пачку принимаем без плашек

// --- Надёжный реконнект: состояние связи (Этап 4 брифа) ---
static NimBLEServer* gServer = nullptr;        // для watchdog и conn params
static NimBLEAdvertising* gAdv = nullptr;
volatile bool bleConnected = false;            // для индикатора на Home
volatile bool bleStatusDirty = true;           // loop() перекрасит индикатор
static uint16_t gConnHandle = 0;               // для запроса экономичных параметров
static bool connParamsPending = false;         // запросить параметры после подписки
static uint32_t connParamsAt = 0;

// --- Кольцевой буфер уведомлений ---
// ЧЕСТНОЕ КОЛЬЦО: feedHead всегда указывает на место следующей записи =
// самую старую при заполнении. Эвикция O(1) без поиска по timestamp,
// а обход «новые сверху» — просто итерация от head назад, БЕЗ сортировки
// (записи и так упорядочены по времени вставки).
// ПАМЯТЬ: сам массив в PSRAM (alloc в setup) — internal RAM не тратим.
// ПОТОКИ: feed[] пишут колбэки NimBLE (ядро 0) и читает loop (ядро 1) —
// ЛЮБОЙ доступ только под feedMux (секции короткие, микросекунды).
#define FEED_SIZE 20
struct Notif {
  uint32_t uid;
  uint8_t  category;
  char     app[48];
  char     title[64];
  char     message[128];
  bool     active;
  bool     wantDetails;   // ещё не получили AppID/Title/Message
  uint8_t  tries;         // сколько раз запрашивали (защита от вечного ретрая)
  bool     expanded;      // карточка раскрыта в ленте
};
static Notif* feed = nullptr;        // FEED_SIZE записей, PSRAM (см. setup)
static uint8_t feedHead = 0;         // индекс СЛЕДУЮЩЕЙ записи (= самой старой при заполнении)
static uint8_t feedCount = 0;
static portMUX_TYPE feedMux = portMUX_INITIALIZER_UNLOCKED;

// Индекс k-й записи от НОВЕЙШЕЙ (k=0 — самое свежее). Звать под feedMux.
static inline uint8_t feedNth(uint8_t k) {
  return (uint8_t)((feedHead + FEED_SIZE - 1 - k) % FEED_SIZE);
}

// Поиск записи по UID (для дозаписи деталей и удаления). -1 если нет.
// Ищем и неактивные тоже: детали могут прийти после Removed (гонка) — не теряем их.
// Валидные записи всегда лежат в [0, feedCount) — порядок для поиска не важен.
// Звать ТОЛЬКО под feedMux.
int feedFind(uint32_t uid) {
  for (uint8_t i = 0; i < feedCount; i++)
    if (feed[i].uid == uid) return i;
  return -1;
}
// Добавить новую запись (кольцо: при заполнении затирается самая старая).
// Звать ТОЛЬКО под feedMux.
int feedAdd(uint32_t uid, uint8_t category) {
  int slot = feedHead;
  feedHead = (uint8_t)((feedHead + 1) % FEED_SIZE);
  if (feedCount < FEED_SIZE) feedCount++;
  feed[slot].uid = uid;
  feed[slot].category = category;
  feed[slot].app[0] = '\0';
  feed[slot].title[0] = '\0';
  feed[slot].message[0] = '\0';
  feed[slot].active = true;
  feed[slot].wantDetails = true;   // loop() запросит детали, когда дойдёт очередь
  feed[slot].tries = 0;
  feed[slot].expanded = false;
  return slot;
}

// --- Обмен BLE-поток -> loop() (LVGL трогаем только в loop) ---
// Раньше: toast_pending + голые глобальные char-буферы. Гонка: loop читал
// toast_title, пока dataSourceCb (ядро 0) его перезаписывал — рваные строки.
// Теперь плашки едут через FreeRTOS-очередь (по образцу cpQueue): каждая —
// атомарная копия, глобальных строк-буферов нет вообще.
struct ToastMsg { char title[64]; char text[128]; char app[48]; };
static QueueHandle_t toastQueue = NULL;   // создаётся в setup ДО initBLE

// Очередь запроса деталей: ОДИН запрос в полёте.
// Раньше был единственный pendingDetailUID — при стартовой пачке от iPhone
// каждый новый Added затирал предыдущий, детали получал только последний,
// остальные оставались без ника ("Notification"). Теперь очередь честная.
volatile uint32_t detailsInFlightUid = 0;  // чей запрос в полёте (0 = никого); пишут оба ядра — volatile
uint32_t detailsSentAt = 0;                // когда отправлен (для таймаута), только loop

// --- Звонок (полноэкранный overlay) ---
volatile bool call_show = false;       // показать экран звонка
volatile bool call_hide = false;       // убрать экран звонка
uint32_t call_uid = 0;                 // UID активного звонка
char call_name[64] = "";               // имя/номер звонящего

// --- Разбор Data Source (детали: AppID=0, Title=1, Message=3) ---

// Копирование текста с фильтром: оставляем ТОЛЬКО символы нашего шрифта
// (ASCII + кириллица + троеточие). Эмодзи и прочие глифы шрифт не содержит —
// LVGL рисовал бы вместо них кракозябры. Переносы строк -> пробел.
static size_t copySanitized(char* dst, size_t dstSize, const uint8_t* src, size_t srcLen) {
  size_t o = 0, i = 0;
  while (i < srcLen && o + 4 < dstSize) {
    uint8_t b = src[i];
    uint32_t cp; size_t n;
    if (b < 0x80)                              { cp = b; n = 1; }
    else if ((b & 0xE0) == 0xC0 && i + 1 < srcLen) {
      cp = ((uint32_t)(b & 0x1F) << 6) | (src[i+1] & 0x3F); n = 2;
    }
    else if ((b & 0xF0) == 0xE0 && i + 2 < srcLen) {
      cp = ((uint32_t)(b & 0x0F) << 12) | ((uint32_t)(src[i+1] & 0x3F) << 6) | (src[i+2] & 0x3F); n = 3;
    }
    else if ((b & 0xF8) == 0xF0 && i + 3 < srcLen) { cp = 0x10000; n = 4; }  // эмодзи и пр.
    else { i++; continue; }                    // битый байт — пропускаем

    if (cp == '\n' || cp == '\t') {            // перенос/таб -> одиночный пробел
      if (o > 0 && dst[o-1] != ' ') dst[o++] = ' ';
    } else if ((cp >= 0x20 && cp <= 0x7E) ||   // ASCII
               (cp >= 0x400 && cp <= 0x45F) || // кириллица (с Ё/ё)
               cp == 0x2026) {                 // …
      memcpy(&dst[o], &src[i], n); o += n;
    }
    // всё остальное (эмодзи, символы, CJK...) молча выкидываем
    i += n;
  }
  dst[o] = '\0';
  return o;
}

static void dataSourceCb(NimBLERemoteCharacteristic* c, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 5) return;
  uint32_t uid = pData[1] | (pData[2] << 8) | (pData[3] << 16) | ((uint32_t)pData[4] << 24);
  if (uid == detailsInFlightUid) detailsInFlightUid = 0;  // полёт свободен — loop() пошлёт следующий

  // Сначала фильтруем атрибуты во ВРЕМЕННЫЕ буферы (вне лока: copySanitized
  // не должен исполняться с выключенными прерываниями), потом коротко под
  // feedMux переносим в feed[] и снимаем копию для плашки.
  char aApp[48] = "", aTitle[64] = "", aMsg[128] = "";
  bool hasApp = false, hasTitle = false, hasMsg = false;

  size_t idx = 5;
  while (idx + 3 <= length) {
    uint8_t  attrID  = pData[idx];
    uint16_t attrLen = pData[idx + 1] | (pData[idx + 2] << 8);
    idx += 3;
    if (idx + attrLen > length) break;
    if (attrLen > 0) {
      if (attrID == 0) {  // AppIdentifier (всегда ASCII — фильтр не нужен)
        uint16_t n = attrLen < sizeof(aApp) - 1 ? attrLen : sizeof(aApp) - 1;
        memcpy(aApp, &pData[idx], n); aApp[n] = '\0'; hasApp = true;
      } else if (attrID == 1) {  // Title — фильтруем эмодзи
        copySanitized(aTitle, sizeof(aTitle), &pData[idx], attrLen); hasTitle = true;
      } else if (attrID == 3) {  // Message — фильтруем эмодзи
        copySanitized(aMsg, sizeof(aMsg), &pData[idx], attrLen); hasMsg = true;
      }
    }
    idx += attrLen;
  }

  ToastMsg tm;
  bool isCall = false, found = false;
  taskENTER_CRITICAL(&feedMux);
  int slot = feedFind(uid);
  if (slot >= 0) {
    found = true;
    feed[slot].wantDetails = false;                        // детали пришли — из очереди долой
    if (hasApp)   strcpy(feed[slot].app, aApp);            // размеры буферов совпадают
    if (hasTitle) strcpy(feed[slot].title, aTitle);
    if (hasMsg)   strcpy(feed[slot].message, aMsg);
    // Если это активный звонок — обновим имя на экране звонка, плашку НЕ показываем
    if (feed[slot].uid == call_uid && feed[slot].category == 1) {
      isCall = true;
      strncpy(call_name, feed[slot].title[0] ? feed[slot].title : "Incoming call", sizeof(call_name) - 1);
      call_name[sizeof(call_name) - 1] = '\0';
    } else {
      strncpy(tm.title, feed[slot].title[0] ? feed[slot].title : "Notification", sizeof(tm.title) - 1);
      tm.title[sizeof(tm.title) - 1] = '\0';
      strncpy(tm.text, feed[slot].message, sizeof(tm.text) - 1);
      tm.text[sizeof(tm.text) - 1] = '\0';
      strncpy(tm.app, feed[slot].app, sizeof(tm.app) - 1);
      tm.app[sizeof(tm.app) - 1] = '\0';
    }
  }
  taskEXIT_CRITICAL(&feedMux);

  if (!found) return;
  if (isCall) { call_show = true; return; }   // перерисовать экран звонка с именем
  // Плашка уезжает в loop атомарной копией (очередь полна = пропуск, не ждём)
  xQueueSend(toastQueue, &tm, 0);
}

// --- Разбор Notification Source (событие: добавлено/удалено) ---
static void notificationSourceCb(NimBLERemoteCharacteristic* c, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 8) return;
  uint32_t uid = pData[4] | (pData[5] << 8) | (pData[6] << 16) | ((uint32_t)pData[7] << 24);

  if (pData[0] == 0) {          // Added
    taskENTER_CRITICAL(&feedMux);
    feedAdd(uid, pData[2]);     // wantDetails=true внутри — loop() запросит по очереди
    taskEXIT_CRITICAL(&feedMux);
    if (pData[2] == 1) {        // Incoming Call — полноэкранный звонок
      call_uid = uid;
      call_show = true;
    }
#if DIAG
    Serial.printf("ANCS added: cat=%d uid=%u\n", pData[2], uid);
#endif
  } else if (pData[0] == 2) {   // Removed
    taskENTER_CRITICAL(&feedMux);
    int slot = feedFind(uid);
    if (slot >= 0) feed[slot].active = false;
    taskEXIT_CRITICAL(&feedMux);
    if (uid == call_uid) {      // звонок завершён — убрать экран
      call_hide = true;
    }
#if DIAG
    Serial.printf("ANCS removed: uid=%u\n", uid);
#endif
  }
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    pClient = s->getClient(info);
    gConnHandle = info.getConnHandle();
    bleConnected = true;
    bleStatusDirty = true;
    Serial.println("iPhone connected");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    pClient = nullptr;
    ancsSubscribed = false;
    ctsSynced = false;             // при след. подключении подведём часы заново
    detailsInFlightUid = 0;        // очередь деталей не должна ждать мёртвый ответ
    connParamsPending = false;
    bleConnected = false;
    bleStatusDirty = true;
    // Мониторинг причин (Этап 4 брифа): 531 (0x213) = iOS сама закрыла соединение —
    // штатно при выходе из радиуса/энергосбережении; частые 531 подряд без движения
    // могут означать проблему бонда (тогда поможет повторный пэйринг).
    Serial.printf("iPhone disconnected, reason %d%s\n", reason,
                  reason == 531 ? " (iOS terminated - re-advertising, iPhone вернётся сам)" : "");
  }
} serverCallbacks;

// --- Единая запись в ANCS Control Point (устраняет дублирование) ---
// Возвращает true при успехе. Используется для запроса деталей и действий звонка.
// Прочитать точное время с iPhone (Current Time Service) и подвести RTC.
// Прочитать точное время с iPhone (Current Time Service) и подвести RTC.
// iOS отдаёт CTS доверенным (bonded) устройствам — бонд от ANCS даёт права,
// НО сервис может появиться не мгновенно после connect. Возвращаем true при
// успехе/окончательной неудаче, false = "ещё не готово, повтори позже".
static bool tryCtsSync() {
  if (!rtcOnline || pClient == nullptr || !pClient->isConnected()) return true;

  // Targeted getService (БЕЗ getServices(true) — тот полный re-discovery
  // раньше сбивал ANCS-подписки и уведомления пропадали). Ищем только 0x1805.
  auto svc = pClient->getService(ctsServiceUUID);
  if (!svc) { Serial.println("CTS: service not found"); return false; }
  auto ch = svc->getCharacteristic(ctsCharUUID);
  if (!ch) { Serial.println("CTS: char not found (retry)"); return false; }
  if (!ch->canRead()) { Serial.println("CTS: char not readable"); return true; }

  std::string v = ch->readValue();
  if (v.size() < 7) { Serial.printf("CTS: short read (%d), retry\n", (int)v.size()); return false; }
  const uint8_t* d = (const uint8_t*)v.data();
  // Раскладка 0x2A2B: [0-1] Year uint16 LE, [2] Month, [3] Day, [4] Hour, [5] Min, [6] Sec
  int Y = d[0] | (d[1] << 8);
  int M = d[2], D = d[3], h = d[4], m = d[5], s = d[6];
  if (Y < 2024 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31) {
    Serial.printf("CTS: bad data %04d-%02d-%02d, retry\n", Y, M, D);
    return false;
  }
  rtc.setDateTime(Y, M, D, h, m, s);
  ctsSynced = true;
  Serial.printf("CTS: time synced %04d-%02d-%02d %02d:%02d:%02d\n", Y, M, D, h, m, s);
  if (lock_overlay) updateLockTime();   // если локскрин открыт — сразу обновить
  return true;
}

// Клиент передаётся ПАРАМЕТРОМ: раньше функция заново читала глобальный
// pClient, и «локальная копия против гонки» в вызывающем коде не работала —
// onDisconnect мог занулить pClient между проверкой и записью.
static bool ancsWriteCP(NimBLEClient* cli, const uint8_t* data, size_t len) {
  if (cli == nullptr || !cli->isConnected()) return false;
  auto svc = cli->getService(ancsServiceUUID);
  if (!svc) return false;
  auto cp = svc->getCharacteristic(cpCharUUID);
  if (!cp) return false;
  return cp->writeValue(data, len, true);
}

// --- Асинхронная запись в Control Point (отдельная FreeRTOS-задача) ---
// writeValue с подтверждением БЛОКИРУЕТ вызывающую задачу до ответа iPhone.
// На полумёртвом линке (ушёл с брелком от телефона, supervision ещё жив)
// это до 30 секунд GATT-таймаута. Вызов из loop() замораживал весь UI:
// тач не читается, экран стоит. Теперь loop только кладёт запрос в очередь,
// а блокируется отдельная задача — UI не страдает.
static QueueHandle_t cpQueue = NULL;
struct CpMsg { uint8_t len; uint8_t data[16]; };

static void ancsWriteTask(void*) {
  CpMsg m;
  for (;;) {
    if (xQueueReceive(cpQueue, &m, portMAX_DELAY) == pdTRUE) {
      // Локальная копия: onDisconnect (поток NimBLE) может занулить pClient
      // между нашей проверкой и записью — со старым кодом это был краш.
      NimBLEClient* cli = pClient;
      if (cli == nullptr || !cli->isConnected()) continue;
      bool ok = ancsWriteCP(cli, m.data, m.len);   // блокируется ЗДЕСЬ — UI живёт своей жизнью
      if (!ok) Serial.println("[ancs] CP write failed");
    }
  }
}

static bool ancsWriteCPAsync(const uint8_t* data, size_t len) {
  if (!cpQueue || len > sizeof(CpMsg::data)) return false;
  CpMsg m;
  m.len = (uint8_t)len;
  memcpy(m.data, data, len);
  return xQueueSend(cpQueue, &m, 0) == pdTRUE;   // не ждём: очередь полна = пропуск
}

// --- Задача подписки на ANCS (ядро 0) ---
// getService (discovery) и subscribe — БЛОКИРУЮЩИЕ вызовы NimBLE: на плохом
// линке висят до 30с GATT-таймаута. Из loop это замораживало весь UI на
// 30 секунд при каждом флапе соединения (диагностика показала loop=30315ms).
// ПРАВИЛО: loop НИКОГДА не зовёт блокирующие BLE-вызовы — все они в задачах.
static void ancsSubscribeTask(void*) {
  for (;;) {
    if (pClient != nullptr && pClient->isConnected() && !ancsSubscribed) {
      NimBLEClient* cli = pClient;              // локальная копия против гонки
      auto svc = cli->getService(ancsServiceUUID);   // блокируется ЗДЕСЬ — UI живёт
      if (svc && cli->isConnected()) {
        auto ns = svc->getCharacteristic(nsCharUUID);
        auto ds = svc->getCharacteristic(dsCharUUID);
        if (ns && ds && cli->isConnected()) {
          ds->subscribe(true, dataSourceCb);
          ns->subscribe(true, notificationSourceCb);
          ancsSubscribed = true;
          ancsSubscribedAt = millis();   // для тихого приёма стартовой пачки
          connParamsPending = true;      // экономичные параметры через 3с (из loop)
          connParamsAt = millis() + 3000;
          Serial.println("ANCS subscribed");
        }
      }
    }
#if FEATURE_CTS
    // BLE-часы: после подписки ANCS пробуем подвести время по CTS.
    // В ЭТОЙ ЖЕ задаче (ядро 0), targeted-поиск, максимум 3 попытки, потом
    // сдаёмся до переподключения. loop и уведомления не трогаем.
    static uint8_t ctsTries = 0;
    if (ancsSubscribed && !ctsSynced) {
      if (ctsTries < 3) {
        if (tryCtsSync()) ctsTries = 0;   // успех/окончательно
        else ctsTries++;
      }
    } else if (!ancsSubscribed) {
      ctsTries = 0;                        // связь слетела — на реконнекте пробуем заново
    }
#endif

#if FEATURE_WATCHDOG
    // Advertising-watchdog: если связи нет и реклама не идёт — поднять её.
    // advertiseOnDisconnect обычно делает это сам, но если реклама умерла
    // криво — восстанавливаем. ЗДЕСЬ (ядро 0, BLE-задача), НЕ в loop:
    // gAdv->start()/getConnectedCount блокируют на мьютексе хоста, из loop
    // это вешало тач. iPhone по бонду переподключится сам.
    if (gAdv && gServer && gServer->getConnectedCount() == 0 && !gAdv->isAdvertising()) {
      bool ok = gAdv->start();
      Serial.printf("BLE watchdog: advertising restart %s\n", ok ? "OK" : "FAILED");
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(1000));     // проверка раз в секунду
  }
}

// --- Действие по звонку: принять (0x00) / отклонить (0x01) — одна функция вместо двух дублей ---
static void callAction(uint8_t action) {
  uint8_t v[6] = {0x02, (uint8_t)call_uid, (uint8_t)(call_uid >> 8),
                  (uint8_t)(call_uid >> 16), (uint8_t)(call_uid >> 24), action};
  ancsWriteCPAsync(v, 6);   // не блокируем LVGL-событие кнопки звонка
}

// ============================================================
//  СИСТЕМА МОДУЛЕЙ (плагинная, из ANCS_architecture.md)
//  Каждый модуль = свой LVGL-экран, создан ОДИН раз при старте.
//  Переключение = lv_scr_load_anim (мгновенно, изоляция жестов бесплатно:
//  события идут только активному экрану).
//  ДОБАВИТЬ НОВЫЙ МОДУЛЬ: написать buildXxx(scr) + строка в modules[].
// ============================================================
enum ModuleId {
  MOD_HOME = 0,        // домашний экран-меню (launcher)
  MOD_LINKS,           // карусель профилей (Instagram/Telegram + NFC + QR)
  MOD_NOTIFICATIONS,   // лента уведомлений
  MOD_CARDS,           // карты лояльности (EAN-13 на экране)
  // MOD_GPS,          // будущее: добавить сюда
  // MOD_FITNESS,      // будущее: шагомер
  MOD_COUNT
};

struct Module {
  const char* name;                 // подпись в меню Home
  lv_obj_t*   screen;               // LVGL-экран модуля (создаётся один раз)
  void (*build)(lv_obj_t* scr);     // построить UI (вызывается один раз в setup)
  void (*onEnter)();                // при входе (обновить данные), может быть NULL
};

// forward-объявления билдеров и хуков
static void buildHome(lv_obj_t* scr);
static void buildCarousel(lv_obj_t* scr);
static void buildNotifications(lv_obj_t* scr);
static void buildCards(lv_obj_t* scr);
static void notifOnEnter();
static void homeOnEnter();

Module modules[MOD_COUNT] = {
  { "Home",          NULL, buildHome,          homeOnEnter },
  { "Links",         NULL, buildCarousel,      NULL },
  { "Notifications", NULL, buildNotifications, notifOnEnter },
  { "Карты",         NULL, buildCards,         NULL },
  // { "GPS",        NULL, buildGPS,           gpsOnEnter },      <- будущее
  // { "Fitness",    NULL, buildFitness,       fitnessOnEnter },  <- будущее
};
uint8_t activeModule = MOD_HOME;

void switchToModule(uint8_t id) {
  if (id >= MOD_COUNT || id == activeModule) return;
  Serial.printf("[nav] -> %s\n", modules[id].name);        // хлебная крошка для отладки фриза
  activeModule = id;
  // Гасим текущее удержание: если модуль открыт долгим нажатием, тот же
  // жест иначе долетит до нового экрана (карусель ловила свой long-press
  // на 600мс и открывала QR поверх только что открытых Links).
  long_press_handled = true;                        // ручной детект QR в touchpad_read пропустит
  lv_indev_wait_release(lv_indev_get_act());        // LVGL-события до отпускания пальца — тоже гасим
  if (modules[id].onEnter) modules[id].onEnter();   // обновить данные перед показом
  Serial.println("[nav] onEnter done");
  // Мгновенное переключение (без fade): fade рендерит 2 экрана 466×466 разом
  // и подвисает. Простая загрузка — без лагов.
  lv_scr_load(modules[id].screen);
  Serial.println("[nav] screen loaded");
}
void goHome() { switchToModule(MOD_HOME); }


// ============================================================
//  NFC функции
// ============================================================
void NFC_Unlock() {
  Wire1.beginTransmission(ST25DV_SYS_ADDR);
  Wire1.write(0x09); Wire1.write(0x00);
  for (int i = 0; i < 8; i++) Wire1.write(0x00);
  Wire1.write(0x09);
  for (int i = 0; i < 8; i++) Wire1.write(0x00);
  Wire1.endTransmission();
  delay(10);
}
void NFC_WriteBlock(uint16_t addr, uint8_t* data, uint16_t size) {
  Wire1.beginTransmission(ST25DV_DATA_ADDR);
  Wire1.write((uint8_t)(addr >> 8));
  Wire1.write((uint8_t)(addr & 0xFF));
  Wire1.write(data, size);
  Wire1.endTransmission();
  delay(6);
}
void NFC_WriteURL(const char* url, uint8_t prefix) {
  NFC_Unlock();
  uint8_t cc[4] = { 0xE1, 0x40, 0x40, 0x00 };
  NFC_WriteBlock(0x0000, cc, 4);
  uint16_t url_len = strlen(url);
  if (url_len > 200) url_len = 200;             // защита от переполнения буфера
  // Собираем сразу в ОДИН буфер (раньше: ndef -> побайтовая перекладка в full)
  static uint8_t full[210];                     // static — не нагружаем стек
  uint16_t ndef_len = 5 + url_len;              // D1 01 len 55 prefix + url
  full[0] = 0x03; full[1] = (uint8_t)ndef_len;  // TLV: тип + длина
  full[2] = 0xD1; full[3] = 0x01;
  full[4] = (uint8_t)(1 + url_len);             // payload = prefix + url
  full[5] = 0x55; full[6] = prefix;
  memcpy(&full[7], url, url_len);
  uint16_t fidx = 7 + url_len;
  full[fidx++] = 0xFE;                          // терминатор TLV
  uint16_t remaining = fidx, offset = 0;
  while (remaining > 0) {
    uint16_t chunk = (remaining > 60) ? 60 : remaining;
    NFC_WriteBlock(0x0004 + offset, &full[offset], chunk);
    offset += chunk; remaining -= chunk; delay(5);
  }
  Serial.print("NFC -> https://"); Serial.println(url);
}

// ============================================================
//  UI-ХЕЛПЕРЫ: фабрики повторяющихся элементов (дедупликация).
//  Поведение 1:1 с развёрнутым кодом — тут НЕТ новой логики.
//  Правило: хелпер покрывает общую часть, уникальные свойства
//  (доп. стили, флаги) навешиваются после вызова как раньше.
// ============================================================
// Подпись: создать + текст + шрифт (NULL = шрифт темы) + цвет + выравнивание
static lv_obj_t* mkLabel(lv_obj_t* p, const char* txt, const lv_font_t* f,
                         uint32_t hex, lv_align_t al, lv_coord_t x, lv_coord_t y) {
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, txt);
  if (f) lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(hex), 0);
  lv_obj_align(l, al, x, y);
  return l;
}

// Панель: контейнер с фоном/радиусом, без рамки и без скролла
static lv_obj_t* mkPanel(lv_obj_t* p, lv_coord_t w, lv_coord_t h,
                         uint32_t bgHex, lv_coord_t radius) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(bgHex), 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

// Круглая кнопка с символом montserrat (звонок: принять/отклонить)
static lv_obj_t* mkRoundBtn(lv_obj_t* p, lv_coord_t size, uint32_t bgHex,
                            const char* sym, lv_coord_t x, lv_coord_t y,
                            lv_event_cb_t cb) {
  lv_obj_t* b = lv_btn_create(p);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(bgHex), 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_align(b, LV_ALIGN_CENTER, x, y);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, sym);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
  lv_obj_center(l);
  return b;
}

// Кнопка-«пилюля» с текстом/символом по центру (домик, Clear)
static lv_obj_t* mkPillBtn(lv_obj_t* p, lv_coord_t w, lv_coord_t h,
                           uint32_t bgHex, const char* txt, lv_event_cb_t cb) {
  lv_obj_t* b = lv_btn_create(p);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(bgHex), 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  return b;
}

// Запуск анимации: один вызов вместо блока из 7-9 строк lv_anim_*.
// ready-колбэк опционален (NULL = без него).
static void animStart(lv_obj_t* obj, int32_t from, int32_t to, uint16_t ms,
                      lv_anim_path_cb_t path, lv_anim_exec_xcb_t exec,
                      lv_anim_ready_cb_t ready) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_time(&a, ms);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_path_cb(&a, path);
  lv_anim_set_exec_cb(&a, exec);
  if (ready) lv_anim_set_ready_cb(&a, ready);
  lv_anim_start(&a);
}

// ============================================================
//  Адаптеры для анимации (как в demo_music)
// ============================================================
static void _img_set_zoom_anim_cb(void * obj, int32_t zoom) {
  lv_img_set_zoom((lv_obj_t *)obj, (uint16_t)zoom);
}
static void _obj_set_x_anim_cb(void * obj, int32_t x) {
  lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)x);
}
// После слайда возвращаем выравнивание по центру (иначе картинка застревает на абс. X).
// Здесь же снимаем блокировку animating — отдельный lv_timer больше не нужен.
static void _album_recenter_cb(lv_anim_t * a) {
  lv_obj_align((lv_obj_t *)a->var, LV_ALIGN_CENTER, 0, -20);
  animating = false;
}

// Создание новой картинки альбома (как album_img_create в demo_music)
// Родитель — экран модуля Links (не lv_scr_act: активным может быть другой модуль)
static lv_obj_t* album_create(uint8_t idx) {
  lv_obj_t *img = lv_img_create(modules[MOD_LINKS].screen);
  lv_img_set_src(img, profiles[idx].img);
  lv_img_set_antialias(img, false);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, -20);
  return img;
}

void updateLabelDots(uint8_t idx) {
  lv_label_set_text(name_label, profiles[idx].name);
  lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 120);
  for (uint8_t i = 0; i < NUM_PROFILES; i++) {
    if (i == idx) {
      lv_obj_set_style_bg_color(dots[i], lv_color_white(), 0);
      lv_obj_set_size(dots[i], 14, 14);
    } else {
      lv_obj_set_style_bg_color(dots[i], lv_color_hex(0x555555), 0);
      lv_obj_set_size(dots[i], 10, 10);
    }
  }
}

// ============================================================
//  Смена альбома — ТОЧНО по паттерну track_load из demo_music
// ============================================================
static void album_next(bool next) {
  if (animating) return;
  animating = true;

  // Новый индекс
  uint8_t idx = current;
  if (next) { idx++; if (idx >= NUM_PROFILES) idx = 0; }
  else      { if (idx == 0) idx = NUM_PROFILES - 1; else idx--; }
  current = idx;

  updateLabelDots(idx);

  // --- ПРОСТОЙ СЛАЙД (одинаково в обе стороны, без fade/zoom — быстрее) ---
  // Старая картинка уезжает за край и удаляется.
  if (album_img) {
    int32_t cur_x = lv_obj_get_x(album_img);
    animStart(album_img, cur_x, next ? cur_x - LCD_WIDTH : cur_x + LCD_WIDTH,
              250, lv_anim_path_ease_in_out, _obj_set_x_anim_cb,
              lv_obj_del_anim_ready_cb);
  }

  // Новая картинка въезжает с противоположного края в центр.
  // В конце — жёстко вернуть выравнивание по центру (_album_recenter_cb),
  // иначе картинка «застревает» на абсолютной X и уезжает в угол.
  album_img = album_create(idx);
  lv_obj_update_layout(modules[MOD_LINKS].screen);
  int32_t center_x = lv_obj_get_x(album_img);            // целевая позиция (центр)
  animStart(album_img, next ? center_x + LCD_WIDTH : center_x - LCD_WIDTH, center_x,
            250, lv_anim_path_ease_in_out, _obj_set_x_anim_cb, _album_recenter_cb);

  // NFC переписываем ОТЛОЖЕННО (после анимации, чтобы не блокировать тач)
  nfc_write_pending = true;
  // Разблокировка animating — в _album_recenter_cb по завершении анимации
}

// ============================================================
//  QR-режим — показ/скрытие красивого QR кода
// ============================================================
// Предгенерация QR всех профилей в PSRAM. Кодируем во временный lv_qrcode,
// копируем его буфер (матрицу) в свой PSRAM-буфер, временный удаляем — пул
// освобождается. Показ потом = просто подставить нужный буфер (мгновенно).
// Предсоздание QR всех профилей (внутри рамки qr_obj), все скрыты.
// Кодирование происходит один раз ЗДЕСЬ (на splash), показ потом мгновенный.
static void createQRs(lv_obj_t *frame) {
  for (uint8_t i = 0; i < NUM_PROFILES && i < QR_MAX; i++) {
    char url[128];
    snprintf(url, sizeof(url), "https://%s", profiles[i].url);
    qr_codes[i] = lv_qrcode_create(frame, 244, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr_codes[i], url, strlen(url));
    lv_obj_center(qr_codes[i]);
    lv_obj_add_flag(qr_codes[i], LV_OBJ_FLAG_HIDDEN);
    Serial.printf("QR[%d] ready: %s\n", i, url);
  }
}

// Видимость элементов карусели (иконка+имя+точки) — дедупликация show/hide
static void carouselElemsHidden(bool hidden) {
  if (hidden) {
    lv_obj_add_flag(album_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(name_label, LV_OBJ_FLAG_HIDDEN);
    for (uint8_t i = 0; i < NUM_PROFILES; i++) lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(album_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(name_label, LV_OBJ_FLAG_HIDDEN);
    for (uint8_t i = 0; i < NUM_PROFILES; i++) lv_obj_clear_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  }
}

static void show_qr() {
  if (qr_mode) return;
  carouselElemsHidden(true);
  // Показываем предсозданный QR текущего профиля (мгновенно, без кодирования)
  for (uint8_t i = 0; i < NUM_PROFILES && i < QR_MAX; i++) {
    if (!qr_codes[i]) continue;
    if (i == current) lv_obj_clear_flag(qr_codes[i], LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(qr_codes[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_clear_flag(qr_obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(qr_label, LV_OBJ_FLAG_HIDDEN);
  qr_mode = true;
  Serial.printf("QR shown (profile %d)\n", current);
}

static void hide_qr() {
  if (!qr_mode) return;
  if (qr_obj)   lv_obj_add_flag(qr_obj, LV_OBJ_FLAG_HIDDEN);
  if (qr_label) lv_obj_add_flag(qr_label, LV_OBJ_FLAG_HIDDEN);
  carouselElemsHidden(false);
  qr_mode = false;
  Serial.println("QR hidden");
}

// Жест свайпа
static void screen_gesture_cb(lv_event_t * e) {
  if (qr_mode) return;   // в QR-режиме свайпы игнорим
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT)  album_next(true);
  else if (d == LV_DIR_RIGHT) album_next(false);
}

// ============================================================
//  LVGL служебное — ТОЧНО как в их 06
// ============================================================
#if LV_USE_LOG != 0
void my_print(const char *buf) { Serial.printf(buf); Serial.flush(); }
#endif

void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area) {
  if(area->x1 % 2 !=0)area->x1--;
  if(area->y1 % 2 !=0)area->y1--;
  if(area->x2 %2 ==0)area->x2++;
  if(area->y2 %2 ==0)area->y2++;
}
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
  lv_disp_flush_ready(disp);
}

#if DIAG
// [diag] Вызывается LVGL после КАЖДОГО рефреша: сколько мс он занял и сколько
// пикселей реально перерисовано. 217156px = весь экран 466x466 (100%).
// Это ответ на вопрос «дельта-перерисовка работает или инвалидируется всё?».
// ВНИМАНИЕ: этот принт на каждый кадр сам тормозит рендер — потому и DIAG.
static void my_refr_monitor_cb(lv_disp_drv_t *d, uint32_t time_ms, uint32_t px) {
  Serial.printf("[refr] %lums %lupx (%lu%%)\n",
                (unsigned long)time_ms, (unsigned long)px,
                (unsigned long)(px * 100UL / (466UL * 466UL)));
}
#endif
void example_increase_lvgl_tick(void *arg) {
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
static bool finger_down = false;

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
#if DIAG
  // [diag] разрыв между опросами тача: если растёт — LVGL/loop голодает
  static uint32_t lastTouchPoll = 0;
  uint32_t tnow = millis();
  if (lastTouchPoll && tnow - lastTouchPoll > diagMaxTouchGap)
    diagMaxTouchGap = tnow - lastTouchPoll;
  lastTouchPoll = tnow;
  uint32_t tr0 = millis();   // [diag] замер вызова: растёт при BLE — тупит CST92xx
#endif
  // Опрашиваем тач непрерывно — нужно чтобы засечь удержание.
  uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
#if DIAG
  uint32_t trDt = millis() - tr0;
  if (trDt > diagMaxTouchRead) diagMaxTouchRead = trDt;
#endif

  // Экран погашен: касание ТОЛЬКО будит (через флаг — из callback нельзя
  // трогать железо дисплея, ломает LVGL). В UI касание не пускаем.
#if SCREEN_DIMMING_ENABLED
  if (!screenOn) {
    // Касание погашенного экрана ИГНОРИРУЕТСЯ (не будит): разблокировка
    // только кнопками Power Key / BOOT — как просил Роман.
    data->state = LV_INDEV_STATE_REL;
    return;
  }
#endif

  if (touched > 0) {
    lastActivity = millis();      // любое касание сбрасывает таймер автогашения
    if (!finger_down) {
      finger_down = true;
      press_start = millis();
      long_press_handled = false;
    }
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x[0];
    data->point.y = y[0];

    // Долгое удержание = переключение QR (ТОЛЬКО в модуле Links)
    uint32_t now = millis();
    if (activeModule == MOD_LINKS &&
        !long_press_handled && press_start > 0 && (now - press_start) > 600) {
      if (qr_mode) hide_qr();
      else         show_qr();
      long_press_handled = true;
    }
  } else {
    if (finger_down) {
      finger_down = false;
      press_start = 0;
    }
    data->state = LV_INDEV_STATE_REL;
  }
}

// ============================================================
//  Плашка-toast уведомления (на верхнем слое, поверх карусели)
//  ЕДИНЫЙ СТАНДАРТ: фиксированная высота, 3 строки по одной:
//  ник (белый) · текст с троеточием (серый) · источник (синий).
//  Полный текст всегда доступен в ленте по long-press.
// ============================================================
lv_obj_t *toast_box = NULL;
lv_timer_t *toast_timer = NULL;
extern lv_obj_t *call_overlay;   // определён ниже (экран звонка)

static void toast_close(lv_timer_t *t) {
  if (toast_box) { lv_obj_del(toast_box); toast_box = NULL; }
  if (toast_timer) { lv_timer_del(toast_timer); toast_timer = NULL; }
}

// --- Смахивание плашки СВАЙПОМ ВВЕРХ (как на iPhone) ---
// Вверх — сознательно: влево/вправо заняты каруселью, а жест ловит сама
// плашка (палец на ней), так что конфликтов с экраном под ней нет.
static void _toast_slide_y_cb(void *obj, int32_t v) {
  lv_obj_set_style_translate_y((lv_obj_t *)obj, v, 0);
}
static void _toast_slide_done_cb(lv_anim_t *a) {
  if (toast_box) { lv_obj_del(toast_box); toast_box = NULL; }
}
static void toastGestureCb(lv_event_t *e) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d != LV_DIR_TOP) return;                    // реагируем только на свайп вверх
  lv_indev_wait_release(lv_indev_get_act());
  if (toast_timer) { lv_timer_del(toast_timer); toast_timer = NULL; }  // авто-таймер больше не нужен
  // Улетает вверх, потом удаляется
  animStart(toast_box, 0, -320, 180, lv_anim_path_ease_in,
            _toast_slide_y_cb, _toast_slide_done_cb);
}

// --- Обрезка текста с "...": общая механика для makeLine/makeMultiLine ---
// Границы UTF-8 символов собираются ОДИН раз, дальше БИНАРНЫЙ ПОИСК по числу
// символов (~7 замеров вместо ~60 линейных — каждый замер сам O(длины),
// линейный подбор давал O(n²) работы на каждую карточку ленты).
#define TRUNC_MAX_CHARS 160
static size_t utf8Bounds(const char* text, size_t maxBytes, uint16_t* ends, size_t maxChars) {
  size_t n = 0, i = 0, len = strlen(text);
  if (len > maxBytes) len = maxBytes;
  while (i < len && n < maxChars) {
    size_t next = i + 1;
    while (next < len && (text[next] & 0xC0) == 0x80) next++;   // байты-продолжения
    ends[n++] = (uint16_t)next;
    i = next;
  }
  return n;   // ends[k] = длина префикса из (k+1) символов в байтах
}

// Максимальное число символов, при котором fits(prefix+"...") истинно.
// fits монотонна по числу символов -> бинарный поиск корректен.
// Возвращает длину префикса в байтах (0 = не влезает ничего).
// Тип колбэка — «сырым» указателем в сигнатуре, БЕЗ typedef: препроцессор
// Arduino вставляет автопрототипы функций в начало файла, где typedef ещё
// не объявлен — с typedef компиляция падала («fits_fn has not been declared»).
static size_t truncBinSearch(const char* text, const uint16_t* ends, size_t nChars,
                             char* buf, bool (*fits)(const char*, lv_coord_t),
                             lv_coord_t limit) {
  size_t lo = 0, hi = nChars;          // lo — заведомо влезает, hi+1 — нет
  while (lo < hi) {
    size_t mid = (lo + hi + 1) / 2;    // пробуем mid символов
    size_t bytes = ends[mid - 1];
    memcpy(buf, text, bytes);
    buf[bytes] = '.'; buf[bytes+1] = '.'; buf[bytes+2] = '.'; buf[bytes+3] = '\0';
    if (fits(buf, limit)) lo = mid;
    else                  hi = mid - 1;
  }
  return lo ? ends[lo - 1] : 0;
}

static bool fitsWidth(const char* buf, lv_coord_t w) {
  lv_point_t sz;
  lv_txt_get_size(&sz, buf, APP_FONT, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return sz.x <= w;
}
// Для многострочной проверки ширина переноса передаётся через статик
// (колбэк без user-data; вызывается только из loop — безопасно)
static lv_coord_t multiFitWidth = 0;
static bool fitsHeight(const char* buf, lv_coord_t h) {
  lv_point_t sz;
  lv_txt_get_size(&sz, buf, APP_FONT, 0, 0, multiFitWidth, LV_TEXT_FLAG_NONE);
  return sz.y <= h;
}

// Одна строка: начало текста + "..." если не влезает (как на iPhone).
// LONG_DOT в LVGL капризен к порядку вызовов и высоте — режем текст САМИ
// по реальной ширине глифов, это работает всегда и с любым шрифтом (и кириллицей).
static lv_obj_t* makeLine(lv_obj_t* parent, const char* text, uint32_t color, lv_coord_t width) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_font(l, APP_FONT, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);   // переносов нет, лишнее не рисуем
  lv_obj_set_width(l, width);

  // Влезает целиком? — ставим как есть
  if (fitsWidth(text, width)) {
    lv_label_set_text(l, text);
    return l;
  }

  // Не влезает — бинарным поиском ищем, сколько символов помещается с "..."
  // (замеряется сразу префикс + "..." против полной ширины)
  static char buf[TRUNC_MAX_CHARS + 4];    // текст максимум 128 + запас + "..."
  static uint16_t ends[TRUNC_MAX_CHARS];
  size_t nChars = utf8Bounds(text, sizeof(buf) - 4, ends, TRUNC_MAX_CHARS);
  size_t cut = truncBinSearch(text, ends, nChars, buf, fitsWidth, width);
  memcpy(buf, text, cut);
  buf[cut] = '.'; buf[cut+1] = '.'; buf[cut+2] = '.'; buf[cut+3] = '\0';
  lv_label_set_text(l, buf);
  return l;
}

// Многострочный текст с капом строк (для РАСКРЫТОЙ карточки):
// переносы как обычно, но если весь текст не влезает в max_lines строк —
// бинарным поиском находим максимум символов, влезающих вместе с "...".
static lv_obj_t* makeMultiLine(lv_obj_t* parent, const char* text, uint32_t color,
                               lv_coord_t width, uint8_t max_lines) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_font(l, APP_FONT, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, width);

  lv_coord_t max_h = lv_font_get_line_height(APP_FONT) * max_lines;

  // Влезает целиком?
  multiFitWidth = width;
  if (fitsHeight(text, max_h)) {
    lv_label_set_text(l, text);
    return l;
  }

  static char buf[TRUNC_MAX_CHARS + 4];
  static uint16_t ends[TRUNC_MAX_CHARS];
  size_t nChars = utf8Bounds(text, sizeof(buf) - 4, ends, TRUNC_MAX_CHARS);
  size_t cut = truncBinSearch(text, ends, nChars, buf, fitsHeight, max_h);
  memcpy(buf, text, cut);
  buf[cut] = '.'; buf[cut+1] = '.'; buf[cut+2] = '.'; buf[cut+3] = '\0';
  lv_label_set_text(l, buf);
  return l;
}

static void showToast(const char* title, const char* text) {
  // Во время звонка плашки НЕ показываем (перекрывали бы кнопки принять/отклонить);
  // уведомление всё равно сохраняется в ленту.
  if (call_overlay) return;

  // Убираем предыдущую, если ещё висит
  if (toast_box) { lv_obj_del(toast_box); toast_box = NULL; }
  if (toast_timer) { lv_timer_del(toast_timer); toast_timer = NULL; }

  // Единая высота: 2 строки (ник + текст) + отступ + паддинги
  lv_coord_t lh = lv_font_get_line_height(APP_FONT);
  const lv_coord_t PAD = 14, GAP = 6;
  lv_coord_t toast_h = PAD * 2 + lh * 2 + GAP;

  // iOS-баннер в стиле "матовое стекло": полупрозрачный фон + световой кант.
  toast_box = mkPanel(lv_layer_top(), 340, toast_h, 0x1c1c1e, 22);
  lv_obj_align(toast_box, LV_ALIGN_CENTER, 0, 40);
  lv_obj_set_style_bg_opa(toast_box, 230, 0);
  lv_obj_set_style_border_width(toast_box, 1, 0);          // кант поверх mkPanel
  lv_obj_set_style_border_color(toast_box, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(toast_box, 50, 0);
  lv_obj_set_style_pad_all(toast_box, PAD, 0);
  lv_obj_set_style_pad_row(toast_box, GAP, 0);
  lv_obj_set_style_shadow_width(toast_box, 24, 0);
  lv_obj_set_style_shadow_opa(toast_box, 100, 0);
  lv_obj_set_style_shadow_color(toast_box, lv_color_black(), 0);
  lv_obj_add_flag(toast_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(toast_box, LV_OBJ_FLAG_GESTURE_BUBBLE);   // жест ловит сама плашка
  lv_obj_add_event_cb(toast_box, toastGestureCb, LV_EVENT_GESTURE, NULL);
  lv_obj_set_flex_flow(toast_box, LV_FLEX_FLOW_COLUMN);      // строки стопкой, сами встают

  makeLine(toast_box, title, 0xffffff, 308);                              // ник
  makeLine(toast_box, text,  0xbbbbbb, 308);                              // текст (1 строка + …)

  // Авто-закрытие через 6 сек
  toast_timer = lv_timer_create(toast_close, 6000, NULL);
  lv_timer_set_repeat_count(toast_timer, 1);
}

// ============================================================
//  Полноэкранный экран ЗВОНКА (чёрный overlay на весь дисплей)
//  Как Apple Watch: имя сверху, 2 круглые кнопки внизу.
// ============================================================
// ============================================================
//  Локскрин — оверлей с часами (стиль экрана блокировки).
//  Как экран звонка, живёт на lv_layer_top() поверх любого модуля.
//  BOOT открывает/закрывает. Часы из RTC (идут автономно, без интернета).
//  Место под погоду зарезервировано — оживёт с WiFi или iOS-приложением.
// ============================================================
// --- RTC (PCF85063 на общей шине Wire) — часы для локскрина ---
SensorPCF85063 rtc;
bool rtcOnline = false;   // не static: используется в forward-объявлении выше (CTS-синк)

lv_obj_t *lock_overlay = NULL;
static lv_obj_t *lock_time = NULL;
static lv_obj_t *lock_date = NULL;
static lv_obj_t *lock_weather = NULL;
volatile bool lock_toggle_pending = false;   // BOOT -> loop откроет/закроет

#if LV_FONT_MONTSERRAT_48
  #define LOCK_TIME_FONT &lv_font_montserrat_48
#else
  #define LOCK_TIME_FONT &lv_font_montserrat_28
#endif

// Обновить время на локскрине из RTC (вызывается раз в секунду, только если открыт)
static void updateLockTime() {
  if (!lock_overlay || !rtcOnline) return;
  RTC_DateTime dt = rtc.getDateTime();
  lv_label_set_text_fmt(lock_time, "%02d:%02d", dt.getHour(), dt.getMinute());
  static const char* wd[] = {"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};
  static const char* mon[] = {"","янв","фев","мар","апр","мая","июн",
                              "июл","авг","сен","окт","ноя","дек"};
  int m = dt.getMonth();
  if (m < 1 || m > 12) m = 0;
  lv_label_set_text_fmt(lock_date, "%d %s", dt.getDay(), mon[m]);
}

static void showLock() {
  if (lock_overlay) return;   // уже открыт
  // Чёрный полноэкранный overlay на верхнем слое
  lock_overlay = mkPanel(lv_layer_top(), LCD_WIDTH, LCD_HEIGHT, 0x000000, 0);
  lv_obj_align(lock_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(lock_overlay, LV_OPA_COVER, 0);

  // Крупное время / дата / место под погоду (оживёт с WiFi или iOS-приложением)
  lock_time    = mkLabel(lock_overlay, "--:--", LOCK_TIME_FONT, 0xffffff, LV_ALIGN_CENTER, 0, -40);
  lock_date    = mkLabel(lock_overlay, "",      APP_FONT,       0xaaaaaa, LV_ALIGN_CENTER, 0,  20);
  lock_weather = mkLabel(lock_overlay, "--°",   APP_FONT,       0x555555, LV_ALIGN_CENTER, 0,  70);

  updateLockTime();
}

static void hideLock() {
  if (!lock_overlay) return;
  lv_obj_del(lock_overlay);
  lock_overlay = NULL;
  lock_time = lock_date = lock_weather = NULL;
}

lv_obj_t *call_overlay = NULL;
lv_obj_t *call_name_label = NULL;
volatile bool call_accept_pending = false;   // тап "принять" -> loop отправит
volatile bool call_reject_pending = false;   // тап "отклонить" -> loop отправит

static void call_accept_cb(lv_event_t *e) { call_accept_pending = true; }
static void call_reject_cb(lv_event_t *e) { call_reject_pending = true; }

static void showCall() {
  screenWake();               // входящий звонок будит экран
  // call_name пишет dataSourceCb (ядро 0) под feedMux — читаем тоже под ним,
  // в локальную копию, чтобы не держать лок во время LVGL-вызовов
  char nameNow[sizeof(call_name)];
  taskENTER_CRITICAL(&feedMux);
  memcpy(nameNow, call_name, sizeof(nameNow));
  taskEXIT_CRITICAL(&feedMux);
  const char* shown = nameNow[0] ? nameNow : "Incoming call";
  // Если overlay уже есть — только обновляем имя (детали могли прийти позже)
  if (call_overlay) {
    if (call_name_label) lv_label_set_text(call_name_label, shown);
    return;
  }

  // Чёрный overlay на ВЕСЬ экран (на верхнем слое, поверх всего)
  call_overlay = mkPanel(lv_layer_top(), LCD_WIDTH, LCD_HEIGHT, 0x000000, 0);
  lv_obj_align(call_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(call_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(call_overlay, 0, 0);

  // Подпись "Incoming call" сверху (маленькая, серая)
  mkLabel(call_overlay, "Incoming call", APP_FONT, 0x888888, LV_ALIGN_CENTER, 0, -120);

  // Имя/номер звонящего — крупно по центру.
  // НЕ через mkLabel: LONG_DOT капризен к порядку (режим и ширина ДО текста).
  call_name_label = lv_label_create(call_overlay);
  lv_obj_set_style_text_color(call_name_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(call_name_label, APP_FONT, 0);
  lv_label_set_long_mode(call_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(call_name_label, 380);
  lv_obj_set_style_text_align(call_name_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(call_name_label, shown);
  lv_obj_align(call_name_label, LV_ALIGN_CENTER, 0, -40);

  // Кнопки: ОТКЛОНИТЬ (красная, слева) и ПРИНЯТЬ (зелёная, справа)
  mkRoundBtn(call_overlay, 110, 0xff3b30, LV_SYMBOL_CLOSE, -90, 90, call_reject_cb);
  mkRoundBtn(call_overlay, 110, 0x34c759, LV_SYMBOL_OK,     90, 90, call_accept_cb);
}

static void hideCall() {
  if (call_overlay) { lv_obj_del(call_overlay); call_overlay = NULL; call_name_label = NULL; }
  call_uid = 0;
}

// ============================================================
//  BLE / ANCS инициализация
// ============================================================
void initBLE() {
  NimBLEDevice::init("SmartTag Brelok");
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setPower(9);

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);
  pServer->advertiseOnDisconnect(true);
  gServer = pServer;                       // для watchdog и conn params

  NimBLEAdvertising* pAdv = pServer->getAdvertising();
  gAdv = pAdv;
  NimBLEAdvertisementData advData{};
  advData.setFlags(0x01);
  uint8_t sol[18];
  sol[0] = 0x11; sol[1] = 0x15;
  memcpy(&sol[2], ancsServiceUUID.getValue(), 16);
  advData.addData(sol, 18);
  pAdv->setAdvertisementData(advData);

  NimBLEAdvertisementData scanResp{};
  scanResp.setName("SmartTag Brelok");
  pAdv->enableScanResponse(true);
  pAdv->setScanResponseData(scanResp);
  pAdv->start();

  Serial.println("BLE/ANCS advertising started");
}

// ============================================================
//  UI
// ============================================================
// ============================================================
//  МОДУЛЬ Links: карусель профилей (бывший buildUI, логика не менялась)
// ============================================================
static void goHomeBtnCb(lv_event_t *e) { goHome(); }

// Маленькая кнопка "домой" (общая для всех модулей). Позиция настраивается:
// в ленте она сдвигается влево, чтобы рядом встала кнопка Clear.
static void addHomeButtonAt(lv_obj_t* scr, lv_coord_t x_ofs, lv_coord_t y_ofs) {
  lv_obj_t *btn = mkPillBtn(scr, 64, 40, 0x2c2c2e, LV_SYMBOL_HOME, goHomeBtnCb);
  lv_obj_set_style_bg_opa(btn, 180, 0);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, x_ofs, y_ofs);
}
static void addHomeButton(lv_obj_t* scr) { addHomeButtonAt(scr, 0, 14); }

static void buildCarousel(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_add_event_cb(scr, screen_gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Стартовая картинка
  album_img = album_create(0);

  // Подпись (текст поставит updateLabelDots ниже)
  name_label = mkLabel(scr, "", APP_FONT, 0xffffff, LV_ALIGN_CENTER, 0, 120);

  // Точки
  int dotSpacing = 26;
  int startX = -(NUM_PROFILES - 1) * dotSpacing / 2;
  for (uint8_t i = 0; i < NUM_PROFILES; i++) {
    lv_obj_t *d = mkPanel(scr, 10, 10, 0x555555, LV_RADIUS_CIRCLE);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, startX + i * dotSpacing, -40);
    dots[i] = d;
  }

  addHomeButton(scr);
  updateLabelDots(0);

  // QR: белая рамка + canvas, создаются ОДИН раз и скрыты. Матрицы всех
  // профилей предсозданы (createQRs). Показ = переключение видимости.
  qr_obj = mkPanel(scr, 260, 260, 0xffffff, 24);
  lv_obj_align(qr_obj, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_pad_all(qr_obj, 8, 0);
  lv_obj_clear_flag(qr_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(qr_obj, LV_OBJ_FLAG_HIDDEN);

  // Подпись под QR (шрифт NULL = стандартный из темы, как было)
  qr_label = mkLabel(scr, "Scan to open", NULL, 0xAAAAAA, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_add_flag(qr_label, LV_OBJ_FLAG_HIDDEN);

  createQRs(qr_obj);   // предсоздать QR всех профилей (кодируются один раз, на splash)
}

// ============================================================
//  МОДУЛЬ Home: карусель по дуге (дизайн 1b)
//  Активный модуль крупно в центре, все модули мини-иконками
//  по нижней дуге. Свайп = листать, тап по центру = открыть.
//  Новый модуль в modules[] автоматически появляется на дуге.
// ============================================================
#include <math.h>

#define HOME_ACCENT   0x35d0d6              // акцент (циан)
#define HOME_ITEMS    (MOD_COUNT - 1)       // все модули кроме Home
#define HOME_SLOT_MAX 8

static lv_obj_t *notif_badge_label = NULL;  // бейдж на слоте Notifications
static lv_obj_t *home_bt_icon = NULL;       // индикатор связи (как раньше)
static lv_obj_t *home_batt_body = NULL;     // корпус батарейки (капсула, стиль iPhone)
static lv_obj_t *home_batt_fill = NULL;     // заливка уровня внутри капсулы
static lv_obj_t *home_batt_pct = NULL;      // процент цифрами рядом с батарейкой

static uint8_t   homeSel = 0;               // выбранный (0..HOME_ITEMS-1)
static lv_obj_t *home_big_circle = NULL;
static lv_obj_t *home_big_icon = NULL;
static lv_obj_t *home_name = NULL;
static lv_obj_t *home_counter = NULL;
static lv_obj_t *home_slots[HOME_SLOT_MAX];
static lv_obj_t *home_slot_icons[HOME_SLOT_MAX];

#if LV_FONT_MONTSERRAT_48
  #define HOME_BIG_FONT &lv_font_montserrat_48
#else
  #define HOME_BIG_FONT &lv_font_montserrat_28
#endif

// Символ модуля. LV_SYMBOL_* живут в montserrat-шрифтах (в font_ru_28 их нет!)
static const char* moduleSymbol(uint8_t id) {
  switch (id) {
    case MOD_LINKS:         return LV_SYMBOL_UPLOAD;    // шаринг профилей
    case MOD_NOTIFICATIONS: return LV_SYMBOL_BELL;
    case MOD_CARDS:         return LV_SYMBOL_SD_CARD;   // похоже на карту
    default:                return LV_SYMBOL_SETTINGS;  // будущие модули
  }
}

// Перекрасить UI под текущий выбор (без пересоздания объектов — быстро)
// Стиль одного слота дуги (вкл/выкл) — вынесено, чтобы при свайпе трогать
// ТОЛЬКО два слота (старый и новый), а не перекрашивать все: каждый
// set_style инвалидирует объект, и полный проход давал лишнюю перерисовку
// всей дуги — свайпы «подвисали», особенно если совпадало с тостом.
static void homeSlotStyle(uint8_t k, bool on) {
  if (k >= HOME_ITEMS || k >= HOME_SLOT_MAX) return;
  lv_obj_set_style_border_color(home_slots[k], lv_color_hex(on ? HOME_ACCENT : 0x2b2e2f), 0);
  lv_obj_set_style_border_width(home_slots[k], on ? 2 : 1, 0);
  lv_obj_set_style_bg_color(home_slots[k], lv_color_hex(on ? 0x0e2b2c : 0x161819), 0);
  lv_obj_set_style_text_color(home_slot_icons[k], lv_color_hex(on ? HOME_ACCENT : 0x8a8a8a), 0);
}

// Обновить центр (иконка+имя+счётчик) под текущий выбор
static void homeCenterApply() {
  uint8_t id = homeSel + 1;
  lv_label_set_text(home_big_icon, moduleSymbol(id));
  lv_label_set_text(home_name, modules[id].name);
  lv_label_set_text_fmt(home_counter, "%d / %d", homeSel + 1, HOME_ITEMS);
}

// Полная раскраска (инициализация экрана)
static void homeApply() {
  homeCenterApply();
  for (uint8_t k = 0; k < HOME_ITEMS && k < HOME_SLOT_MAX; k++)
    homeSlotStyle(k, k == homeSel);
}

static void homeNext(int8_t dir) {
  uint8_t prev = homeSel;
  homeSel = (uint8_t)((homeSel + dir + HOME_ITEMS) % HOME_ITEMS);
  // ФИНАЛЬНЫЙ ФИКС (по итогам бисекции): дельта — стилизуем только ДВА слота
  // (старый и новый) вместо полного прохода, а чтобы LVGL не рендерил их
  // отдельными разрозненными областями (обход дерева + flush на каждую),
  // явно инвалидируем ОДИН объемлющий прямоугольник — перекрывающиеся
  // области LVGL склеивает в одну.
  homeSlotStyle(prev, false);
  homeSlotStyle(homeSel, true);
  homeCenterApply();
  if (prev != homeSel && home_slots[prev] && home_slots[homeSel]) {
    lv_area_t a, b;
    lv_obj_get_coords(home_slots[prev], &a);
    lv_obj_get_coords(home_slots[homeSel], &b);
    lv_area_t u;
    u.x1 = LV_MIN(a.x1, b.x1); u.y1 = LV_MIN(a.y1, b.y1);
    u.x2 = LV_MAX(a.x2, b.x2); u.y2 = LV_MAX(a.y2, b.y2);
    lv_obj_invalidate_area(lv_obj_get_screen(home_slots[prev]), &u);
  }
}

static void homeGestureCb(lv_event_t *e) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT)       { lv_indev_wait_release(lv_indev_get_act()); homeNext(1);  }
  else if (d == LV_DIR_RIGHT) { lv_indev_wait_release(lv_indev_get_act()); homeNext(-1); }
}

// Открыть выбранный модуль. Вешается на LONG_PRESSED (нажать и чуть
// подержать, ~250мс), НЕ на клик: короткий свайп LVGL иногда не распознаёт
// как жест, оставался чистый тап и модуль открывался случайно при листании.
// Долгое нажатие в момент свайпа физически невозможно — конфликт исчез.
static void homeOpenCb(lv_event_t *e) {
  // От центрального кольца user_data == NULL — открываем текущий выбор.
  // От слота приходит его индекс — сначала выбираем его, затем открываем.
  void* ud = lv_event_get_user_data(e);
  if (ud != NULL) {
    uint8_t k = (uint8_t)(uintptr_t)ud;
    if (k < HOME_ITEMS) { homeSel = k; homeApply(); }
  }
  switchToModule(homeSel + 1);
}

// Тап по мини-иконке = только ВЫБРАТЬ (перелистнуть на неё). Открытие —
// долгим нажатием (по центру или по самой иконке), см. homeOpenCb.
static void homeSlotCb(lv_event_t *e) {
  uint8_t k = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (k >= HOME_ITEMS) return;
  homeSel = k;
  homeApply();
}

static void buildHome(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, homeGestureCb, LV_EVENT_GESTURE, NULL);

  // Статус-ряд по центру сверху (стиль iPhone): слева Bluetooth, справа батарейка.
  // Bluetooth — символ montserrat (в русском шрифте LV_SYMBOL нет).
  home_bt_icon = mkLabel(scr, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_28,
                         0x555555, LV_ALIGN_CENTER, -40, -182);

  // Батарейка: рисованная капсула + «носик» + заливка уровня внутри.
  home_batt_body = lv_obj_create(scr);
  lv_obj_set_size(home_batt_body, 40, 20);            // внешний корпус
  lv_obj_align(home_batt_body, LV_ALIGN_CENTER, 0, -182);
  lv_obj_set_style_radius(home_batt_body, 5, 0);
  lv_obj_set_style_bg_opa(home_batt_body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(home_batt_body, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(home_batt_body, 2, 0);
  lv_obj_set_style_pad_all(home_batt_body, 2, 0);     // зазор корпус-заливка
  lv_obj_clear_flag(home_batt_body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(home_batt_body, LV_OBJ_FLAG_CLICKABLE);

  // «Носик» батарейки справа
  lv_obj_t *batt_tip = mkPanel(scr, 3, 8, 0x888888, 1);
  lv_obj_align_to(batt_tip, home_batt_body, LV_ALIGN_OUT_RIGHT_MID, 1, 0);

  // Заливка уровня (ширина задаётся в updateHomeBattery), прижата влево
  home_batt_fill = mkPanel(home_batt_body, 0, LV_PCT(100), 0x34c759, 2);
  lv_obj_align(home_batt_fill, LV_ALIGN_LEFT_MID, 0, 0);

  // Процент цифрами справа от батарейки
  home_batt_pct = lv_label_create(scr);
  lv_obj_set_style_text_font(home_batt_pct, APP_FONT, 0);
  lv_obj_set_style_text_color(home_batt_pct, lv_color_hex(0xcccccc), 0);
  lv_obj_align_to(home_batt_pct, batt_tip, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
  lv_label_set_text(home_batt_pct, "");

  // Счётчик "1 / 3" (текст поставит homeApply)
  home_counter = mkLabel(scr, "", APP_FONT, 0x5f5f5f, LV_ALIGN_CENTER, 0, -150);
  lv_obj_set_width(home_counter, 200);
  lv_obj_set_style_text_align(home_counter, LV_TEXT_ALIGN_CENTER, 0);

  // Внешнее декоративное кольцо ВМЕСТО тени: тень 40px на круге 160px
  // выделяла ~40КБ из пула LVGL на КАЖДЫЙ рендер — после ленты уведомлений
  // (20 карточек забивают пул) аллокация падала, LV_ASSERT уходил в while(1)
  // = то самое «тихое зависание» при возврате домой. Кольцо стоит 0 байт.
  lv_obj_t *ring = lv_obj_create(scr);
  lv_obj_set_size(ring, 178, 178);
  lv_obj_align(ring, LV_ALIGN_CENTER, 0, -35);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ring, lv_color_hex(0x123a3c), 0);
  lv_obj_set_style_border_width(ring, 2, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

  // Центральное кольцо с иконкой активного модуля
  home_big_circle = lv_obj_create(scr);
  lv_obj_set_size(home_big_circle, 160, 160);
  lv_obj_align(home_big_circle, LV_ALIGN_CENTER, 0, -35);
  lv_obj_set_style_radius(home_big_circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(home_big_circle, lv_color_hex(0x0e2b2c), 0);
  lv_obj_set_style_border_color(home_big_circle, lv_color_hex(HOME_ACCENT), 0);
  lv_obj_set_style_border_width(home_big_circle, 2, 0);
  lv_obj_clear_flag(home_big_circle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(home_big_circle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(home_big_circle, homeOpenCb, LV_EVENT_LONG_PRESSED, NULL);

  // Иконка активного модуля в центре (текст поставит homeApply)
  home_big_icon = mkLabel(home_big_circle, "", HOME_BIG_FONT, HOME_ACCENT,
                          LV_ALIGN_CENTER, 0, 0);

  // Название модуля (русский шрифт; текст поставит homeApply)
  home_name = mkLabel(scr, "", APP_FONT, 0xffffff, LV_ALIGN_CENTER, 0, 72);
  lv_obj_set_width(home_name, 340);
  lv_obj_set_style_text_align(home_name, LV_TEXT_ALIGN_CENTER, 0);

  // Мини-иконки всех модулей по нижней дуге.
  // Позиции считаются от числа модулей — новые встают сами.
  const float R = 172.0f;        // радиус дуги от центра экрана
  const float STEP = 34.0f;      // градусов между слотами
  for (uint8_t k = 0; k < HOME_ITEMS && k < HOME_SLOT_MAX; k++) {
    float a = (90.0f + (k - (HOME_ITEMS - 1) / 2.0f) * STEP) * 3.14159f / 180.0f;
    lv_coord_t xo = (lv_coord_t)(cosf(a) * R);
    lv_coord_t yo = (lv_coord_t)(sinf(a) * R);

    // Цвета фона/рамки/иконки — временные: homeApply() в конце перекрасит
    lv_obj_t *slot = mkPanel(scr, 56, 56, 0x161819, LV_RADIUS_CIRCLE);
    lv_obj_align(slot, LV_ALIGN_CENTER, xo, yo);
    lv_obj_set_style_pad_all(slot, 0, 0);
    lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(slot, homeSlotCb, LV_EVENT_CLICKED, (void*)(uintptr_t)k);          // тап = выбрать
    lv_obj_add_event_cb(slot, homeOpenCb, LV_EVENT_LONG_PRESSED, (void*)(uintptr_t)k); // придержать = открыть
    home_slots[k] = slot;

    home_slot_icons[k] = mkLabel(slot, moduleSymbol(k + 1), &lv_font_montserrat_28,
                                 0x8a8a8a, LV_ALIGN_CENTER, 0, 0);

    // Бейдж уведомлений — красный кружок в углу слота Notifications
    if (k + 1 == MOD_NOTIFICATIONS) {
      notif_badge_label = mkLabel(slot, "", APP_FONT, 0xffffff, LV_ALIGN_TOP_RIGHT, 10, -10);
      lv_obj_set_style_bg_color(notif_badge_label, lv_color_hex(0xff3b30), 0);
      lv_obj_set_style_bg_opa(notif_badge_label, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(notif_badge_label, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_pad_hor(notif_badge_label, 8, 0);
      lv_obj_set_style_pad_ver(notif_badge_label, 2, 0);
      lv_obj_add_flag(notif_badge_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  homeApply();
}

// Обновить бейдж (скрываем при нуле, чтобы не висел пустой красный кружок)
static void updateHomeBadge() {
  if (!notif_badge_label) return;
  uint8_t n = 0;
  taskENTER_CRITICAL(&feedMux);
  for (uint8_t i = 0; i < feedCount; i++) if (feed[i].active) n++;
  taskEXIT_CRITICAL(&feedMux);
  if (n == 0) {
    lv_obj_add_flag(notif_badge_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text_fmt(notif_badge_label, "%d", n);
    lv_obj_clear_flag(notif_badge_label, LV_OBJ_FLAG_HIDDEN);
  }
}

// Перерисовать батарейку в стиле iPhone: капсула + заливка по уровню.
// Ширина заливки = процент от внутренней ширины корпуса. Молнии нет.
static void updateHomeBattery() {
  if (!home_batt_body || !home_batt_fill) return;
  if (battPercent < 0) {                       // нет батареи — прячем весь индикатор
    lv_obj_add_flag(home_batt_body, LV_OBJ_FLAG_HIDDEN);
    if (home_batt_pct) lv_label_set_text(home_batt_pct, "");
    return;
  }
  lv_obj_clear_flag(home_batt_body, LV_OBJ_FLAG_HIDDEN);
  if (home_batt_pct) lv_label_set_text_fmt(home_batt_pct, "%d%%", battPercent);

  const lv_coord_t inner = 34;                 // внутренняя ширина корпуса (см. buildHome)
  lv_coord_t w = (lv_coord_t)(inner * battPercent / 100);
  if (w < 3 && battPercent > 0) w = 3;         // чтобы «чуть-чуть» было видно
  lv_obj_set_width(home_batt_fill, w);

  // Цвет заливки: зелёный при зарядке или норме, жёлтый <30, красный <10 (стиль iOS)
  uint32_t col = (battCharging || battPercent >= 30) ? 0x34c759
               : (battPercent >= 10 ? 0xffd60a : 0xff3b30);
  lv_obj_set_style_bg_color(home_batt_fill, lv_color_hex(col), 0);
}

// При входе на Home освежаем индикаторы (иначе ждали бы следующего тика loop)
static void homeOnEnter() {
  updateHomeBattery();
  bleStatusDirty = true;   // loop перекрасит BT-иконку в этом же кадре
}


// ============================================================
//  МОДУЛЬ Cards: карты лояльности
//  Рисуем EAN-13 САМИ (lv_barcode есть только в LVGL 9, у нас 8.4).
//  Код — белая подложка + чёрные полоски-объекты, 95 модулей по стандарту.
//  Свайп влево/вправо листает карты, номер цифрами под кодом
//  (запасной вариант: кассир вбивает руками).
//  ДОБАВИТЬ КАРТУ: вписать имя + номер (ровно 13 цифр из приложения
//  магазина) в cards[] ниже. Пустые номера в карусели пропускаются.
// ============================================================
struct LoyaltyCard { const char* name; const char* number; uint32_t bg; };
static const LoyaltyCard cards[] = {
  { "Дикси",       "2661067227540", 0xE8641B },   // оранжевый
  { "Верный",      "1009471739241", 0xD6001C },   // красный
  { "Спортмастер", "3606647190350", 0x0D4BA0 },   // синий  ВНИМАНИЕ: контрольная
                                                   // цифра не сходится (должна быть 6) —
                                                   // проверь номер по пластику
  { "Пятёрочка",   "", 0x2AA036 },   // <- впиши 13 цифр из приложения
  { "Перекрёсток", "", 0x00543D },   // <-
  { "Магнит",      "", 0xC8102E },   // <-
  { "ОКей",        "", 0xE30613 },   // <-
};
static const uint8_t CARDS_TOTAL = sizeof(cards) / sizeof(cards[0]);
static int8_t cardIdx = -1;
static lv_obj_t *card_name_label = NULL;
static lv_obj_t *card_panel = NULL;       // белая подложка со штрихом
static lv_obj_t *card_num_label = NULL;

// Штрих-код рисуем в ОДИН canvas (а не 30 объектов-полосок): нет churn пула,
// нет роста фрагментации, показывается как единая картинка.
// ФОРМАТ INDEXED_1BIT: код чёрно-белый, true color хранил 111КБ — 1bpp с
// палитрой из 2 цветов хранит ~7КБ (16× меньше). Ширина 384 (кратна 8,
// чтобы байтовый stride был целым во всех конвенциях LVGL), код 380px
// (95 модулей × 4px) центрируется с отступом 2px. Рисуем биты напрямую в
// буфер (draw-функции canvas требуют TRUE_COLOR): собираем ОДНУ строку-
// шаблон из mods[] и memcpy её на остальные 145 строк.
#define CARD_CV_W      384
#define CARD_CV_H      146
#define CARD_CV_STRIDE (CARD_CV_W / 8)                    // 48 байт на строку
#define CARD_CV_PAL    (4 * 2)                            // палитра: 2 цвета lv_color32_t
#define CARD_CV_XOFF   2                                  // центрируем 380px в 384px
static lv_obj_t *card_canvas = NULL;
static uint8_t  *card_cv_buf = NULL;                      // PSRAM: палитра + битмап

// Залить весь код белым (индекс 0)
static void cardCanvasClear() {
  if (!card_cv_buf) return;
  memset(card_cv_buf + CARD_CV_PAL, 0x00, CARD_CV_STRIDE * CARD_CV_H);
  if (card_canvas) lv_obj_invalidate(card_canvas);
}

// Нарисовать код из mods[95]: строка-шаблон -> memcpy на все строки
static void cardCanvasDraw(const bool* mods) {
  if (!card_cv_buf) return;
  uint8_t *row0 = card_cv_buf + CARD_CV_PAL;
  memset(row0, 0x00, CARD_CV_STRIDE);
  const int SCALE = 4;                                    // модуль 4px -> код 380px
  for (int i = 0; i < 95; i++) {
    if (!mods[i]) continue;
    for (int px = 0; px < SCALE; px++) {
      int x = CARD_CV_XOFF + i * SCALE + px;
      row0[x >> 3] |= (uint8_t)(0x80 >> (x & 7));         // индекс 1 = чёрный
    }
  }
  for (int y = 1; y < CARD_CV_H; y++)
    memcpy(row0 + y * CARD_CV_STRIDE, row0, CARD_CV_STRIDE);
  if (card_canvas) lv_obj_invalidate(card_canvas);
}

// --- Таблицы EAN-13 (7-битные коды цифр, старший бит = первый модуль) ---
static const uint8_t EAN_L[10] = { 0x0D,0x19,0x13,0x3D,0x23,0x31,0x2F,0x3B,0x37,0x0B };
static const uint8_t EAN_G[10] = { 0x27,0x33,0x1B,0x21,0x1D,0x39,0x05,0x11,0x09,0x17 };
// Схема чётности левой половины по первой цифре (бит=1 -> G-код)
static const uint8_t EAN_PAR[10] = { 0x00,0x0B,0x0D,0x0E,0x13,0x19,0x1C,0x15,0x16,0x1A };

// 13 цифр -> 95 модулей (true = чёрный). false при кривом номере.
static bool ean13Modules(const char* num, bool* mods) {
  if (strlen(num) != 13) return false;
  uint8_t d[13];
  for (int i = 0; i < 13; i++) {
    if (num[i] < '0' || num[i] > '9') return false;
    d[i] = num[i] - '0';
  }
  // Контрольная цифра не сходится — всё равно рисуем (вдруг у сети нестандарт).
  // Принт убран: печатался на каждый рендер карты и засорял лог.

  int m = 0;
  mods[m++] = 1; mods[m++] = 0; mods[m++] = 1;              // стартовый страж
  uint8_t par = EAN_PAR[d[0]];                               // 1-я цифра кодируется чётностью
  for (int i = 1; i <= 6; i++) {
    uint8_t code = ((par >> (6 - i)) & 1) ? EAN_G[d[i]] : EAN_L[d[i]];
    for (int b = 6; b >= 0; b--) mods[m++] = (code >> b) & 1;
  }
  mods[m++] = 0; mods[m++] = 1; mods[m++] = 0; mods[m++] = 1; mods[m++] = 0;  // центр
  for (int i = 7; i <= 12; i++) {
    uint8_t code = (~EAN_L[d[i]]) & 0x7F;                    // R = инверсия L
    for (int b = 6; b >= 0; b--) mods[m++] = (code >> b) & 1;
  }
  mods[m++] = 1; mods[m++] = 0; mods[m++] = 1;              // конечный страж
  return true;
}

// Показать карту idx (перерисовать полоски). idx=-1 -> нет заданных карт.
static void cardsShow(int8_t idx) {
  cardIdx = idx;
  if (idx < 0) {
    lv_label_set_text(card_name_label, "Нет карт");
    lv_label_set_text(card_num_label, "впиши номера в cards[]");
    cardCanvasClear();
    return;
  }
  // Фирменный цвет магазина фоном — карту узнаёшь с одного взгляда,
  // а кассир видит «настоящую» карту, а не самоделку
  lv_obj_set_style_bg_color(modules[MOD_CARDS].screen, lv_color_hex(cards[idx].bg), 0);
  lv_label_set_text(card_name_label, cards[idx].name);
  lv_label_set_text(card_num_label, cards[idx].number);

  bool mods[95];
  if (!ean13Modules(cards[idx].number, mods)) {
    lv_label_set_text(card_num_label, "номер: нужно 13 цифр");
    cardCanvasClear();
    return;
  }
  // Штрих в canvas ОДИН РАЗ (битами напрямую, см. cardCanvasDraw). Никаких
  // объектов-полосок — пул не дробится, рендер экрана карт = блит картинки.
  cardCanvasDraw(mods);
}

// Следующая/предыдущая карта с непустым номером
static void cardsNext(int8_t dir) {
  if (cardIdx < 0) return;
  int8_t i = cardIdx;
  for (uint8_t n = 0; n < CARDS_TOTAL; n++) {
    i = (i + dir + CARDS_TOTAL) % CARDS_TOTAL;
    if (cards[i].number[0]) { cardsShow(i); return; }
  }
}

static void cardsGestureCb(lv_event_t *e) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT)       cardsNext(1);
  else if (d == LV_DIR_RIGHT) cardsNext(-1);
}

static void buildCards(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  addHomeButton(scr);

  // Название магазина
  card_name_label = mkLabel(scr, "", APP_FONT, 0xffffff, LV_ALIGN_TOP_MID, 0, 80);

  // Белая подложка под штрих: яркий код на глубоком чёрном AMOLED —
  // лучший случай для камерных сканеров касс
  card_panel = mkPanel(scr, 420, 170, 0xffffff, 16);
  lv_obj_align(card_panel, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_pad_all(card_panel, 0, 0);

  // Canvas штрих-кода поверх белой подложки. INDEXED_1BIT в PSRAM (~7КБ
  // вместо 111КБ true color) — одна картинка вместо 30 объектов,
  // рисуется один раз при смене карты.
  card_cv_buf = (uint8_t *)heap_caps_malloc(CARD_CV_PAL + CARD_CV_STRIDE * CARD_CV_H, MALLOC_CAP_SPIRAM);
  if (card_cv_buf) {
    card_canvas = lv_canvas_create(card_panel);
    lv_canvas_set_buffer(card_canvas, card_cv_buf, CARD_CV_W, CARD_CV_H, LV_IMG_CF_INDEXED_1BIT);
    lv_canvas_set_palette(card_canvas, 0, lv_color_white());
    lv_canvas_set_palette(card_canvas, 1, lv_color_black());
    lv_obj_center(card_canvas);
    cardCanvasClear();
  } else {
    Serial.println("card canvas: PSRAM alloc failed");
  }

  // Номер цифрами — запасной вариант для кассира (белый: фон теперь цветной)
  card_num_label = mkLabel(scr, "", APP_FONT, 0xffffff, LV_ALIGN_CENTER, 0, 130);

  // Свайпы влево/вправо листают карты (жест ловит экран модуля)
  lv_obj_add_event_cb(scr, cardsGestureCb, LV_EVENT_GESTURE, NULL);

  // Первая карта с заданным номером
  int8_t first = -1;
  for (uint8_t i = 0; i < CARDS_TOTAL; i++)
    if (cards[i].number[0]) { first = i; break; }
  cardsShow(first);
}

// ============================================================
//  МОДУЛЬ Notifications: лента как на iPhone
//  Вертикальный скролл с инерцией (LVGL из коробки). Карточки из feed[].
//  ЕДИНЫЙ РАЗМЕР карточек: 3 строки (ник · текст · источник синим).
//  Свайп ВЛЕВО по карточке = удалить (анимация «смахивания», как iPhone).
//  Long-press = раскрыть полный текст. Короткий тап по раскрытой = свернуть.
//  Кнопка Clear = очистить все.
//  ВАЖНО: перестроение списка ВСЕГДА через флаг notif_dirty из loop() —
//  lv_obj_clean внутри event-колбэка удалял бы карточку, по которой
//  ещё идёт обработка события (краш).
// ============================================================
static lv_obj_t *notif_list = NULL;      // скроллируемый контейнер карточек
static volatile bool notif_dirty = false;   // loop() перестроит ленту + бейдж
// Снапшот feed[] для перестроения ленты: копируется под КОРОТКИМ локом,
// дальше UI строится по копии — BLE-колбэки могут писать в feed параллельно
// без гонок. Лежит в PSRAM (alloc в setup), internal RAM не тратим.
static Notif* feedSnap = nullptr;

// --- Раскрытие карточки НА МЕСТЕ (без перестроения списка) ---
// Каждая раскрываемая карточка создаётся сразу с ДВУМЯ версиями текста
// (строка + полный), long-press лишь переключает видимость. Перестроение
// списка под зажатым пальцем запускало в LVGL новый цикл нажатия, и
// отпускание пальца засчитывалось тапом — карточка тут же сворачивалась
// («прыгает: открыл-закрыл»). Тут перестроения нет — нет и фантомного тапа.
// Дети карточки: [0]=ник, [1]=текст-строка, [2]=текст-полный (если раскрываема).
static void cardApplyExpanded(lv_obj_t* card, bool exp) {
  lv_obj_t* mCol = lv_obj_get_child(card, 1);
  lv_obj_t* mExp = lv_obj_get_child(card, 2);
  if (!mCol || !mExp) return;              // нераскрываемая (текст влезает целиком)
  lv_coord_t lh = lv_font_get_line_height(APP_FONT);
  const lv_coord_t PAD = 14, GAP = 6;
  if (exp) {
    lv_obj_add_flag(mCol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mExp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(card, LV_SIZE_CONTENT);          // ровно под реальный текст
  } else {
    lv_obj_add_flag(mExp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mCol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(card, PAD * 2 + lh * 2 + GAP);   // единый свёрнутый размер
  }
  lv_obj_set_style_bg_color(card, lv_color_hex(exp ? 0x2c2c2e : 0x1c1c1e), 0);
}

// --- Анимация «смахивания»: translate_x (обычный set_x flex-раскладка перебила бы) ---
static void _card_slide_x_cb(void *obj, int32_t v) {
  lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}
static void _card_slide_done_cb(lv_anim_t *a) {
  notif_dirty = true;    // карточка уехала — rebuild безопасно сделает loop()
}

// Единый обработчик событий карточки (жест/long-press/тап)
static void notifCardEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *card = (lv_obj_t *)lv_event_get_current_target(e);
  uint32_t uid = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

  if (code == LV_EVENT_GESTURE) {
    // Свайп ВЛЕВО = удалить, как смахивание на iPhone
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (d != LV_DIR_LEFT) return;
    lv_indev_wait_release(lv_indev_get_act());   // гасим CLICKED после свайпа
    taskENTER_CRITICAL(&feedMux);
    int slot = feedFind(uid);
    if (slot >= 0) feed[slot].active = false;    // данные — сразу
    taskEXIT_CRITICAL(&feedMux);
    // Визуально — уезжает за левый край, после чего rebuild сделает loop()
    animStart(card, 0, -(int32_t)LCD_WIDTH, 200, lv_anim_path_ease_in,
              _card_slide_x_cb, _card_slide_done_cb);
  }
  else if (code == LV_EVENT_LONG_PRESSED) {
    // Long-press = раскрыть. Если у карточки нет полной версии текста
    // (влезает в строку целиком) — раскрывать нечего, игнорируем.
    if (lv_obj_get_child_cnt(card) < 3) return;
    taskENTER_CRITICAL(&feedMux);
    int slot = feedFind(uid);
    bool doExpand = (slot >= 0 && !feed[slot].expanded);
    if (doExpand) feed[slot].expanded = true;
    taskEXIT_CRITICAL(&feedMux);
    if (doExpand) cardApplyExpanded(card, true);   // на месте, список не трогаем
  }
  else if (code == LV_EVENT_SHORT_CLICKED) {
    // Короткий тап по РАСКРЫТОЙ карточке = свернуть
    // (SHORT_CLICKED, не CLICKED: CLICKED прилетает и после long-press)
    taskENTER_CRITICAL(&feedMux);
    int slot = feedFind(uid);
    bool doCollapse = (slot >= 0 && feed[slot].expanded);
    if (doCollapse) feed[slot].expanded = false;
    taskEXIT_CRITICAL(&feedMux);
    if (doCollapse) cardApplyExpanded(card, false);
  }
}

static void notifClearCb(lv_event_t *e) {
  taskENTER_CRITICAL(&feedMux);
  for (uint8_t i = 0; i < feedCount; i++) { feed[i].active = false; feed[i].expanded = false; }
  taskEXIT_CRITICAL(&feedMux);
  notifOnEnter();        // тут безопасно: нажата кнопка Clear, а не карточка
  updateHomeBadge();
}

// Перестроить список карточек из feed[] (вызывается при входе и при изменениях)
static void notifOnEnter() {
  if (!notif_list || !feedSnap) return;
  // Запоминаем прокрутку: перестроение (раскрытие/удаление/новое уведомление)
  // не должно выбрасывать пользователя в начало списка.
  lv_coord_t scroll_y = lv_obj_get_scroll_y(notif_list);
  lv_obj_clean(notif_list);   // удалить старые карточки

  // Снимаем копию активных «новые сверху» под коротким локом.
  // СОРТИРОВКИ БОЛЬШЕ НЕТ: кольцо feed упорядочено по времени вставки,
  // обход от head назад и есть «новые сверху» (пузырёк O(n²) удалён).
  uint8_t n = 0;
  taskENTER_CRITICAL(&feedMux);
  for (uint8_t k = 0; k < feedCount; k++) {
    uint8_t i = feedNth(k);
    if (feed[i].active) feedSnap[n++] = feed[i];
  }
  taskEXIT_CRITICAL(&feedMux);

  if (n == 0) {
    lv_obj_t *empty = lv_label_create(notif_list);
    lv_label_set_text(empty, "No notifications");
    lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(empty, APP_FONT, 0);
    return;
  }

  // ЕДИНАЯ высота карточек: свёрнутая = 2 строки (ник + текст),
  // раскрытая = ник + до 4 строк текста (капаем, чтобы не вылезала за экран).
  lv_coord_t lh = lv_font_get_line_height(APP_FONT);
  const lv_coord_t PAD = 14, GAP = 6;
  lv_coord_t card_h = PAD * 2 + lh * 2 + GAP;                // единая свёрнутая высота

  for (int k = 0; k < n; k++) {
    const char* msg = feedSnap[k].message[0] ? feedSnap[k].message : "";

    // Раскрываема ли карточка: только если текст НЕ влезает в одну строку.
    // «Привет» виден целиком — раскрывать нечего, long-press будет игнорироваться.
    lv_point_t msz;
    lv_txt_get_size(&msz, msg, APP_FONT, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    bool expandable = msz.x > 330;
    if (!expandable && feedSnap[k].expanded) {   // гасим флаг и в оригинале (по uid)
      feedSnap[k].expanded = false;
      taskENTER_CRITICAL(&feedMux);
      int slot = feedFind(feedSnap[k].uid);
      if (slot >= 0) feed[slot].expanded = false;
      taskEXIT_CRITICAL(&feedMux);
    }
    bool expanded = feedSnap[k].expanded;

    lv_obj_t *card = mkPanel(notif_list, 360, expanded ? LV_SIZE_CONTENT : card_h,
                             expanded ? 0x2c2c2e : 0x1c1c1e, 20);
    lv_obj_set_style_pad_all(card, PAD, 0);
    lv_obj_set_style_pad_row(card, GAP, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);   // строки стопкой, без ручных align_to
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_GESTURE_BUBBLE);  // жест ловит САМА карточка, не экран
    lv_obj_add_event_cb(card, notifCardEventCb, LV_EVENT_ALL, (void*)(uintptr_t)feedSnap[k].uid);

    // 1) Ник — всегда 1 строка
    makeLine(card, feedSnap[k].title[0] ? feedSnap[k].title : "Notification", 0xffffff, 330);

    // 2) Текст: ОБЕ версии сразу (строка + полный до 4 строк), видима одна.
    //    Long-press переключает их на месте — без перестроения списка.
    lv_obj_t *mCol = makeLine(card, msg, 0xaaaaaa, 330);
    if (expandable) {
      lv_obj_t *mExp = makeMultiLine(card, msg, 0xdddddd, 330, 4);
      if (expanded) lv_obj_add_flag(mCol, LV_OBJ_FLAG_HIDDEN);
      else          lv_obj_add_flag(mExp, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Возвращаем прокрутку туда, где пользователь был (без анимации).
  // Сначала даём LVGL посчитать реальные размеры нового содержимого.
  lv_obj_update_layout(notif_list);
  lv_obj_scroll_to_y(notif_list, scroll_y, LV_ANIM_OFF);
}

static void buildNotifications(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Круглый экран срезает углы — обе кнопки ставим ПАРОЙ по центру сверху,
  // чуть ниже (y=30): домик слева от центра, Clear справа. Обе целиком видны.
  addHomeButtonAt(scr, -55, 30);

  // Кнопка Clear (рядом с домиком)
  lv_obj_t *clr = mkPillBtn(scr, 90, 40, 0x3a3a3c, "Clear", notifClearCb);
  lv_obj_align(clr, LV_ALIGN_TOP_MID, 45, 30);

  // Скроллируемая колонка карточек — инерция и отскок у LVGL из коробки
  notif_list = lv_obj_create(scr);
  lv_obj_set_size(notif_list, 400, 380);
  lv_obj_align(notif_list, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_opa(notif_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(notif_list, 0, 0);
  lv_obj_set_style_pad_row(notif_list, 12, 0);
  lv_obj_set_flex_flow(notif_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(notif_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(notif_list, LV_DIR_VER);
}

// ============================================================
//  SETUP — фундамент из 06 + наш UI вместо demo_music
// ============================================================
void setup() {
  Serial.begin(115200);

  // Лента уведомлений и её снапшот — в PSRAM (~10КБ суммарно): internal RAM
  // не тратим, задачам NimBLE (ядро 0) PSRAM доступна (это не ISR-контекст).
  // Fallback на internal, если PSRAM вдруг не поднялась — прошивка живёт.
  feed     = (Notif*)heap_caps_malloc(sizeof(Notif) * FEED_SIZE, MALLOC_CAP_SPIRAM);
  feedSnap = (Notif*)heap_caps_malloc(sizeof(Notif) * FEED_SIZE, MALLOC_CAP_SPIRAM);
  if (!feed)     feed     = (Notif*)malloc(sizeof(Notif) * FEED_SIZE);
  if (!feedSnap) feedSnap = (Notif*)malloc(sizeof(Notif) * FEED_SIZE);
  memset(feed, 0, sizeof(Notif) * FEED_SIZE);

  // Очередь плашек BLE->loop (атомарные копии, см. ToastMsg) — ДО initBLE
  toastQueue = xQueueCreate(8, sizeof(ToastMsg));

  Wire.begin(IIC_SDA, IIC_SCL);

  // Ресет тача. БАГ был: digitalWrite до pinMode — пин оставался входом и
  // импульс ресета фактически не выдавался (работало лишь потому, что
  // touch.begin делает свой ресет). Плюс delay(1000) — целая секунда
  // старта впустую: CST92xx готов за ~300мс.
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW);
  delay(30);
  digitalWrite(TP_RESET, HIGH);
  delay(300);

  touch.setPins(TP_RESET, TP_INT);
  bool result = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
  if (result == false) {
    Serial.println("touch is not online...");
    while (1) delay(1000);
  }
  Serial.print("Model :");
  Serial.println(touch.getModelName());
  // ВАЖНО (граблина из брифа): touch.sleep() и attachInterrupt МЕШАЮТ жестам
  // и вызывают подвисания тача. Работаем ТОЛЬКО непрерывным опросом в my_touchpad_read.
  touch.setMaxCoordinates(466, 466);
  touch.setMirrorXY(true, true);

  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) delay(1000);
  }
  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();

  // Батарея: AXP2101 на той же шине Wire (адрес свой, конфликта с тачем/IMU нет).
  // Порядок и набор ADC — ТОЧНО как в рабочей демке 05_LVGL_AXP2101_ADC_Data.
#if FEATURE_PMU
  pmuOnline = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (pmuOnline) {
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.enableTemperatureMeasure();
    pmu.enableBattDetection();          // определение наличия батареи
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();     // ← отсюда getBatteryPercent()
    pmu.enableSystemVoltageMeasure();
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ |
                  XPOWERS_AXP2101_PKEY_LONG_IRQ);   // Power Key: гашение + диагностика
    Serial.println("AXP2101 online");
  } else {
    Serial.println("AXP2101 not found - battery info disabled");
  }
#else
  Serial.println("[bisect] FEATURE_PMU=0: AXP2101 не инициализируем");
#endif

#if FEATURE_RTC
  // RTC PCF85063 (та же шина Wire) — часы для локскрина, идут автономно.
  rtcOnline = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (rtcOnline) {
    RTC_DateTime now = rtc.getDateTime();
    // Ставим ВРЕМЯ СБОРКИ только при ПЕРВОМ старте (RTC пустой).
    // Дальше точное время подводит CTS с iPhone (syncTimeFromCTS).
    // RTC_FORCE_SET_TIME больше не нужен — CTS перебьёт при подключении.
    if (now.getYear() < 2024) {
      const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
      char mon[4] = {0};
      int day = 1, year = 2026, hh = 0, mm = 0, ss = 0;
      sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
      const char* p = strstr(months, mon);
      int mi = p ? (int)((p - months) / 3) + 1 : 1;
      sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
      rtc.setDateTime(year, mi, day, hh, mm, ss);
      Serial.printf("RTC: set build time %04d-%02d-%02d %02d:%02d:%02d\n",
                    year, mi, day, hh, mm, ss);
    }
    Serial.println("RTC online");
  } else {
    Serial.println("RTC not found - clock disabled");
  }
#else
  Serial.println("[bisect] FEATURE_RTC=0: RTC не инициализируем");
#endif

  // NFC шина
  Wire1.begin(NFC_SDA, NFC_SCL);

  gfx->begin();
  gfx->setBrightness(SCREEN_BRIGHTNESS);
  lastActivity = millis();          // старт отсчёта автогашения
  screenWidth = gfx->width();
  screenHeight = gfx->height();

  lv_init();

  // ============================================================
  //  ПАМЯТЬ / РЕНДЕР LVGL
  //  ВАЖНЫЙ УРОК: буфер /8 (попытка сэкономить internal RAM) раздул render
  //  до ~190мс (меньший буфер = больше кусков flush + software-поворот
  //  доворачивает каждый кусок). Пока идёт render, тач НЕ опрашивается →
  //  «тач не срабатывает». Возврат на /4 — рендер снова ~70мс, тач живой.
  //  Internal RAM экономим НЕ на буферах (это бьёт по рендеру), а вынося
  //  пул виджетов в PSRAM через lv_conf.h (LV_MEM_CUSTOM) — это рендер НЕ
  //  трогает. Иконки/шрифты — const → уже во FLASH.
  //  LVGL_BUF_PSRAM 0 = буферы в internal DMA (быстро, дисплей точно ок).
  //
  //  ДВА буфера — ОБЯЗАТЕЛЬНО, урок зависания на ленте уведомлений:
  //  шина QSPI (esp_lcd) ставит передачу в DMA-очередь и может вернуть
  //  управление ДО её завершения. С одним буфером LVGL начинал перезаписывать
  //  буфер, пока DMA ещё читал из него — на самой тяжёлой перерисовке
  //  (лента, 20 карточек, полный экран) дисплей замирал намертво, выглядело
  //  как «тач не реагирует». buf2 не ускоряет рендер, но СТРАХУЕТ
  //  асинхронную передачу: LVGL рисует в один буфер, пока DMA гонит другой.
  // ============================================================
  #define LVGL_BUF_DIVISOR 4
  #define LVGL_BUF_PSRAM   0
  size_t buf_px = screenWidth * screenHeight / LVGL_BUF_DIVISOR;
#if LVGL_BUF_PSRAM
  uint32_t buf_caps = MALLOC_CAP_SPIRAM;
#else
  uint32_t buf_caps = MALLOC_CAP_DMA;
#endif
  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), buf_caps);
  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), buf_caps);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_px);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.sw_rotate = 0;   // поворот убран: экран фиксирован, рендер не платит
                            // за попиксельную перекладку буфера каждый кадр
#if DIAG
  disp_drv.monitor_cb = my_refr_monitor_cb;   // [diag] px/время каждого рефреша
#endif
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  // Пороги жеста — ДЕФОЛТНЫЕ (50px / 3). Подкрутки 30/1 были под неверную
  // гипотезу «жест не добирает порог из-за редкой выборки». Бисекция
  // (SCREEN_DIMMING_ENABLED 0 = листается идеально) доказала: виноват был
  // PMU-опрос на шине тача во время жеста. Старая прошивка отлично живёт
  // на дефолтных порогах — лишних подкруток не держим.
  lv_indev_drv_register(&indev_drv);

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick, .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  // Кнопка BOOT (GPIO0) — локскрин с часами (в рантайме читается безопасно)
  pinMode(0, INPUT_PULLUP);

  // --- Splash-экран загрузки: показываем, пока грузятся модули и QR ---
  lv_obj_t *splash = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(splash, lv_color_black(), 0);
  lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *sp_title = lv_label_create(splash);
  lv_label_set_text(sp_title, "SmartTag");
  lv_obj_set_style_text_font(sp_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(sp_title, lv_color_white(), 0);
  lv_obj_align(sp_title, LV_ALIGN_CENTER, 0, -20);
  lv_obj_t *sp_spin = lv_spinner_create(splash, 1000, 60);
  lv_obj_set_size(sp_spin, 50, 50);
  lv_obj_align(sp_spin, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_arc_width(sp_spin, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sp_spin, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sp_spin, lv_color_hex(HOME_ACCENT), LV_PART_INDICATOR);
  lv_scr_load(splash);
  for (int k = 0; k < 5; k++) { lv_timer_handler(); delay(15); }  // отрисовать заставку

  // Создаём ВСЕ экраны модулей один раз (мгновенное переключение потом).
  // Тут же createQRs кодирует QR всех профилей — самая долгая часть загрузки.
  for (uint8_t i = 0; i < MOD_COUNT; i++) {
    modules[i].screen = lv_obj_create(NULL);
    modules[i].build(modules[i].screen);
    lv_timer_handler();   // крутим спиннер между модулями
  }
  lv_scr_load(modules[MOD_HOME].screen);   // стартуем с Home
  activeModule = MOD_HOME;
  lv_obj_del(splash);                       // заставку убираем

  NFC_WriteURL(profiles[0].url, URI_PREFIX_HTTPS);

  // BLE/ANCS — приём уведомлений в фоне
  initBLE();

  // Задача асинхронных записей в ANCS Control Point (ядро 0, рядом с BLE-хостом;
  // loop/LVGL живут на ядре 1 и больше никогда не блокируются GATT-записью)
  cpQueue = xQueueCreate(8, sizeof(CpMsg));
  // Стек 8К: цепочка NimBLE-клиента (getService/writeValue) глубокая,
  // 4К грозит переполнением стека = «тихое зависание» без паники
  xTaskCreatePinnedToCore(ancsWriteTask, "ancsWr", 8192, NULL, 1, NULL, 0);
  // Подписка на ANCS — тоже блокирующая (discovery до 30с на плохом линке),
  // поэтому тоже в задаче на ядре 0, НЕ в loop
  xTaskCreatePinnedToCore(ancsSubscribeTask, "ancsSub", 8192, NULL, 1, NULL, 0);

  Serial.printf("Free internal RAM: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("Free PSRAM: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.printf("Largest free internal block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.println("Setup done");
}

void loop() {
#if DIAG
  // [diag] худшая итерация loop: если растёт при подключённом BLE — цикл
  // тормозит и тач голодает; если мала, а тач плохой — врёт сам контроллер
  static uint32_t loopPrevStart = 0;
  uint32_t loopNow = millis();
  if (loopPrevStart && loopNow - loopPrevStart > diagMaxLoopMs)
    diagMaxLoopMs = loopNow - loopPrevStart;
  loopPrevStart = loopNow;
#endif

  // Авто-поворот по IMU УБРАН (Роман попросил фикс. ориентацию). Экран
  // теперь всегда в одной ориентации, sw_rotate=0 → рендер не крутит буфер.
  // Чип IMU остаётся инициализированным в setup — под будущий шагомер.

  // --- Кнопка BOOT: короткое нажатие = локскрин (часы) открыть/закрыть ---
  static bool bootWasPressed = false;
  static uint32_t bootPressTime = 0;
  bool bootNow = (digitalRead(0) == LOW);
  if (bootNow && !bootWasPressed) { bootWasPressed = true; bootPressTime = millis(); }
  if (!bootNow && bootWasPressed) {
    bootWasPressed = false;
    uint32_t held = millis() - bootPressTime;
    if (held > 30 && held < 800) {   // дебаунс + только короткое нажатие
#if SCREEN_DIMMING_ENABLED
      if (!screenOn) { wakeRequested = true; }   // погашен — сначала будим (в loop)
      else if (lock_overlay) hideLock();
      else showLock();
#else
      if (lock_overlay) hideLock();
      else showLock();
#endif
    }
  }

  // --- Локскрин: тикаем время раз в секунду, пока открыт ---
  if (lock_overlay) {
    static uint32_t lastLockTick = 0;
    if (millis() - lastLockTick > 1000) {
      lastLockTick = millis();
      updateLockTime();
    }
  }

  // --- Отложенная запись NFC (после анимации, когда UI свободен) ---
  if (nfc_write_pending && !animating) {
    nfc_write_pending = false;
    NFC_WriteURL(profiles[current].url, URI_PREFIX_HTTPS);
  }

  // --- ANCS: подписка выполняется в ancsSubscribeTask (ядро 0) ---
  // Здесь её больше НЕТ: getService/subscribe блокирующие, в loop им не место.

  // --- CTS (BLE-часы): выполняется в ancsSubscribeTask при FEATURE_CTS.
  // В loop его НЕТ: getService блокирующий, да и всё BLE — вне главного цикла.

  // --- ANCS: очередь запросов деталей (один в полёте) ---
  // iOS отвечает на Get Notification Attributes ПО ОДНОМУ; если сыпать запросы
  // пачкой, ответы теряются. Шлём следующий только когда предыдущий отвечен
  // (dataSourceCb обнуляет detailsInFlightUid) или протух по таймауту 600мс.
  if (ancsSubscribed &&
      (detailsInFlightUid == 0 || millis() - detailsSentAt > 600)) {
    detailsInFlightUid = 0;
    // Кандидата выбираем под локом (feed[] пишут колбэки ядра 0),
    // сам запрос шлём уже снаружи — в критической секции только память.
    uint32_t uid = 0;
    bool haveCandidate = false;
    taskENTER_CRITICAL(&feedMux);
    for (uint8_t i = 0; i < feedCount; i++) {
      if (!feed[i].active || !feed[i].wantDetails) continue;
      if (feed[i].tries >= 3) { feed[i].wantDetails = false; continue; }  // хватит
      feed[i].tries++;
      uid = feed[i].uid;
      haveCandidate = true;
      break;                    // один запрос за раз
    }
    taskEXIT_CRITICAL(&feedMux);
    if (haveCandidate) {
      // Запрос: AppID(0), Title(1,max32), Message(3,max128)
      uint8_t val[12] = {
        0x0,
        (uint8_t)(uid), (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24),
        0x0,                    // AttrID 0 = AppIdentifier
        0x1, 0x20, 0x0,         // AttrID 1 = Title, max 32
        0x3, 0x80, 0x0          // AttrID 3 = Message, max 128
      };
      if (ancsWriteCPAsync(val, 12)) {
        detailsInFlightUid = uid;
        detailsSentAt = millis();
      }
    }
  }

  // --- ANCS: новые уведомления (атомарные копии из очереди, гонок нет) ---
  {
    ToastMsg tm;
    bool gotAny = false;
    while (toastQueue && xQueueReceive(toastQueue, &tm, 0) == pdTRUE) {
      gotAny = true;
      // Первые 3 сек после подписки iPhone присылает ВЕСЬ Центр уведомлений
      // (зеркалирование). Эту пачку кладём в ленту тихо — без фейерверка плашек.
      bool initialBurst = (millis() - ancsSubscribedAt < 3000);
      // Плашку показываем ТОЛЬКО если пользователь НЕ в ленте.
      // В ленте уведомление и так появится в списке — плашка поверх не нужна.
      if (activeModule != MOD_NOTIFICATIONS && !initialBurst)
        showToast(tm.title, tm.text);
      // bundle ID продолжаем собирать для таблицы appNames (принт теперь
      // из loop — Serial с двух ядер вперемешку больше не путается)
      if (tm.app[0]) Serial.printf("ANCS app: %s\n", tm.app);
    }
    if (gotAny) {
      if (activeModule == MOD_NOTIFICATIONS) notifOnEnter();  // лента открыта — обновить раз
      updateHomeBadge();
    }
  }

  // --- Периодические принты диагностики (раз в 5с, только при DIAG).
  // BLE-операций тут НЕТ: advertising-watchdog переехал в ancsSubscribeTask
  // (ядро 0), т.к. любой NimBLE-вызов из loop блокирует тач на мьютексе хоста.
#if DIAG
  static uint32_t lastBleCheck = 0;
  if (millis() - lastBleCheck > 5000) {
    lastBleCheck = millis();
#if LV_MEM_CUSTOM == 0
    // Монитор пула LVGL (только для встроенного пула; с PSRAM-аллокатором
    // память фактически безлимитна и монитор не нужен)
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("[mem] LVGL pool: used %d%%, frag %d%%, biggest free %u\n",
                  100 - mon.free_size * 100 / mon.total_size, mon.frag_pct,
                  (unsigned)mon.free_biggest_size);
#else
    Serial.printf("[mem] LVGL in PSRAM, free PSRAM: %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
    // [diag] худшие показатели за 5с: loop-итерация, разрыв опроса тача,
    // стоимость опроса питальника (PowerKey+батарея), стоимость IMU.
    // ble/chg — контекст: подключён ли телефон, идёт ли зарядка.
    Serial.printf("[diag] loop=%ums touchGap=%ums touchRead=%ums render=%ums pmu=%ums imu=%ums ble=%d chg=%d\n",
                  diagMaxLoopMs, diagMaxTouchGap, diagMaxTouchRead, diagMaxRender,
                  diagMaxPmuMs, diagMaxImuMs,
                  bleConnected ? 1 : 0, battCharging ? 1 : 0);
    diagMaxLoopMs = diagMaxTouchGap = diagMaxPmuMs = diagMaxImuMs = 0;
    diagMaxTouchRead = diagMaxRender = 0;
  }
#endif

  // --- Экономичные параметры соединения (после подписки, батарея) ---
  // 30-50мс интервал, latency 4, supervision 5с — в рамках гайдлайнов Apple.
  // Сравнение через вычитание: `millis() > connParamsAt` ломалось на
  // переполнении millis (49 дней аптайма) — брелок столько живёт легко.
  if (connParamsPending && (int32_t)(millis() - connParamsAt) >= 0) {
    connParamsPending = false;
    if (gServer && gServer->getConnectedCount() > 0) {
      gServer->updateConnParams(gConnHandle, 24, 40, 4, 500);
      Serial.println("Conn params: power-friendly requested");
    }
  }

  // --- Power Key (AXP2101) + автогашение экрана (энергосбережение) ---
#if SCREEN_DIMMING_ENABLED
  // Power Key: редкий опрос прямо из loop — единственного владельца шины.
  // НЕ опрашиваем, пока палец на экране (finger_down): дифф со старой
  // прошивкой (SCREEN_DIMMING 0, листалось идеально) показал, что PMU-опрос —
  // единственное новое на шине тача, и при периоде 150мс он попадал
  // практически в КАЖДЫЙ свайп (свайп ~150мс), влезая между координатами
  // жеста. Теперь во время жеста шина на 100% принадлежит тачу.
  static uint32_t lastPwrPoll = 0;
  if (pmuOnline && !finger_down && millis() - lastPwrPoll > 250) {
    lastPwrPoll = millis();
#if DIAG
    uint32_t t0 = millis();                          // [diag] сколько стоит опрос
#endif
    pmu.getIrqStatus();
    bool sp = pmu.isPekeyShortPressIrq();
    pmu.clearIrqStatus();
#if DIAG
    uint32_t dt = millis() - t0;
    if (dt > diagMaxPmuMs) diagMaxPmuMs = dt;
#endif
    if (sp) {
      if (screenOn) screenSleep();
      else          wakeRequested = true;
    }
  }
  // Пробуждение (по касанию или Power Key) — единственное место включения яркости.
  if (wakeRequested) {
    wakeRequested = false;
    if (!screenOn) {
      screenWake();
      finger_down = false;
      press_start = 0;
      long_press_handled = false;
      lv_indev_wait_release(lv_indev_get_act());
    }
    lastActivity = millis();
  }
  // Автогашение после SCREEN_TIMEOUT_MS без касаний (во время звонка не гасим).
  if (screenOn && !call_overlay && millis() - lastActivity > SCREEN_TIMEOUT_MS) {
    screenSleep();
  }
#endif

  // --- Батарея: опрос раз в 10с из loop (владелец шины), рисуем при изменении ---
  if (pmuOnline) {
    static uint32_t lastBatt = 0;
    static bool battInit = false;
    // БАГ был: гейт `battPercent < 0` при ОТСУТСТВУЮЩЕЙ батарее оставался
    // истинным вечно (percent = -1) → чтение I2C каждый цикл → шина забита,
    // тач висит. Флаг battInit: один раз на старте, дальше строго раз в 10с.
    if (!battInit || millis() - lastBatt > 10000) {
      battInit = true;
      lastBatt = millis();
      bool battPresent = pmu.isBatteryConnect();     // батареи может НЕ быть (тест от USB)
      int p = battPresent ? pmu.getBatteryPercent() : -1;
      bool ch = battPresent ? pmu.isCharging() : false;
      if (p != battPercent || ch != battCharging) {
        battPercent = p;
        battCharging = ch;
        updateHomeBattery();
        Serial.printf("[batt] %d%% charging=%d present=%d\n", battPercent, battCharging, battPresent);
      }
    }
  }

  // --- Индикатор связи на Home ---
  if (bleStatusDirty && home_bt_icon) {
    bleStatusDirty = false;
    lv_obj_set_style_text_color(home_bt_icon,
        lv_color_hex(bleConnected ? 0x0a84ff : 0x555555), 0);
  }

  // --- Лента: отложенное перестроение (свайп-удаление / раскрытие / сворачивание) ---
  // Перестраиваем ЗДЕСЬ, а не в event-колбэке карточки: lv_obj_clean внутри
  // события удалял бы объект, по которому LVGL ещё ведёт обработку.
  if (notif_dirty) {
    notif_dirty = false;
    if (activeModule == MOD_NOTIFICATIONS) notifOnEnter();
    updateHomeBadge();
  }

  // --- Звонок: показать/обновить/убрать полноэкранный экран ---
  if (call_show) { call_show = false; showCall(); }
  if (call_hide) { call_hide = false; hideCall(); }

  // --- Звонок: действия с кнопок (одна функция вместо двух дублей) ---
  if (call_accept_pending) { call_accept_pending = false; callAction(0x00); hideCall(); }
  if (call_reject_pending) { call_reject_pending = false; callAction(0x01); hideCall(); }

#if DIAG
  uint32_t rnd0 = millis();                    // [diag] стоимость рендера LVGL
#endif
  uint32_t idle = lv_timer_handler();
#if DIAG
  uint32_t rndDt = millis() - rnd0;
  if (rndDt > diagMaxRender) diagMaxRender = rndDt;
#endif
  // LVGL сам говорит, сколько можно спать до следующего кадра.
  // Ограничиваем сверху, чтобы тач и BLE обрабатывались часто (отзывчивость).
  if (idle > 5) idle = 5;
  delay(idle);
}
