#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "inst.h"
#include "cpu.h"
#include "mem_controller.h"

// sleep at after every frame refresh at 59.7Hz, the inverse of which
// approximates to 16750418 nanoseconds.
// TOOD: handle the fractional nanoseconds for better
// precision, dynamically set the period based on work done.
static const struct timespec tick_timespec = {tv_nsec: 16750418};

static void sleep_frame() {
  // TODO: handle error case, remain
  int res = clock_nanosleep(CLOCK_MONOTONIC, 0, &tick_timespec, NULL);
  if (res) {
    printf("DEBUG: error in sleep: %d\n", res);
  }
}

struct gb
{
  struct cpu *cpu;
  struct mem_controller *mc;
  struct interrupt_controller *ic;
  struct lcd_controller *lcd;
  struct timer_controller *tc;
};

ssize_t gb_dump(struct gb *gb) {
  FILE *dump;
  if ((dump = fopen("dump.bin", "wb")) == NULL) {
    return 0;

  }
  size_t result = fwrite(gb->mc->ram, 1, 0x10000, dump);
  printf("DEBUG: dumped total %ld bytes\n", result);
  fclose(dump);
  return result;
}

static void DEBUG_cpu_to_str(char *buf, struct cpu *cpu) {
  int pos = sprintf(buf, "{A: 0x%02X,B: 0x%02X,", cpu->A, cpu->B);
  pos += sprintf(buf+pos, "C: 0x%02X,D: 0x%02X,", cpu->C, cpu->D);
  pos += sprintf(buf+pos, "E: 0x%02X,F: 0x%02X,", cpu->E, cpu->F);
  pos += sprintf(buf+pos, "H: 0x%02X,L: 0x%02X,", cpu->H, cpu->L);
  sprintf(buf+pos, "PC: 0x%04X,SP: 0x%04X}", cpu->PC, cpu->SP);
}

void gb_run(struct gb *gb)
{
  init_lcd();

  // run until error, dump core on error
  int t_cycles = 0;
  int LIMIT = 4194304 * 30;
  //int LIMIT = 800000;
  while (t_cycles < LIMIT) {
    cpu_tick(gb->cpu);
    lcd_tick(gb->lcd);
    t_cycles++;
    /*    if (t_cycles % 4194304 == 0) {
      sleep(1);
      }*/
  }
  printf("ran %d clock cycles\n", t_cycles);
  sleep(5);
  if (gb_dump(gb) < LIMIT) {
    fprintf(stderr, "error: could not write dump file");
  }
}


// TODO: make this a method of memory controller
void load_rom(struct mem_controller *mc, char *filename)
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
	mc->ram[addr++] = b;
    }
    if (addr != 0x8000)
    {
        fprintf(stderr, "error reading ROM: cannot read full 32K of ROM: %x\n", addr);
        exit(1);
    }
    fclose(fin);
}


// TODO: use getopt
int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "error: missing arguments.\n");
    return 1;
  } else if (argc == 2) { // run game
    struct cpu cpu = {0};
    init_cpu(&cpu);

    struct gb gb = {0};
    struct mem_controller mc = {0};
    struct interrupt_controller ic = {0};
    struct lcd_controller lcd = {0};
    struct timer_controller tc = {0};

    lcd.mc = &mc;
    lcd.ic = &ic;
    lcd.LCDC = 0x83; // TODO: put in an init?

    mc.ic = &ic;
    mc.lcd = &lcd;
    mc.tc = &tc;

    cpu.mc = &mc;
    cpu.ic = &ic;

    gb.cpu = &cpu;
    gb.mc = &mc;
    gb.lcd = &lcd;
    gb.tc = &tc;

    load_rom(gb.mc, argv[1]);
    gb_run(&gb);
  } else if (argc == 3) { // disassemble
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }

    struct mem_controller mc = {0};
    load_rom(&mc, argv[2]);

    int addr = 0x0;
    struct inst *decoded;
    char buf[16];
    while (addr < 0x8000) {
      decoded = mem_read_inst(&mc, addr);
      if (decoded) {
	inst_to_str(buf, decoded);
	printf("0x%02X\t%s\n", addr, buf);
	addr+=decoded->bytelen;
      } else {
	printf("0x%02X\tDB 0x%02X\n", addr, mc.ram[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
