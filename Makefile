.PHONY: all
all: gbe test_inst test_cpu

gbe:	inst cpu gbe.c
	gcc -Wall -o gbe gbe.c inst.o cpu.o

test_inst:  inst
	gcc -Wall -o inst_test inst_test.c inst.o

test_cpu:  inst cpu
	gcc -Wall -o cpu_test cpu_test.c inst.o cpu.o

cpu:    cpu.c
	gcc -Wall -c cpu.c

inst:   inst/inst.c
	gcc -Wall -c inst/inst.c

clean:
	rm gbe *_test *.o
