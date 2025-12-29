.PHONY: all
all: gbe tests

gbe:	cpu gbe.c
	gcc -g -O0 -Wall -o gbe gbe.c inst.o cpu.o mem_controller.o lcd.o -lX11

tests:  inst cpu
	gcc -g -O0 -Wall -o tests tests.c cpu.o inst.o mem_controller.o lcd.o -lX11

cpu:    inst cpu.c
	gcc -g -O0 -Wall -c cpu.c mem_controller.c

inst:   inst/inst.c mem_controller.c lcd.c
	gcc -g -O0 -Wall -c inst/inst.c mem_controller.c lcd.c

clean:
	rm ./gbe *_test *.o
