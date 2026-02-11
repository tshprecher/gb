#TODO: simplify

.PHONY: all
all: gbe tests

gbe:	cpu gbe.c
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o gbe gbe.c input.o inst.o cpu.o memory.o display.o sound.o timing.o -lX11 -lpulse -lpulse-simple

tests:  inst cpu
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o tests tests.c cpu.o input.o inst.o memory.o display.o sound.o timing.o -lX11 -lpulse -lpulse-simple

cpu:    inst cpu.c
	gcc  -c -I/opt/X11/include/ -I/opt/local/include/ -g -O0 -Wall cpu.c

inst:   display.c sound.c input.c memory.c inst/inst.c
	gcc -c -I/opt/X11/include/ -I/opt/local/include/ -g -O0 -Wall display.c sound.c input.c timing.c memory.c inst/inst.c

clean:
	rm ./gbe *_test *.o
