################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.c 

OBJS += \
./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.o 

C_DEPS += \
./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/lib_nfc/Common/src/%.o Middlewares/ST/lib_nfc/Common/src/%.su Middlewares/ST/lib_nfc/Common/src/%.cyclo: ../Middlewares/ST/lib_nfc/Common/src/%.c Middlewares/ST/lib_nfc/Common/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I"/Users/romanagalarov/STM32CubeIDE/workspace_2.1.1/display_tft_s7796u/Middlewares/ST/lib_nfc/lib_NDEF/Core/inc" -I"/Users/romanagalarov/STM32CubeIDE/workspace_2.1.1/display_tft_s7796u/Middlewares/ST/lib_nfc/Common/inc" -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../NFC/App -I../Drivers/BSP/Components/ST25DV -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-lib_nfc-2f-Common-2f-src

clean-Middlewares-2f-ST-2f-lib_nfc-2f-Common-2f-src:
	-$(RM) ./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.cyclo ./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.d ./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.o ./Middlewares/ST/lib_nfc/Common/src/lib_95HFConfigManager.su

.PHONY: clean-Middlewares-2f-ST-2f-lib_nfc-2f-Common-2f-src

