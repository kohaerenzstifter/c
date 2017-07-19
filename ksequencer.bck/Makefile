KOHAERENZSTIFTUNG := ../kohaerenzstiftung/

all: binary/ksequencer

binary/ksequencer: $(KOHAERENZSTIFTUNG)/Static/libkohaerenzstiftung.a
	cp $(KOHAERENZSTIFTUNG)/Static/libkohaerenzstiftung.a Release/
	cp $(KOHAERENZSTIFTUNG)/kohaerenzstiftung.h src/
	cd Release && make
	-mkdir -p binary
	cp Release/ksequencer binary/ksequencer

install: all
	install -m 755 binary/ksequencer /usr/bin/
	install -m 644 ksequencer.desktop /usr/share/applications/

uninstall:
	rm -rf /usr/bin/ksequencer
	rm -rf /usr/share/applications/ksequencer.desktop

clean:
	cd Release && make clean
	rm -rf Release/libkohaerenzstiftung.a
	rm -rf src/kohaerenzstiftung.h
	rm -rf src/*.o
	rm -rf binary
