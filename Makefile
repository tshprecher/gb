.PHONY: all
all: gbe tests

gbe:	inst cpu gbe.c
	gcc -Wall -o gbe gbe.c inst.o cpu.o

tests:  inst
	gcc -Wall -o inst_test inst_test.c inst.o

cpu:    cpu.c
	gcc -Wall -c cpu.c

inst:   inst/inst.c
	gcc -Wall -c inst/inst.c

clean:
	rm gbe inst_test *.o
