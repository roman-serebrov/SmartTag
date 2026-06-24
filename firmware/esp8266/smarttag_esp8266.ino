/* SmartTag ESP8266 — прошивка Wi-Fi модуля
 *
 * Что изменилось относительно старой версии:
 *   - Убран хардкод ssid/password. Сеть и пароль больше НЕ в коде.
 *   - При первом включении (нет сохранённой сети) устройство поднимает
 *     свою точку доступа "SmartTag-XXXX". Клиент с телефона заходит на неё,
 *     открывается страница со списком найденных сетей + полем пароля,
 *     выбирает свою сеть, вводит пароль -> сохраняется в LittleFS -> ребут.
 *   - Если сеть сохранена — подключается к ней и работает как раньше:
 *     /data, /update, /profile, /done + mDNS smarttag.local.
 *   - Добавлен /wifi/reset — сброс сохранённой сети (для смены Wi-Fi
 *     из личного кабинета, когда устройство уже онлайн).
 *
 * НЕ изменилось:
 *   - SoftwareSerial со STM32 на пинах GPIO4/GPIO5 (RX/TX).
 *   - Протокол с STM32: P:idx:title|url|icon|isRating, PD:count, G, U:.
 *   - UART0 (Serial) — только отладка.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

/* ---------- глобальные объекты ---------- */
EspSoftwareSerial::UART stm32;
ESP8266WebServer        server(80);
DNSServer               dnsServer;

/* Режим работы: точка доступа (настройка) или станция (рабочий) */
enum Mode { MODE_AP, MODE_STA };
Mode mode = MODE_AP;

/* Сохранённые креды (читаются из флеша) */
String savedSsid = "";
String savedPass = "";
String savedCode = "";   /* короткий код устройства из личного кабинета */

/* Адрес сервера ЗАШИТ в прошивку — пользователь его не вводит.
   Для разработки — локальный IP компа; для продакшена поменять на домен,
   напр. "https://api.smarttag.app". Это единственная строка, которую
   меняешь при переезде на боевой сервер. */
#define SERVER_URL "http://192.168.0.107:3001"

const unsigned long HEARTBEAT_INTERVAL = 30000;  /* как часто сообщаем серверу «я онлайн» */

const char*    CRED_FILE = "/wifi.conf";
const IPAddress AP_IP(192, 168, 4, 1);
const byte     DNS_PORT  = 53;

/* ====================================================================
 *  ХРАНЕНИЕ КРЕДОВ В LittleFS
 *  Формат файла — 3 строки: ssid, password, code.
 *  Пароль НЕ тримим (значимые пробелы), у остальных убираем края.
 * ==================================================================== */
bool loadCreds() {
    if (!LittleFS.exists(CRED_FILE)) return false;
    File f = LittleFS.open(CRED_FILE, "r");
    if (!f) return false;

    savedSsid = f.readStringUntil('\n'); savedSsid.replace("\r", "");
    savedPass = f.readStringUntil('\n'); savedPass.replace("\r", "");
    savedCode = f.readStringUntil('\n'); savedCode.replace("\r", "");
    f.close();

    savedSsid.trim();                 /* SSID выбран из списка */
    savedCode.trim();
    return savedSsid.length() > 0;
}

bool saveCreds(const String& ssid, const String& pass, const String& code) {
    File f = LittleFS.open(CRED_FILE, "w");
    if (!f) return false;
    f.print(ssid); f.print('\n');
    f.print(pass); f.print('\n');
    f.print(code); f.print('\n');
    f.close();
    return true;
}

void clearCreds() {
    if (LittleFS.exists(CRED_FILE)) LittleFS.remove(CRED_FILE);
}

/* ====================================================================
 *  РАБОЧИЕ ОБРАБОТЧИКИ (режим станции) — логика как в старой версии
 * ==================================================================== */

/* Простой парсер JSON строки */
String getJsonString(const String& json, const String& key) {
    String search = "\"" + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf("\"", start);
    if (end < 0) return "";
    return json.substring(start, end);
}

