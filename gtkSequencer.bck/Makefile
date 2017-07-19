KOHAERENZSTIFTUNG := ../kohaerenzstiftung/

all: binary/gtkSequencer

binary/gtkSequencer: $(KOHAERENZSTIFTUNG)/Static/libkohaerenzstiftung.a
	cp $(KOHAERENZSTIFTUNG)/Static/libkohaerenzstiftung.a Release/
	cp $(KOHAERENZSTIFTUNG)/kohaerenzstiftung.h src/
	cd Release && make
	-mkdir -p binary
	cp Release/gtkSequencer binary/gtkSequencer

clean:
	cd Release && make clean
	rm -rf Release/libkohaerenzstiftung.a
	rm -rf src/kohaerenzstiftung.h
	rm -rf src/*.o
	rm -rf binary