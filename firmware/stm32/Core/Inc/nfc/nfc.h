#pragma once
#include <stdint.h>

void     NFC_WriteProfile(uint8_t sel);
uint32_t NFC_GetScanCount(void);
uint32_t NFC_GetProfileCount(uint8_t sel);
void NFC_WriteURL_Public(const char* url, uint8_t prefix);
