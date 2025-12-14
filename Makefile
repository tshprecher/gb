.PHONY: all
all: gbe tests

gbe:	inst cpu gbe.c
	gcc -Wall -o gbe gbe.c inst.o cpu.o mem_controller.o

tests:  inst cpu
	gcc -Wall -o tests tests.c cpu.o inst.o mem_controller.o

cpu:    cpu.c mem_controller.c
	gcc -Wall -c cpu.c mem_controller.c

inst:   inst/inst.c
	gcc -Wall -c inst/inst.c

clean:
	rm gbe *_test *.o
