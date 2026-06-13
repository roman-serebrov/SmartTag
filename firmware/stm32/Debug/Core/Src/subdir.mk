################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/font_montserrat.c \
../Core/Src/lcd_s7796u.c \
../Core/Src/main.c \
../Core/Src/nfc_wrapper.c \
../Core/Src/qrcodegen.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/touch.c 

OBJS += \
./Core/Src/font_montserrat.o \
./Core/Src/lcd_s7796u.o \
./Core/Src/main.o \
./Core/Src/nfc_wrapper.o \
./Core/Src/qrcodegen.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/touch.o 

C_DEPS += \
./Core/Src/font_montserrat.d \
./Core/Src/lcd_s7796u.d \
./Core/Src/main.d \
./Core/Src/nfc_wrapper.d \
./Core/Src/qrcodegen.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/touch.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/nfc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/storage" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/app" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/utils" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/ui" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Src/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/esp" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Core/Inc/screensaver" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/lib_NDEF/Core/inc" -I"/Users/romanagalarov/Projects/SmartTag/firmware/stm32/Middlewares/ST/lib_nfc/Common/inc" -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../NFC/App -I../Drivers/BSP/Components/ST25DV -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/font_montserrat.cyclo ./Core/Src/font_montserrat.d ./Core/Src/font_montserrat.o ./Core/Src/font_montserrat.su ./Core/Src/lcd_s7796u.cyclo ./Core/Src/lcd_s7796u.d ./Core/Src/lcd_s7796u.o ./Core/Src/lcd_s7796u.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/nfc_wrapper.cyclo ./Core/Src/nfc_wrapper.d ./Core/Src/nfc_wrapper.o ./Core/Src/nfc_wrapper.su ./Core/Src/qrcodegen.cyclo ./Core/Src/qrcodegen.d ./Core/Src/qrcodegen.o ./Core/Src/qrcodegen.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/touch.cyclo ./Core/Src/touch.d ./Core/Src/touch.o ./Core/Src/touch.su

.PHONY: clean-Core-2f-Src

