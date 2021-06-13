.PHONY: all
all: gb_asm

# This rule tells make how to build hello from hello.cpp
gb_asm: main.c inst
	gcc -o gb_asm main.c decode.o write.o

inst:   inst/decode.c inst/write.c
	gcc -Wall -c inst/decode.c inst/write.c

clean:
	rm gb_asm *.o


