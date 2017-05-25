################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/commands/edit.c \
../src/commands/find.c \
../src/commands/help.c \
../src/commands/process.c \
../src/commands/quit.c \
../src/commands/submit.c \
../src/commands/view.c 

OBJS += \
./src/commands/edit.o \
./src/commands/find.o \
./src/commands/help.o \
./src/commands/process.o \
./src/commands/quit.o \
./src/commands/submit.o \
./src/commands/view.o 

C_DEPS += \
./src/commands/edit.d \
./src/commands/find.d \
./src/commands/help.d \
./src/commands/process.d \
./src/commands/quit.d \
./src/commands/submit.d \
./src/commands/view.d 


# Each subdirectory must supply rules for building sources it contributes
src/commands/%.o: ../src/commands/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"$(KOHAERENZSTIFTUNG_PATH)" $(shell pkg-config --cflags glib-2.0) -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


