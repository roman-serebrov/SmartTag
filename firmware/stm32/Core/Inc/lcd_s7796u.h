#ifndef LCD_S7796U_H
#define LCD_S7796U_H

#include "stm32h7xx_hal.h"

extern SPI_HandleTypeDef hspi1;

/* GPIO — подтяни к своим портам */
#define LCD_CS_PORT   GPIOA
#define LCD_CS_PIN    GPIO_PIN_4
#define LCD_DC_PORT   GPIOA
#define LCD_DC_PIN    GPIO_PIN_2
#define LCD_RST_PORT  GPIOA
#define LCD_RST_PIN   GPIO_PIN_3

#define LCD_CS_LOW()   HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_SET)
#define LCD_DC_CMD()   HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_RESET)
#define LCD_DC_DATA()  HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_SET)
#define LCD_RST_LOW()  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

/* Размер */
#define LCD_WIDTH   320
#define LCD_HEIGHT  240


/* Цвета RGB565 */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_ORANGE  0xFD20

/* API */
void LCD_Init(void);
void LCD_FillScreen(uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str,
                    uint16_t fg, uint16_t bg, uint8_t scale);
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawFrameBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buf);
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);
#endif
