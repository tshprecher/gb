.PHONY: all
all: gbe tests

gbe:	inst cpu gbe.c
	gcc -Wall -o gbe gbe.c inst.o cpu.o

tests:  inst cpu
	gcc -Wall -o tests tests.c cpu.o inst.o

cpu:    cpu.c
	gcc -Wall -c cpu.c

inst:   inst/inst.c
	gcc -Wall -c inst/inst.c

clean:
	rm gbe *_test *.o
