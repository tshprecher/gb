#TODO: simplify

.PHONY: all
all:	gbe tests

gbe:	gbe.c cpu.c display.c inst.c input.c memory.c sound.c timing.c
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o gbe gbe.c \
		cpu.c inst.c display.c input.c memory.c sound.c timing.c -lX11 -lpulse -lpulse-simple

tests:  tests.c cpu.c display.c inst.c input.c memory.c sound.c timing.c
	gcc -I/opt/X11/include/ -I/opt/local/include/ -L/opt/X11/lib/ -L/opt/local/lib/ -g -O0 -Wall -o tests \
		tests.c cpu.c display.c inst.c input.c memory.c sound.c timing.c -lX11 -lpulse -lpulse-simple

.PHONY: clean
clean:
	rm -f ./gbe ./tests *.o
