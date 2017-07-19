USER_OBJS := libkohaerenzstiftung.a
OBJS := ../src/main.o ../src/output.o ../src/input.o \
  ../src/table.o ../src/gui.o ../src/ksequencer.o ../src/test.o
LIBS := -ldl `pkg-config --libs glib-2.0` `pkg-config --libs gtk+-3.0` \
  -lasound -pthread -lm

CFLAGS = -O0 -g -Wall -Werror `pkg-config --cflags glib-2.0` \
  `pkg-config --cflags gtk+-3.0` -I../../kohaerenzstiftung/ \
  -Wno-deprecated-declarations

#CFLAGS = -O0 -g -Wall -Werror `pkg-config --cflags glib-2.0` \
  `pkg-config --cflags gtk+-3.0` -I../../kohaerenzstiftung/ \
  -Wno-deprecated-declarations

# All Target
all: ksequencer

# Tool invocations
ksequencer: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -rdynamic -o "ksequencer" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'

# Other Targets
clean:
	 rm -rf $(OBJS) ksequencer
