.PHONY: all
all: gbe tests

gbe:	cpu gbe.c
	gcc -g -O0 -Wall -o gbe gbe.c inst.o cpu.o memory.o display.o sound.o -lX11 -lpulse -lpulse-simple

tests:  inst cpu
	gcc -g -O0 -Wall -o tests tests.c cpu.o inst.o memory.o display.o sound.o -lX11 -lpulse -lpulse-simple

cpu:    inst cpu.c
	gcc -g -O0 -Wall -c cpu.c memory.c

inst:   inst/inst.c memory.c display.c sound.c
	gcc -g -O0 -Wall -c inst/inst.c memory.c display.c sound.c

clean:
	rm ./gbe *_test *.o
