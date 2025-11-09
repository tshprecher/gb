.PHONY: all
all: gbe

gbe: inst cpu gbe.c
	gcc -Wall -o gbe gbe.c inst.o cpu.o

cpu:    cpu.c
	gcc -Wall -c cpu.c

inst:   inst/inst.c
	gcc -Wall -c inst/inst.c

clean:
	rm gbe *.o
