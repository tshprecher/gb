#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "inst.h"
#include "cpu.h"

#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_CYAN "\x1B[36m"
#define TERM_COLOR_RESET "\033[0m"

struct gb
{
    struct cpu *cpu;
    char ram[0x10000];
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
  if (argc != 3  || strcmp(argv[1], "-d") != 0 )  {
    fprintf(stderr, "Error: can only run with arg -d \n");
    exit(1);
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
      printf("DB %x\n", gb.ram[addr]);
      addr++;
    }
  }


  return 0;
}
