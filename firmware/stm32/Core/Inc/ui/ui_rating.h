#pragma once
#include <stdint.h>

void UI_Rating_Show(void);
uint8_t UI_Rating_Update(uint16_t tx, uint16_t ty, uint8_t touched);
uint8_t UI_Rating_IsDone(void);
uint8_t UI_Rating_GetScore(void);
void UI_Rating_Reset(void);
