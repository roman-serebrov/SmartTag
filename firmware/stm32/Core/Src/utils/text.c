#include "text.h"
#include <stdint.h>

uint16_t text_width_mont(const char* str) {
    uint16_t w = 0;
    const uint8_t* s = (const uint8_t*)str;
    while (*s) {
        if (*s < 0x80) { s++; }
        else if ((*s & 0xE0) == 0xC0) { s += 2; }
        else { s++; continue; }
        w += 10;
    }
    return w;
}
