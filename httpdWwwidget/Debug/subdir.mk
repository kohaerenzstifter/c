################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../wwwidget.c 

OBJS += \
./wwwidget.o 

C_DEPS += \
./wwwidget.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(shell pkg-config --cflags glib-2.0) -I"$(KOHAERENZSTIFTUNG_PATH)" -I"$(HTTPD_PATH)/src" -O0 -g3 -Wall -Werror -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


