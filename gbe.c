#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "inst.h"
#include "cpu.h"

struct gb
{
  struct cpu *cpu;
  uint8_t ram[0x10000];

  int debug_mode;
};

void gb_debug_print_bin(char byte)
{
    printf("%d%d%d%d%d%d%d%d",
           (byte & (1 << 7)) != 0,
           (byte & (1 << 6)) != 0,
           (byte & (1 << 5)) != 0,
           (byte & (1 << 4)) != 0,
           (byte & (1 << 3)) != 0,
           (byte & (1 << 2)) != 0,
           (byte & (1 << 1)) != 0,
           (byte & 1) != 0);
}

// print the state of the machine for debugging
void gb_debug_print(struct gb *gb)
{
  // clear terminal
  printf("\033[2J");
  char buf[32];
  struct inst ins;
  char *pc = (char *) &gb->ram[gb->cpu->PC];

  printf("************* ROM DEBUGGER **************");
  printf("\n");
  printf("\n");
  printf("-----------------------------------------");
  printf("\n");

  init_inst_from_bytes(&ins, pc);
  inst_to_str(&ins, buf);
  printf("instruction (txt) :\t%s", buf);
  printf("\n");
  printf("instruction (hex) :\t");
  for (int i = 0; i < ins.bytelen; i++)
    {
      printf("0x%02X ", (uint8_t) pc[i]);
    }
  printf("\n");
  printf("instruction (bin) :\t");
  for (int i = 0; i < ins.bytelen; i++)
    {
      gb_debug_print_bin(pc[i]);
      printf(" ");
    }
  printf("\n\n");
  printf("FLAGS REGISTER:");
  printf("\n");
  printf("\tF: 0x%02X {Z: %d, N: %d, H: %d, CY: %d}",
	 gb->cpu->F,
	 cpu_flag(gb->cpu, FLAG_Z),
	 cpu_flag(gb->cpu, FLAG_N),
	 cpu_flag(gb->cpu, FLAG_H),
	 cpu_flag(gb->cpu, FLAG_CY));
  printf("\n\n");
  printf("16 BIT REGISTERS:");
  printf("\n\n");
  printf("\tPC: 0x%04X", gb->cpu->PC);
  printf("\n");
  printf("\tSP: 0x%04X", gb->cpu->SP);
  printf("\n\n");
  printf("8 BIT REGISTERS:");
  printf("\n\n");
  printf("\tA: 0x%02X", gb->cpu->A);
  printf("\n");
  printf("\tB: 0x%02X", gb->cpu->B);
  printf("\n");
  printf("\tC: 0x%02X", gb->cpu->C);
  printf("\n");
  printf("\tD: 0x%02X", gb->cpu->D);
  printf("\n");
  printf("\tE: 0x%02X", gb->cpu->E);
  printf("\n");
  printf("\tH: 0x%02X", gb->cpu->H);
  printf("\n");
  printf("\tL: 0x%02X", gb->cpu->L);
  printf("\n");
  printf("\n\n");

  printf("NEXT 10 LINES:");
  printf("\n\n");

  for (int a = 0, pc = gb->cpu->PC; a < 10; a++) {
    struct inst inst;
    if (!init_inst_from_bytes(&inst, &gb->ram[pc])) {
      fprintf(stderr, "error: could not decode instructions");
      exit(1);
    }
    inst_to_str(&inst, buf);
    if (a == 0)
      printf("0x%02X      --> %s", pc, buf);
    else
      printf("0x%02X          %s", pc, buf);
    printf("\n");
    pc+=ins.bytelen;
  }
  printf("-----------------------------------------");
  printf("\n");
  printf("\n\n");
}

ssize_t gb_dump(struct gb *gb) {
  FILE *dump;
  if ((dump = fopen("dump.bin", "wb")) == NULL) {
    return 0;
  }
  size_t result = fwrite(gb->ram, 1, 0x10000, dump);
  fclose(dump);
  return result;
}

