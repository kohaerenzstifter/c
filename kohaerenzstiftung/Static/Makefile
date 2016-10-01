%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc `pkg-config --cflags glib-2.0` \
		-O3 -Wall -c \
		-fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" \
		-MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

OBJS += ./kohaerenzstiftung.o 
C_DEPS += ./kohaerenzstiftung.d 

all: libkohaerenzstiftung.a

libkohaerenzstiftung.a: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC Archiver'
	ar -r  "libkohaerenzstiftung.a" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'

clean:
	rm -rf $(OBJS)$(C_DEPS) libkohaerenzstiftung.a
