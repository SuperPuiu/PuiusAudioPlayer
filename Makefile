SOURCES = $(wildcard src/*.c)
CFLAGS = -Isrc/.. -Wall -pedantic -std=c11 -lSDL3 -lSDL3_mixer -lm

buildTest:
	mkdir -p bin
	gcc $(SOURCES) microui.c $(CFLAGS) -g -ggdb3 -lX11 -o bin/test

buildLinux:
	mkdir -p bin
	gcc $(SOURCES) microui.c $(CFLAGS) -o3 -lX11 -o bin/test

windows:
	x86_64-w64-mingw32-gcc -D WINDOWS="" $(SOURCES) microui.c $(CFLAGS) -lcomdlg32 -lgdi32 -o3 -o bin/windows_test.exe

run:
	./bin/test

clean:
	rm bin/test
	rm bin/windows_text.exe
