#include "touch.h"

extern I2C_HandleTypeDef hi2c2;

#define FT6236_ADDR     0x38 << 1
#define FT6236_TD_STATUS 0x02
#define FT6236_TOUCH1_XH 0x03

uint8_t Touch_Read(uint16_t *x, uint16_t *y) {
    uint8_t buf[6];
    uint8_t reg = FT6236_TD_STATUS;
    uint16_t sum_x = 0, sum_y = 0;
    uint8_t count = 0;

    /* Читаем 3 раза и усредняем */
    for (uint8_t i = 0; i < 3; i++) {
        HAL_I2C_Master_Transmit(&hi2c2, FT6236_ADDR, &reg, 1, 10);
        HAL_I2C_Master_Receive(&hi2c2, FT6236_ADDR, buf, 6, 10);

        uint8_t touches = buf[0] & 0x0F;
        if (touches == 0 || touches > 2) continue;

        uint16_t raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
        uint16_t raw_y = ((buf[3] & 0x0F) << 8) | buf[4];

        sum_x += raw_y;
        sum_y += 240 - raw_x;
        count++;
        HAL_Delay(2);
    }

    if (count == 0) return 0;

    *x = sum_x / count;
    *y = sum_y / count;

    return 1;
}
