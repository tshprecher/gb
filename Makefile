.PHONY: all
all: gb_asm

gb_asm: asm/main.c inst
	gcc -o gb_asm asm/main.c decode.o write.o

inst:   inst/decode.c inst/write.c
	gcc -Wall -c inst/decode.c inst/write.c

clean:
	rm gb_asm *.o


