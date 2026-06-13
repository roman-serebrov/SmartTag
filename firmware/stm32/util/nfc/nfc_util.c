static void NFC_Unlock(void) {
    uint8_t pwd[19] = {0x09,0x00,0,0,0,0,0,0,0,0,0x09,0,0,0,0,0,0,0,0};
    HAL_I2C_Master_Transmit(&hi2c1, 0xAE, pwd, 19, 200);
    HAL_Delay(10);
}

static void NFC_ProtectRF(void) {
    NFC_Unlock()
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

static void NFC_WriteURL(const char* url, uint8_t prefix) {
    NFC_Unlock();
    uint8_t url_len = strlen(url);
    uint8_t payload_len = 1 + url_len;
    uint8_t ndef[64];
    uint8_t idx = 0;
    ndef[idx++] = 0xD1;
    ndef[idx++] = 0x01;
    ndef[idx++] = payload_len;
    ndef[idx++] = 0x55;
    ndef[idx++] = prefix;
    for (uint8_t b = 0; b < url_len; b++) ndef[idx++] = url[b];
    uint8_t full[70];
    full[0] = 0x03;
    full[1] = idx;
    for (uint8_t i = 0; i < idx; i++) full[2+i] = ndef[i];
    full[2+idx] = 0xFE;
    NFC_WriteBlock(0x0004, full, idx+3);
}

static void NFC_WriteProfile(uint8_t sel) {
    if (profiles[sel].is_wifi) {
        NFC_WriteWiFi();
    } else {
        NFC_WriteURL(profiles[sel].url, profiles[sel].prefix);
    }
    NFC_ProtectRF();
}
