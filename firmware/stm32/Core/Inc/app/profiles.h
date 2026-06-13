#pragma once
#include <stdint.h>
#include "config.h"

typedef struct {
    const char*  name;
    const char*  url;
    uint8_t      prefix;
    uint16_t     color;
    const char*  filename;
    uint16_t*    icon;
    uint8_t      is_wifi;
    uint8_t      is_vcard;
    uint8_t      is_rating;
} Profile_t;

extern Profile_t profiles[NUM_PROFILES];

void Profiles_Init(void);
uint8_t Profiles_Count(void);
