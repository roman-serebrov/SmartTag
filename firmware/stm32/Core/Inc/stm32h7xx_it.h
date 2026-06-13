/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __STM32H7xx_IT_H
#define __STM32H7xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Флаг срабатывания NFC GPO — выставляется в EXTI15_10_IRQHandler */
extern volatile uint8_t g_nfc_gpo_fired;

/* Exported functions prototypes */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI4_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void DMA1_Stream0_IRQHandler(void);
void SPI1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_IT_H */
