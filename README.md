# SmartTag

**NFC-смарт визитка с тачскрином** — устройство, которое заменяет бумажные визитки. Одно касание телефона — и контакт открывает ваш профиль, соцсеть, сайт или подключается к WiFi.

📷 Instagram: [@nfcsmarttag](https://instagram.com/nfcsmarttag)

---

## Архитектура проекта

```
SmartTag/
├── firmware/
│   ├── stm32/             # Прошивка основного MCU (STM32CubeIDE)
│   └── esp8266/           # Прошивка WiFi-модуля (Arduino IDE)
├── web/
│   ├── backend/           # API сервер (Node.js + Express)
│   └── frontend/          # Личный кабинет (React)
├── tools/
│   └── img_converter/     # Утилиты: конвертер иконок в RGB565
├── docs/                  # Документация, схемы, анализ
├── hardware/              # Pinout, схемы подключения
└── assets/
    └── icons/             # Иконки для SD-карты
```

## Железо

| Компонент | Модель | Интерфейс |
|-----------|--------|-----------|
| MCU | STM32H750VBTx | — |
| Дисплей | ST7796U TFT | SPI |
| NFC | ST25DV16K | I²C |
| Тачскрин | FT6236 | I²C |
| WiFi | ESP8266 D1 Mini | UART4 |
| Хранилище | SD Card | SDMMC/SPI |

## Быстрый старт

### Прошивка
1. Открыть `firmware/stm32/` в STM32CubeIDE
2. Открыть `firmware/esp8266/` в Arduino IDE
3. Прошить оба модуля

### Веб (личный кабинет)
```bash
# Backend
cd web/backend
npm install
cp .env.example .env    # заполнить переменные
npm run dev

# Frontend
cd web/frontend
npm install
npm start
```

## Лицензия

Проект в разработке. Лицензия будет определена позже.
