################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/commands/subdir.mk
-include src/subdir.mk
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
all: docs

dependents:
	-cd /home/sancho/Desktop/k/kohaerenzstiftung/Debug && $(MAKE) all

# Tool invocations
docs: $(OBJS) $(USER_OBJS) /home/sancho/Desktop/k/kohaerenzstiftung/Debug/libkohaerenzstiftung.so
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L"/home/sancho/Desktop/k/kohaerenzstiftung/Debug" -o "docs" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) docs
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
/home/sancho/Desktop/k/kohaerenzstiftung/Debug/libkohaerenzstiftung.so:

-include ../makefile.targets
