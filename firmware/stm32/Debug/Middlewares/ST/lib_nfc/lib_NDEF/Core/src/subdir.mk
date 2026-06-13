################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.c \
../Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.c \
../Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.c 

OBJS += \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.o \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.o \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.o 

C_DEPS += \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.d \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.d \
./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/lib_nfc/lib_NDEF/Core/src/%.o Middlewares/ST/lib_nfc/lib_NDEF/Core/src/%.su Middlewares/ST/lib_nfc/lib_NDEF/Core/src/%.cyclo: ../Middlewares/ST/lib_nfc/lib_NDEF/Core/src/%.c Middlewares/ST/lib_nfc/lib_NDEF/Core/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/nfc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/storage" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/app" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Src/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/lib_NDEF/Core/inc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/Common/inc" -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../NFC/App -I../Drivers/BSP/Components/ST25DV -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-lib_nfc-2f-lib_NDEF-2f-Core-2f-src

clean-Middlewares-2f-ST-2f-lib_nfc-2f-lib_NDEF-2f-Core-2f-src:
	-$(RM) ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.cyclo ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.d ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.o ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF.su ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.cyclo ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.d ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.o ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/lib_NDEF_URI.su ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.cyclo ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.d ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.o ./Middlewares/ST/lib_nfc/lib_NDEF/Core/src/tagtype5_wrapper.su

.PHONY: clean-Middlewares-2f-ST-2f-lib_nfc-2f-lib_NDEF-2f-Core-2f-src

