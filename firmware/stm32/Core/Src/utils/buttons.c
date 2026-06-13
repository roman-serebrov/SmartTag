#include "buttons.h"
#include "main.h"

#define BTN_LEFT_PORT   GPIOD
#define BTN_LEFT_PIN    GPIO_PIN_4
#define BTN_RIGHT_PORT  GPIOD
#define BTN_RIGHT_PIN   GPIO_PIN_3
#define BTN_DEBOUNCE_MS 300

static uint32_t s_last_press = 0;

void Buttons_Init(void) {
    /* Пины уже настроены в MX_GPIO_Init как INPUT PULLUP */
}

ButtonEvent_t Buttons_Update(void) {
    uint32_t now = HAL_GetTick();
    if (now - s_last_press < BTN_DEBOUNCE_MS) return BTN_NONE;

    if (HAL_GPIO_ReadPin(BTN_LEFT_PORT, BTN_LEFT_PIN) == GPIO_PIN_RESET) {
        s_last_press = now;
        return BTN_LEFT;
    }

    if (HAL_GPIO_ReadPin(BTN_RIGHT_PORT, BTN_RIGHT_PIN) == GPIO_PIN_RESET) {
        s_last_press = now;
        return BTN_RIGHT;
    }

    return BTN_NONE;
}
