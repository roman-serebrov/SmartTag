#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "tagtype5_wrapper.h"

#define NFC_DEVICE_MAX_NDEFMEMORY  2048
#define ST25DV_MAX_SIZE            2048
#define RESULTOK                   0
#define ERRORCODE_GENERIC          1
#define URI_ID_0x00_STRING         ""

#ifdef __cplusplus
}
#endif

#endif
