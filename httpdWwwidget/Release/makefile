################################################################################
# Automatically-generated file. Do not edit!
################################################################################

ifneq ($(MAKECMDGOALS),clean)
ifndef KOHAERENZSTIFTUNG_PATH
$(error Environment variable KOHAERENZSTIFTUNG_PATH not set)
endif
ifndef HTTPD_PATH
$(error Environment variable HTTPD_PATH not set)
endif
endif

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: libhttpdWwwidget.so

# Tool invocations
libhttpdWwwidget.so: $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L"$(KOHAERENZSTIFTUNG_PATH)/Release" -shared -o "libhttpdWwwidget.so" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(LIBRARIES) libhttpdWwwidget.so
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
$(LIBS):

-include ../makefile.targets
