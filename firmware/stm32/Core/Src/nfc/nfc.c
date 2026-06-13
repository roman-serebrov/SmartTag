#include "nfc.h"
#include "profiles.h"
#include "config.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;

#define SCAN_COUNT_REG      RTC_BKP_DR0
#define PROFILE_COUNT_BASE  RTC_BKP_DR1

static void NFC_Unlock(void);
static void NFC_ProtectRF(void);
static void NFC_WriteBlock(uint16_t addr, uint8_t* data, uint16_t size);
void NFC_WriteURL_Public(const char* url, uint8_t prefix);
static void NFC_WriteWiFi(void);
static void NFC_WriteVCard(void);

uint32_t NFC_GetScanCount(void) {
    return HAL_RTCEx_BKUPRead(&hrtc, SCAN_COUNT_REG);
}

uint32_t NFC_GetProfileCount(uint8_t sel) {
    if (sel >= NUM_PROFILES) return 0;
    return HAL_RTCEx_BKUPRead(&hrtc, PROFILE_COUNT_BASE + sel);
}

static void NFC_IncrementCounters(uint8_t sel) {
    uint32_t total = HAL_RTCEx_BKUPRead(&hrtc, SCAN_COUNT_REG);
    HAL_RTCEx_BKUPWrite(&hrtc, SCAN_COUNT_REG, total + 1);
    if (sel < NUM_PROFILES) {
        uint32_t prof = HAL_RTCEx_BKUPRead(&hrtc, PROFILE_COUNT_BASE + sel);
        HAL_RTCEx_BKUPWrite(&hrtc, PROFILE_COUNT_BASE + sel, prof + 1);
    }
}

static void NFC_Unlock(void) {
    uint8_t pwd[19] = {0x09,0x00,0,0,0,0,0,0,0,0,0x09,0,0,0,0,0,0,0,0};
    HAL_I2C_Master_Transmit(&hi2c1, 0xAE, pwd, 19, 200);
    HAL_Delay(10);
}

static void NFC_ProtectRF(void) {
    NFC_Unlock();
    HAL_Delay(5);
    uint8_t buf[3] = { 0x00, 0x04, 0x0C };
    HAL_I2C_Master_Transmit(&hi2c1, 0xAE, buf, 3, 200);
    HAL_Delay(10);
}

static void NFC_WriteBlock(uint16_t addr, uint8_t* data, uint16_t size) {
    uint8_t buf[66];
    buf[0] = addr >> 8;
    buf[1] = addr & 0xFF;
    for (uint16_t i = 0; i < size; i++) buf[i+2] = data[i];
    HAL_I2C_Master_Transmit(&hi2c1, 0xA6, buf, size+2, 500);
    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() - t < 200) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, 0xA6, 1, 10) == HAL_OK) break;
        HAL_Delay(1);
    }
}

void NFC_WriteURL_Public(const char* url, uint8_t prefix) {
    NFC_Unlock();
    uint16_t url_len     = (uint16_t)strlen(url);
    uint16_t payload_len = 1 + url_len;
    uint8_t  ndef[200];   /* запас для длинных ссылок */
    uint16_t idx = 0;
    ndef[idx++] = 0xD1;
    ndef[idx++] = 0x01;
    ndef[idx++] = (uint8_t)payload_len;
    ndef[idx++] = 0x55;
    ndef[idx++] = prefix;
    for (uint16_t b = 0; b < url_len; b++) ndef[idx++] = (uint8_t)url[b];
    uint8_t  full[210];
    uint16_t fidx = 0;
    full[fidx++] = 0x03;
    full[fidx++] = (uint8_t)idx;
    for (uint16_t i = 0; i < idx; i++) full[fidx++] = ndef[i];
    full[fidx++] = 0xFE;
    /* Пишем блоками по 60 байт */
    uint16_t remaining = fidx;
    uint16_t offset = 0;
    while (remaining > 0) {
        uint16_t chunk = (remaining > 60) ? 60 : remaining;
        NFC_WriteBlock(0x0004 + offset, &full[offset], chunk);
        offset += chunk;
        remaining -= chunk;
        HAL_Delay(5);
    }
}

static void NFC_WriteWiFi(void) {
    NFC_Unlock();
    static char wifi_str[128];
    snprintf(wifi_str, sizeof(wifi_str), "WIFI:T:WPA;S:%s;P:%s;;", WIFI_SSID, WIFI_PASSWORD);
    uint8_t url_len = strlen(wifi_str);
    uint8_t ndef[128];
    uint8_t idx = 0;
    ndef[idx++] = 0xD1;
    ndef[idx++] = 0x01;
    ndef[idx++] = url_len + 3;
    ndef[idx++] = 'T';
    ndef[idx++] = 0x02;
    ndef[idx++] = 'e';
    ndef[idx++] = 'n';
    for (uint8_t i = 0; i < url_len; i++) ndef[idx++] = wifi_str[i];
    uint8_t full[140];
    full[0] = 0x03;
    full[1] = idx;
    for (uint8_t i = 0; i < idx; i++) full[2+i] = ndef[i];
    full[2+idx] = 0xFE;
    NFC_WriteBlock(0x0004, full, idx+3);
}

static void NFC_WriteVCard(void) {
    NFC_Unlock();
    const char* vcard = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Roman\r\nTEL:+71234567890\r\nEND:VCARD\r\n";
    uint8_t vlen = strlen(vcard);
    const char* mime = "text/x-vcard";
    uint8_t mime_len = strlen(mime);
    uint8_t ndef[110];
    uint8_t idx = 0;
    ndef[idx++] = 0xD2;
    ndef[idx++] = mime_len;
    ndef[idx++] = vlen;
    for (uint8_t i = 0; i < mime_len; i++) ndef[idx++] = mime[i];
    for (uint8_t i = 0; i < vlen; i++)     ndef[idx++] = vcard[i];
    uint8_t full[120];
    uint8_t fidx = 0;
    full[fidx++] = 0x03;
    full[fidx++] = idx;
    for (uint8_t i = 0; i < idx; i++) full[fidx++] = ndef[i];
    full[fidx++] = 0xFE;
    uint16_t remaining = fidx;
    uint16_t offset = 0;
    while (remaining > 0) {
        uint16_t chunk = (remaining > 60) ? 60 : remaining;
        NFC_WriteBlock(0x0004 + offset, &full[offset], chunk);
        offset += chunk;
        remaining -= chunk;
        HAL_Delay(5);
    }
}

void NFC_WriteProfile(uint8_t sel) {
    if (profiles[sel].is_vcard) {
        NFC_WriteVCard();
    } else if (profiles[sel].is_wifi) {
        NFC_WriteWiFi();
    } else {
    	NFC_WriteURL_Public(profiles[sel].url, profiles[sel].prefix);
    }
    NFC_ProtectRF();
    NFC_IncrementCounters(sel);
}