int getJsonInt(const String& json, const String& key) {
    String search = "\"" + key + "\":";
    int start = json.indexOf(search);
    if (start < 0) return 0;
    start += search.length();
    String num = "";
    for (unsigned int i = start; i < json.length(); i++) {
        char c = json.charAt(i);
        if (c >= '0' && c <= '9') num += c;
        else if (num.length() > 0) break;
    }
    return num.toInt();
}

/* GET /data — запрос статистики со STM32 */
void handleGetData() {
    stm32.print("G");
    delay(500);

    String response = "";
    if (stm32.available()) {
        response = stm32.readStringUntil('\n');
        response.trim();
    }

    if (response.length() > 5 && response.startsWith("{")) {
        server.send(200, "application/json", response);
    } else {
        server.send(500, "application/json", "{\"error\":\"no data from STM32\"}");
    }
}

/* POST /update — старый текстовый эндпоинт */
void handlePostUpdate() {
    String body = server.arg("plain");
    String toSend = "U:" + body + "\n";
    delay(100);
    for (unsigned int i = 0; i < toSend.length(); i++) {
        stm32.write(toSend[i]);
        delay(2);
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

/* POST /profile — один профиль */
void handlePostProfile() {
    String body = server.arg("plain");

    int idx      = getJsonInt(body, "idx");
    String title = getJsonString(body, "title");
    String url   = getJsonString(body, "url");
    String icon  = getJsonString(body, "icon");
    int isRating = getJsonInt(body, "isRating");

    String cmd = "P:" + String(idx) + ":" + title + "|" + url + "|" + icon + "|" + String(isRating) + "\n";
    Serial.println("-> STM32: " + cmd);

    for (unsigned int i = 0; i < cmd.length(); i++) {
        stm32.write(cmd[i]);
        delay(2);
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

/* POST /done — все профили отправлены */
void handlePostDone() {
    String body = server.arg("plain");
    int count = getJsonInt(body, "count");

    String cmd = "PD:" + String(count) + "\n";
    Serial.println("-> STM32: " + cmd);

    for (unsigned int i = 0; i < cmd.length(); i++) {
        stm32.write(cmd[i]);
        delay(2);
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"count\":" + String(count) + "}");
}

void handleRoot() {
    server.send(200, "text/plain",
        "SmartTag API\nGET /data\nPOST /update\nPOST /profile\nPOST /done\nPOST /wifi/reset");
}

/* POST /wifi/reset — сбросить сеть и уйти в режим настройки.
   Дёргается из личного кабинета, когда нужно сменить Wi-Fi. */
void handleWifiReset() {
    server.send(200, "application/json", "{\"status\":\"resetting\"}");
    delay(300);
    clearCreds();
    ESP.restart();
}

/* ====================================================================
 *  СТРАНИЦА НАСТРОЙКИ (режим точки доступа)
 * ==================================================================== */
const char SETUP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartTag — настройка Wi-Fi</title>
<style>
  *{box-sizing:border-box;font-family:-apple-system,Segoe UI,Roboto,sans-serif}
  body{margin:0;background:#0f1115;color:#e8eaed;padding:20px;max-width:480px;margin:0 auto}
  h1{font-size:20px;margin:8px 0 4px}
  p.sub{color:#9aa0a6;margin:0 0 18px;font-size:14px}
  .net{display:flex;justify-content:space-between;align-items:center;
       padding:12px 14px;margin:6px 0;background:#1b1e24;border:1px solid #2a2e36;
       border-radius:10px;cursor:pointer}
  .net:hover{border-color:#4c8bf5}
  .net.sel{border-color:#4c8bf5;background:#1d2533}
  .net .name{font-size:15px}
  .net .rssi{color:#9aa0a6;font-size:12px}
  .lock{color:#9aa0a6;font-size:12px;margin-left:6px}
  input{width:100%;padding:12px 14px;margin:10px 0;background:#1b1e24;
        border:1px solid #2a2e36;border-radius:10px;color:#e8eaed;font-size:15px}
  button{width:100%;padding:13px;border:0;border-radius:10px;background:#4c8bf5;
         color:#fff;font-size:16px;font-weight:600;cursor:pointer;margin-top:8px}
  button:disabled{opacity:.5}
  .ghost{background:#1b1e24;border:1px solid #2a2e36;color:#e8eaed;font-weight:400}
  #status{text-align:center;color:#9aa0a6;margin-top:14px;font-size:14px}
</style></head><body>
<h1>Подключение SmartTag</h1>
<p class="sub">Выберите вашу сеть Wi-Fi и введите пароль</p>
<div id="list"><p class="sub">Поиск сетей…</p></div>
<button class="ghost" onclick="scan()">↻ Обновить список</button>
<input id="ssid" placeholder="Название сети" autocomplete="off">
<input id="pass" type="password" placeholder="Пароль">
<input id="code" placeholder="Код устройства (6 символов)" autocomplete="off" maxlength="8" style="text-transform:uppercase">
<button id="go" onclick="save()" disabled>Подключить</button>
<div id="status"></div>
<script>
function scan(){
  var L=document.getElementById('list');
  if(L.dataset.polling!=='1') L.innerHTML='<p class="sub">Поиск сетей…</p>';
  fetch('/scan').then(r=>r.json()).then(function(d){
    if(d.scanning){ L.dataset.polling='1'; setTimeout(scan,1500); return; }
    L.dataset.polling='';
    var nets=d.networks||[];
    var h='';
    nets.forEach(function(n){
      var lock=n.secure?'<span class="lock">\u{1F512}</span>':'';
      h+='<div class="net" onclick="pick(this,\''+n.ssid.replace(/'/g,"\\'")+'\')">'
        +'<span class="name">'+n.ssid+lock+'</span>'
        +'<span class="rssi">'+n.rssi+' dBm</span></div>';
    });
    if(!nets.length) h='<p class="sub">Сети не найдены. Нажмите «Обновить».</p>';
    L.innerHTML=h;
  }).catch(function(){L.dataset.polling='';document.getElementById('list').innerHTML='<p class="sub">Ошибка сканирования</p>';});
}
function pick(el,ssid){
  document.querySelectorAll('.net').forEach(function(e){e.classList.remove('sel')});
  el.classList.add('sel');
  document.getElementById('ssid').value=ssid;
  check();
}
function check(){
  document.getElementById('go').disabled=!document.getElementById('ssid').value;
}
document.getElementById('ssid').addEventListener('input',check);
function save(){
  var ssid=document.getElementById('ssid').value;
  var pass=document.getElementById('pass').value;
  var code=document.getElementById('code').value.toUpperCase();
  if(!ssid)return;
  document.getElementById('go').disabled=true;
  document.getElementById('status').textContent='Сохраняю и подключаюсь…';
  var b='ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)
       +'&code='+encodeURIComponent(code);
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
   .then(function(){document.getElementById('status').textContent=
     'Готово. Устройство перезагружается и подключается к «'+ssid+'». Сеть SmartTag-… скоро исчезнет.';})
   .catch(function(){document.getElementById('status').textContent='Ошибка сохранения';
     document.getElementById('go').disabled=false;});
}
scan();
</script></body></html>
)HTML";

/* GET /scan — список найденных сетей в JSON, отсортирован по сигналу, без дублей */
void handleScan() {
    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_RUNNING) {              /* -1: скан ещё идёт */
        server.send(200, "application/json", "{\"scanning\":true}");
        return;
    }
    if (n == WIFI_SCAN_FAILED) {               /* -2: не запускался — стартуем асинхронно */
        WiFi.scanNetworks(true /*async*/, true /*hidden*/);
        server.send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    int cnt = (n > 50) ? 50 : n;

    int order[50];
    for (int i = 0; i < cnt; i++) order[i] = i;
    for (int i = 0; i < cnt; i++)
        for (int j = i + 1; j < cnt; j++)
            if (WiFi.RSSI(order[j]) > WiFi.RSSI(order[i])) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }

    String json = "{\"networks\":[";
    bool first = true;
    for (int k = 0; k < cnt; k++) {
        int i = order[k];
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;

        bool dup = false;
        for (int m = 0; m < k; m++) if (WiFi.SSID(order[m]) == s) { dup = true; break; }
        if (dup) continue;

        String esc = "";
        for (unsigned int c = 0; c < s.length(); c++) {
            char ch = s[c];
            if (ch == '"' || ch == '\\') esc += '\\';
            esc += ch;
        }
        if (!first) json += ",";
        first = false;
        json += "{\"ssid\":\"" + esc + "\",\"rssi\":" + String(WiFi.RSSI(i))
              + ",\"secure\":" + String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false") + "}";
    }
    json += "]}";
    WiFi.scanDelete();   /* следующий /scan увидит -2 и запустит свежий скан */
    server.send(200, "application/json", json);
}

/* POST /save — получили сеть и пароль, сохраняем, ребут */
void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");   /* пароль НЕ тримим */
    String code = server.arg("code");
    ssid.trim();
    code.trim();
    code.toUpperCase();

    if (ssid.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"empty ssid\"}");
        return;
    }
    saveCreds(ssid, pass, code);
    server.send(200, "application/json", "{\"status\":\"saved\"}");
    Serial.println("Saved Wi-Fi: " + ssid + " | code: " + code + " -> reboot");
    delay(800);
    ESP.restart();
}

/* Captive portal: на ЛЮБОЙ неизвестный запрос отдаём саму страницу настройки.
   Для iOS это критично: системный детектор captive (запрос к captive.apple.com)
   должен получить НЕ слово "Success", а нашу страницу — тогда iPhone сам
   показывает всплывающее окно с настройкой, без ручного ввода адреса в Safari.
   Android/Windows аналогично покажут уведомление «Войти в сеть». */
void handleCaptive() {
    server.send_P(200, "text/html", SETUP_PAGE);
}

/* ====================================================================
 *  ЗАПУСК РЕЖИМОВ
 * ==================================================================== */
/* ====================================================================
 *  HEARTBEAT — устройство само сообщает серверу «я онлайн»
 *  POST {server}/api/devices/heartbeat  body: {"token":..., "localIp":...}
 *  Сервер находит девайс по токену, ставит lastSeen=now, localIp.
 * ==================================================================== */
/* Запросить у STM32 статистику сканов (команда G) -> JSON-строка или "" */
String getStmStats() {
    while (stm32.available()) stm32.read();   /* чистим возможный мусор */
    stm32.print("G");
    delay(500);
    String resp = "";
    if (stm32.available()) {
        resp = stm32.readStringUntil('\n');
        resp.trim();
    }
    return resp.startsWith("{") ? resp : "";
}

void sendHeartbeat() {
    if (savedCode.length() == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String stats = getStmStats();   /* {"total":..,"p0":..,...} или "" */

    String url  = String(SERVER_URL) + "/api/devices/heartbeat";
    String body = "{\"code\":\"" + savedCode + "\",\"hwId\":\"" + WiFi.macAddress() +
                  "\",\"localIp\":\"" + WiFi.localIP().toString() + "\"";
    if (stats.length() > 0) body += ",\"stats\":" + stats;
    body += "}";

    HTTPClient http;
    http.setTimeout(3000);            /* не блокировать loop надолго, если сервер недоступен */
    int code = -1;

    if (String(SERVER_URL).startsWith("https")) {
        BearSSL::WiFiClientSecure sc;
        sc.setInsecure();             /* ESP8266 не тянет полную проверку TLS-сертификата */
        if (http.begin(sc, url)) {
            http.addHeader("Content-Type", "application/json");
            code = http.POST(body);
            http.end();
        }
    } else {
        WiFiClient c;
        if (http.begin(c, url)) {
            http.addHeader("Content-Type", "application/json");
            code = http.POST(body);
            http.end();
        }
    }
    Serial.println("Heartbeat " + url + " -> " + String(code));
}

/* ====================================================================
 *  ПОДТЯГИВАНИЕ ПРОФИЛЕЙ ИЗ БАЗЫ (источник истины — сервер)
 *  GET {SERVER_URL}/api/devices/profiles?code=КОД
 *  Сервер отдаёт уже готовые строки протокола STM32:
 *     P:idx:title|url|icon|isRating
 *     ...
 *     PD:count
 *  ESP просто пересылает тело ответа в STM32 — парсить ничего не нужно.
 * ==================================================================== */
void pullProfiles() {
    if (savedCode.length() == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String url = String(SERVER_URL) + "/api/devices/profiles?code=" + savedCode;

    HTTPClient http;
    http.setTimeout(5000);
    int code = -1;
    String body = "";

    if (String(SERVER_URL).startsWith("https")) {
        BearSSL::WiFiClientSecure sc;
        sc.setInsecure();
        if (http.begin(sc, url)) {
            code = http.GET();
            if (code == 200) body = http.getString();
            http.end();
        }
    } else {
        WiFiClient c;
        if (http.begin(c, url)) {
            code = http.GET();
            if (code == 200) body = http.getString();
            http.end();
        }
    }

    Serial.println("Pull profiles -> " + String(code));

    if (code == 200 && body.length() > 0) {
        /* пересылаем готовые строки P:/PD: в STM32 как есть */
        for (unsigned int i = 0; i < body.length(); i++) {
            stm32.write(body[i]);
            delay(2);
        }
        Serial.println("Profiles forwarded to STM32:\n" + body);
    }
}

bool startStation() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    Serial.print("Connecting to " + savedSsid);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
        delay(250);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" FAILED");
        return false;
    }
    Serial.println("\nIP: " + WiFi.localIP().toString());

    MDNS.begin("smarttag");

    server.on("/",           handleRoot);
    server.on("/data",       HTTP_GET,  handleGetData);
    server.on("/update",     HTTP_POST, handlePostUpdate);
    server.on("/profile",    HTTP_POST, handlePostProfile);
    server.on("/done",       HTTP_POST, handlePostDone);
    server.on("/wifi/reset", HTTP_POST, handleWifiReset);
    server.begin();

    Serial.println("Server OK (station mode)");
    mode = MODE_STA;
    sendHeartbeat();                  /* сразу сообщаем серверу, что мы онлайн */
    pullProfiles();                   /* и подтягиваем актуальные профили из базы */
    return true;
}

void startAccessPoint() {
    /* AP_STA — чтобы сканировать сети, оставаясь точкой доступа */
    WiFi.mode(WIFI_AP_STA);

    char suffix[5];
    sprintf(suffix, "%04X", ESP.getChipId() & 0xFFFF);
    String apName = "SmartTag-" + String(suffix);

    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName);                 /* открытая сеть, без пароля */
    Serial.println("AP up: " + apName + "  http://" + AP_IP.toString());

    /* captive portal: весь DNS заворачиваем на себя */
    dnsServer.start(DNS_PORT, "*", AP_IP);

    server.on("/",     HTTP_GET,  []() { server.send_P(200, "text/html", SETUP_PAGE); });
    server.on("/scan", HTTP_GET,  handleScan);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleCaptive);
    server.begin();

    WiFi.scanNetworks(true /*async*/, true /*hidden*/);   /* первый скан заранее */
    Serial.println("Server OK (setup mode)");
    mode = MODE_AP;
}

/* ====================================================================
 *  SETUP / LOOP
 * ==================================================================== */
void setup() {
    Serial.begin(115200);
    stm32.begin(9600, SWSERIAL_8N1, 4, 5, false, 256);   /* RX=GPIO4 TX=GPIO5 — без изменений */
    stm32.setTimeout(1000);

    WiFi.persistent(false);              /* креды храним сами в LittleFS */

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed, formatting…");
        LittleFS.format();
        LittleFS.begin();
    }

    if (loadCreds()) {
        Serial.println("Found saved Wi-Fi: " + savedSsid);
        if (!startStation()) {
            Serial.println("Falling back to setup AP");
            startAccessPoint();
        }
    } else {
        Serial.println("No saved Wi-Fi -> setup AP");
        startAccessPoint();
    }
}

void loop() {
    server.handleClient();

    if (mode == MODE_AP) {
        dnsServer.processNextRequest();
    } else {
        MDNS.update();
        /* мягкое переподключение, как было */
        static unsigned long lastTry = 0;
        if (WiFi.status() != WL_CONNECTED && millis() - lastTry > 5000) {
            lastTry = millis();
            WiFi.reconnect();
        }
        /* heartbeat раз в HEARTBEAT_INTERVAL */
        static unsigned long lastHB = 0;
        if (WiFi.status() == WL_CONNECTED && millis() - lastHB > HEARTBEAT_INTERVAL) {
            lastHB = millis();
            sendHeartbeat();
        }
    }
}
