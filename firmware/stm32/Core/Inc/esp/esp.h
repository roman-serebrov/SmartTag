#pragma once
#include <stdint.h>

extern uint8_t g_esp_busy;
extern volatile uint8_t g_profiles_ready;

void ESP_Init(void);
void ESP_Update(void);
uint8_t ESP_GetProfileCount(void);
