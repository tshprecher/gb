#TODO: simplify

.PHONY: all
all:	gbe tests

gbe:	inst.a cpu.o gbe.c
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o gbe gbe.c inst.a cpu.o -lX11 -lpulse -lpulse-simple

tests:  inst.a cpu.o
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o tests tests.c cpu.o inst.a -lX11 -lpulse -lpulse-simple

cpu.o:	cpu.c
	gcc -c -I/opt/X11/include/ -I/opt/local/include/ -g -O0 -Wall cpu.c

inst.a:	display.c sound.c input.c memory.c inst/inst.c
	gcc -c -I/opt/X11/include/ -I/opt/local/include/ -g -O0 -Wall display.c sound.c input.c timing.c memory.c inst/inst.c
	ar rcs inst.a display.o sound.o input.o timing.o memory.o inst.o

.PHONY: clean
clean:
	rm -f ./gbe *_test cpu.o inst.a
