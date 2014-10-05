################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/plugins/subdir.mk
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
all: httpd

dependents:
	-cd /home/sancho/git/c/kohaerenzstiftung/Debug && $(MAKE) clean all

# Tool invocations
httpd: $(OBJS) $(USER_OBJS) /home/sancho/git/c/kohaerenzstiftung/Debug/libkohaerenzstiftung.so
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L"/home/sancho/git/c/kohaerenzstiftung/Debug" -rdynamic -o "httpd" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) httpd
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
/home/sancho/git/c/kohaerenzstiftung/Debug/libkohaerenzstiftung.so:

-include ../makefile.targets
