#include "stm32h7xx_hal.h"
#include "tagtype5_wrapper.h"
#include "lib_NDEF.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

#define ST25DV_ADDR_DATA   0xA6
#define ST25DV_ADDR_DATA_R 0xA7

uint16_t NDEF_Wrapper_ReadData(uint8_t* pData, uint16_t Offset, uint16_t DataSize) {
    uint8_t addr[2] = {Offset >> 8, Offset & 0xFF};
    if (HAL_I2C_Master_Transmit(&hi2c1, ST25DV_ADDR_DATA, addr, 2, 100) != HAL_OK)
        return 1;
    if (HAL_I2C_Master_Receive(&hi2c1, ST25DV_ADDR_DATA_R, pData, DataSize, 200) != HAL_OK)
        return 1;
    return 0;
}

uint16_t NDEF_Wrapper_WriteData(uint8_t* pData, uint16_t Offset, uint16_t DataSize) {
    uint8_t buf[DataSize + 2];
    buf[0] = Offset >> 8;
    buf[1] = Offset & 0xFF;
    memcpy(&buf[2], pData, DataSize);
    if (HAL_I2C_Master_Transmit(&hi2c1, ST25DV_ADDR_DATA, buf, DataSize + 2, 200) != HAL_OK)
        return 1;
    HAL_Delay(6);
    return 0;
}

uint16_t NfcTag_SelectProtocol(NFCTAG_Protocol_Id_t protocol) {
    return 0;
}

uint16_t NfcTag_WriteNDEF(uint16_t Size, uint8_t* pData) {
    return NDEF_Wrapper_WriteData(pData, 0, Size);
}

uint16_t NfcTag_ReadNDEF(uint8_t* pData) {
    return NDEF_Wrapper_ReadData(pData, 0, NFC_DEVICE_MAX_NDEFMEMORY);
}
