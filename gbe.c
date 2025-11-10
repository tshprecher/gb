#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "inst.h"
#include "cpu.h"

#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_CYAN "\x1B[36m"
#define TERM_COLOR_RESET "\033[0m"

struct debugger {
    struct inst debug_last_inst[5];
    int16_t debug_last_inst_addr[5];
    int debug_last_pointer;
};

struct gb
{
  struct cpu *cpu;
  uint8_t ram[0x10000];

  // initialize to run with debugger
  struct debugger *dbg;
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

void gb_debug_print_diff_int(int is_diff, int num)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("%d", num);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("%d", num);
    }
}

void gb_debug_print_diff_16_bit(int is_diff, int value)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("0x%04X", value & 0xFFFF);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("0x%04X", value & 0xFFFF);
    }
}

void gb_debug_print_diff_8_bit(int is_diff, int value)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("0x%02X", value & 0xFF);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("0x%02X", value & 0xFF);
    }
}

void gb_debug_print(struct gb *gb)
{
    char buf[32];
    struct inst ins;
    char *pc = (char *) &gb->ram[gb->cpu->PC];

    init_inst_from_bytes(&ins, pc);
    inst_to_str(&ins, buf);

    printf("instruction (txt) :\t%s\n", buf);
    printf("instruction (hex) :\t");
    for (int i = 0; i < ins.bytelen; i++)
    {
      printf("0x%2X ", (uint8_t) pc[i]);
    }
    printf("\n");
    printf("instruction (bin) :\t");
    for (int i = 0; i < ins.bytelen; i++)
    {
      gb_debug_print_bin(pc[i]);
      printf(" ");
    }

    printf("\n\n");

    printf("\t -----------------------------------------      Z:   ");
    gb_debug_print_diff_int(0, gb->cpu->Z);
    printf("\n");
    printf("\t|                    PC                   |     N:   ");
    gb_debug_print_diff_int(0, gb->cpu->N);
    printf("\n");
    printf("\t|-----------------------------------------|     H:   ");
    gb_debug_print_diff_int(0, gb->cpu->H);
    printf("\n");
    printf("\t|                  ");
    gb_debug_print_diff_16_bit(0, gb->cpu->PC);
    printf("                 |     CY:  ");
    gb_debug_print_diff_int(0, gb->cpu->CY);
    printf("\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|                    SP                   |\n");
    printf("\t|-----------------------------------------|   Code:\n");
    printf("\t|                  ");
    gb_debug_print_diff_16_bit(0, gb->cpu->SP);
    printf("                 |");
    //    gb_debug_print_last_inst(gb, 4);
    printf("\n");

    printf("\t|-----------------------------------------|");
    //    gb_debug_print_last_inst(gb, 3);
    printf("\n");
    printf("\t|         A          |          F         |");
    //    gb_debug_print_last_inst(gb, 2);
    printf("\n");
    printf("\t|--------------------|--------------------|");
    printf("\n");
    //    gb_debug_print_last_inst(gb, 1);
    printf("\t|       ");

    gb_debug_print_diff_8_bit(0, gb->cpu->A);
    printf("         |        ");
    gb_debug_print_diff_8_bit(0, gb->cpu->F);
    printf("        |");
    //    gb_debug_print_last_inst(gb, 0);
    printf("\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|         B          |          C         |");
    //    gb_debug_print_next_inst(gb, 1);
    printf("\n");
    printf("\t|--------------------|--------------------|");
    //    gb_debug_print_next_inst(gb, 2);
    printf("\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(0, gb->cpu->B);
    printf("         |        ");
    gb_debug_print_diff_8_bit(0, gb->cpu->C);
    printf("        |");
    //    gb_debug_print_next_inst(gb, 3);
    printf("\n");
    printf("\t|-----------------------------------------|");
    //    gb_debug_print_next_inst(gb, 4);
    printf("\n");
    printf("\t|         D          |          E         |");
    //    gb_debug_print_next_inst(gb, 5);
    printf("\n");
    printf("\t|--------------------|--------------------|\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(0, gb->cpu->D);
    printf("         |        ");
    gb_debug_print_diff_8_bit(0, gb->cpu->E);
    printf("        |\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|         H          |          L         |\n");
    printf("\t|--------------------|--------------------|\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(0, gb->cpu->H);
    printf("         |        ");
    gb_debug_print_diff_8_bit(0, gb->cpu->L);
    printf("        |\n");

    printf("\t ----------------------------------------- \n");
}

void gb_run(struct gb *gb)
{
    if (gb->dbg != NULL)
    {
      gb_debug_print(gb);
      /*        // always run the first instruction
        if (!gb_debug_exec_inst(gb))
        {
            printf("error executing instruction\n");
            exit(1);
        }
        while (1)
        {
            gb_debug_break(gb);
            exit(0);
	    }*/
    }
    else
    {
      // TODO: remove this, obviously
      fprintf(stderr, "Error: can only run in debug mode for now\n");
      exit(1);
    }
}


// load 32K ROM into starting address 0x150
void gb_load_rom(struct gb *gb, char *filename)
{
    FILE *fin = fopen(filename, "rb");

    // TODO: this error handling looks wrong
    // TODO: perhaps make reading roms common in some library?
    if (NULL == fin)
    {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }

    char b;
    int addr = 0;
    while (addr < 0x8150)
    {
        size_t c = fread(&b, 1, 1, fin);
        if (c == 0)
        {
            break;
        }
        if (addr < 0x150)
        {
            addr++;
            continue;
        }
        gb->ram[addr] = b;
        addr++;
    }

    if (addr < 0x150)
    {
        fprintf(stderr, "Error reading ROM: cannot find code starting at addresss 0x150: %x\n", addr);
        exit(1);
    }
    if (addr != 0x8150)
    {
        fprintf(stderr, "Error reading ROM: cannot read full 32K of ROM: %x\n", addr);
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
  } else if (argc == 2) {
    struct cpu cpu = {0};
    struct gb gb = {0};
    struct debugger dbg = {0};
    cpu.ram = gb.ram;
    gb.cpu = &cpu;
    gb.dbg = &dbg;
    gb_load_rom(&gb, argv[1]);
    gb_run(&gb);
  } else if (argc == 3) {
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }

    struct cpu cpu = {0};
    struct gb gb = {0};
    cpu.ram = gb.ram;
    gb.cpu = &cpu;
    gb_load_rom(&gb, argv[2]);

    int addr = 0x150;
    struct inst decoded = {0};
    char buf[16];
    while (addr < 0x8150) {
      int res = init_inst_from_bytes(&decoded, gb.ram + addr);
      if (res) {
	inst_to_str(&decoded, buf);
	printf("%s\n", buf);
	addr+=decoded.bytelen;
      } else {
	printf("DB 0x%02X\n", gb.ram[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
