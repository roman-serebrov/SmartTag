#include "lcd_s7796u.h"
#include <stdlib.h>

/* ============================================================ */
/*  Низкий уровень SPI                                          */
/* ============================================================ */

static void spi_send(uint8_t byte) {
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

static void lcd_cmd(uint8_t cmd) {
    LCD_DC_CMD();
    LCD_CS_LOW();
    spi_send(cmd);
    LCD_CS_HIGH();
}

static void lcd_data8(uint8_t data) {
    LCD_DC_DATA();
    LCD_CS_LOW();
    spi_send(data);
    LCD_CS_HIGH();
}

static void lcd_data16(uint16_t data) {
    LCD_DC_DATA();
    LCD_CS_LOW();
    spi_send(data >> 8);
    spi_send(data & 0xFF);
    LCD_CS_HIGH();
}

/* ============================================================ */
/*  Hardware Reset                                              */
/* ============================================================ */

static void lcd_reset(void) {
    LCD_CS_HIGH();
    LCD_RST_LOW();        // сразу в LOW
    HAL_Delay(100);       // держим долго
    LCD_RST_HIGH();
    HAL_Delay(200);       // ждём пока встанет
}

/* ============================================================ */
/*  Инициализация S7796U                                        */
/* ============================================================ */
static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void LCD_Init(void) {
    lcd_reset();
    HAL_Delay(100);  /* добавить эту строку */

       lcd_cmd(0x01);
       HAL_Delay(200);  /* увеличить с 120 до 200 */

       lcd_cmd(0x11);
       HAL_Delay(200);



    /* Interface Mode */
    lcd_cmd(0xB0); lcd_data8(0x00);

    /* Frame Rate: 60Hz */
    lcd_cmd(0xB1); lcd_data8(0xA0);

    /* Display Inversion: 2-dot */
    lcd_cmd(0xB4); lcd_data8(0x02);

    /* Display Function Control */
    lcd_cmd(0xB6);
    lcd_data8(0x02);
    lcd_data8(0x02);

    /* VCOM */
    lcd_cmd(0xC5);
    lcd_data8(0x02);
    lcd_data8(0x07);
    lcd_data8(0xFF);

    /* Positive Gamma */
    lcd_cmd(0xE0);
    lcd_data8(0x00); lcd_data8(0x03);
    lcd_data8(0x09); lcd_data8(0x08);
    lcd_data8(0x16); lcd_data8(0x0A);
    lcd_data8(0x3F); lcd_data8(0x78);
    lcd_data8(0x4C); lcd_data8(0x09);
    lcd_data8(0x0A); lcd_data8(0x08);
    lcd_data8(0x16); lcd_data8(0x1A);
    lcd_data8(0x0F);

    /* Negative Gamma */
    lcd_cmd(0xE1);
    lcd_data8(0x00); lcd_data8(0x16);
    lcd_data8(0x19); lcd_data8(0x03);
    lcd_data8(0x0F); lcd_data8(0x05);
    lcd_data8(0x32); lcd_data8(0x45);
    lcd_data8(0x46); lcd_data8(0x04);
    lcd_data8(0x0E); lcd_data8(0x0D);
    lcd_data8(0x35); lcd_data8(0x37);
    lcd_data8(0x0F);

    /* Memory Access: landscape 320x240, BGR */
    lcd_cmd(0x36); lcd_data8(0x28);

    /* Pixel Format: 16bit RGB565 */
    lcd_cmd(0x3A); lcd_data8(0x55);

    /* Display ON */
    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
       static uint16_t black_buf[320] = {0};
       LCD_DC_DATA();
       LCD_CS_LOW();
       hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
       HAL_SPI_Init(&hspi1);
       for (uint16_t row = 0; row < LCD_HEIGHT; row++) {
           HAL_SPI_Transmit(&hspi1, (uint8_t*)black_buf, LCD_WIDTH, HAL_MAX_DELAY);
       }
       hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
       HAL_SPI_Init(&hspi1);
       LCD_CS_HIGH();
    lcd_cmd(0x29); HAL_Delay(20);
}

/* ============================================================ */
/*  Окно вывода                                                 */
/* ============================================================ */

static void lcd_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    lcd_cmd(0x2A);
    lcd_data16(x);
    lcd_data16(x + w - 1);

    lcd_cmd(0x2B);
    lcd_data16(y);
    lcd_data16(y + h - 1);

    lcd_cmd(0x2C);
}

/* ============================================================ */
/*  Графика                                                     */
/* ============================================================ */

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    lcd_set_window(x, y, 1, 1);
    lcd_data16(color);
}

void LCD_FillRect(uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    lcd_set_window(x, y, w, h);

    static uint16_t fill_buf[320];
    for (uint16_t i = 0; i < w; i++) fill_buf[i] = color;

    LCD_DC_DATA();
    LCD_CS_LOW();
    hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&hspi1);
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(&hspi1, (uint8_t*)fill_buf, w, HAL_MAX_DELAY);
    }
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&hspi1);
    LCD_CS_HIGH();
}

void LCD_FillScreen(uint16_t color) {
    LCD_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_DrawLine(uint16_t x0, uint16_t y0,
                  uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx =  abs((int16_t)x1 - x0);
    int16_t dy = -abs((int16_t)y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    while (1) {
        LCD_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void LCD_DrawRect(uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h, uint16_t color) {
    LCD_DrawLine(x,       y,       x+w-1, y,       color);
    LCD_DrawLine(x,       y+h-1,   x+w-1, y+h-1,   color);
    LCD_DrawLine(x,       y,       x,     y+h-1,   color);
    LCD_DrawLine(x+w-1,   y,       x+w-1, y+h-1,   color);
}

/* ============================================================ */
/*  Шрифт 5x7                                                   */
/* ============================================================ */

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x04,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00},
    {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x40,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},
};

void LCD_DrawString(uint16_t x, uint16_t y, const char *str,
                    uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t cx = x;
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 'z') { cx += 6 * scale; continue; }
        const uint8_t *glyph = font5x7[c - 32];
        for (uint8_t col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            for (uint8_t row = 0; row < 7; row++) {
                uint16_t color = (bits & (1 << row)) ? fg : bg;
                if (scale == 1)
                    LCD_DrawPixel(cx + col, y + row, color);
                else
                    LCD_FillRect(cx + col*scale, y + row*scale,
                                 scale, scale, color);
            }
        }
        cx += 6 * scale;
    }
}

/* Отправить буфер кадра через DMA */
extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;


void LCD_DrawFrameBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buf) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    lcd_set_window(x, y, w, h);
    uint32_t n = (uint32_t)w * h;
    LCD_DC_DATA();
    LCD_CS_LOW();
    /* Переключаем в 16-bit */
    hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&hspi1);
    HAL_SPI_Transmit(&hspi1, (uint8_t*)buf, n, HAL_MAX_DELAY);
    /* Возвращаем 8-bit */
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&hspi1);
    LCD_CS_HIGH();
}

void LCD_DisplayOn(void) {
    lcd_cmd(0x29);
}

void LCD_DisplayOff(void) {
    lcd_cmd(0x28);
}