void gb_run(struct gb *gb)
{
  char buf[16];
  if (gb->debug_mode)
    {
      gb_debug_print(gb);
      printf("debug > ");
      while (fgets(buf, sizeof(buf), stdin) != NULL) {
	if (buf[0] == 'n') {
	  struct inst inst = cpu_next_instruction(gb->cpu);
	  if (cpu_exec_instruction(gb->cpu, &inst) <= 0) {
	    printf("\n");
	    exit(1);
	  }
	} else if (buf[0] == 'r') {
	  long parsed_hex = strtol(buf+1, NULL, 16);

	  printf("parsed_hex = 0x%02lX -> 0x%02X\n", parsed_hex, gb->ram[parsed_hex]);
	  sleep(3);
	} else if (buf[0] == 's') {
	  //	  gb->ram[0xFF44] = 0x91; // for zelda
	  gb->ram[0xFF44] = 0x94; // for tetris
	} else if (buf[0] == 'd') {
	  // dump the ram contents
	  ssize_t result = gb_dump(gb);
	  if (result < 0x10000) {
	    fprintf(stderr, "error: could not write dump file: wrote %ld bytes out of %d", result, 0x10000);
	    sleep(1);
	  }
	} else {
	  fprintf(stderr, "error: unknown debugger command");
	  sleep(1);
	}
	gb_debug_print(gb);
	printf("debug > ");
      }
    }
  else {
    // run until error, dump core on error
    int inst_cnt = 0;
    char buf[32];
    // HACK: unblock the check for the LY register
    // gb->ram[0xFF44] = 0x91; // for zelda
    gb->ram[0xFF44] = 0x94; // for tetris
    int LIMIT = 60000;
    while (inst_cnt < LIMIT) {
      struct inst inst = cpu_next_instruction(gb->cpu);
      inst_to_str(&inst, buf);
      //      printf("ram value at 0x0000 -> 0x%02X\n", gb->ram[0]);
      printf("0x%04X\t%s\n", gb->cpu->PC, buf);
      if (cpu_exec_instruction(gb->cpu, &inst) <= 0) {
	inst_to_str(&inst, buf);
	fprintf(stderr, "error: instruction # %d could not execute '%s' @ 0x%04X\n", inst_cnt, buf, gb->cpu->PC);
	break;
      }
      inst_cnt++;
    }
    printf("ran %d instuctions\n", inst_cnt);
    if (gb_dump(gb) < LIMIT) {
      fprintf(stderr, "error: could not write dump file");
    }
  }
}


// load 32K ROM into starting address 0x150
void gb_load_rom(struct gb *gb, char *filename)
{
    FILE *fin = fopen(filename, "rb");
    if (NULL == fin)
    {
        fprintf(stderr, "error opening file: %s\n", filename);
        exit(1);
    }

    char b;
    int addr = 0;
    while (addr < 0x8000)
    {
        size_t c = fread(&b, 1, 1, fin);
        if (c == 0)
            break;
        gb->ram[addr] = b;
        addr++;
    }
    if (addr != 0x8000)
    {
        fprintf(stderr, "error reading ROM: cannot read full 32K of ROM: %x\n", addr);
        exit(1);
    }
    gb->cpu->PC = 0x150;
    gb->cpu->SP = 0xFFFE;

    fclose(fin);
}


// TODO: use getopt
int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "error: missing arguments.\n");
    return 1;
  } else if (argc == 2) { // run game with debugger
    struct cpu cpu = {0};
    struct gb gb = {0};
    cpu.ram = gb.ram;
    gb.cpu = &cpu;
    //    gb.debug_mode = 1;
    gb_load_rom(&gb, argv[1]);
    gb_run(&gb);
  } else if (argc == 3) { // disassemble
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }

    struct cpu cpu = {0};
    struct gb gb = {0};
    cpu.ram = gb.ram;
    gb.cpu = &cpu;
    gb_load_rom(&gb, argv[2]);

    int addr = 0;
    struct inst decoded = {0};
    char buf[16];
    while (addr < 0x8150) {
      int res = init_inst_from_bytes(&decoded, gb.ram + addr);
      if (res) {
	inst_to_str(&decoded, buf);
	printf("0x%02X\t%s\n", addr, buf);
	addr+=decoded.bytelen;
      } else {
	printf("0x%02X\tDB 0x%02X\n", addr, gb.ram[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
