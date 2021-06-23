.PHONY: all
all: gbe gb_asm

gbe: gbe.c inst cpu
	gcc -o gbe gbe.c decode.o write.o cpu.o

gb_asm: asm/main.c inst
	gcc -o gb_asm asm/main.c decode.o write.o

cpu:    cpu.c
	gcc -Wall -c cpu.c

inst:   inst/decode.c inst/write.c
	gcc -Wall -c inst/decode.c inst/write.c

clean:
	rm gb_asm *.o


