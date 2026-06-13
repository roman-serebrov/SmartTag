################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/nfc/nfc.c 

OBJS += \
./Core/Src/nfc/nfc.o 

C_DEPS += \
./Core/Src/nfc/nfc.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/nfc/%.o Core/Src/nfc/%.su Core/Src/nfc/%.cyclo: ../Core/Src/nfc/%.c Core/Src/nfc/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/nfc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/storage" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/app" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Src/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/lib_NDEF/Core/inc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/Common/inc" -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../NFC/App -I../Drivers/BSP/Components/ST25DV -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-nfc

clean-Core-2f-Src-2f-nfc:
	-$(RM) ./Core/Src/nfc/nfc.cyclo ./Core/Src/nfc/nfc.d ./Core/Src/nfc/nfc.o ./Core/Src/nfc/nfc.su

.PHONY: clean-Core-2f-Src-2f-nfc

