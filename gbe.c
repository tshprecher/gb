#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "sound.h"
#include "input.h"
#include "inst.h"
#include "cpu.h"
#include "memory.h"

#define CLOCK_FREQ 4194304

static void sleep_ns(int64_t ns) {
  const struct timespec ts = {.tv_nsec = ns};
  struct timespec rem;
  int res;
  if ((res = nanosleep(&ts, &rem))) {
    perror("nanosleep() failed");
    exit(1);
  }
}

static int64_t get_time_ns() {
  struct timespec ts = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    perror("clock_gettime() failed");
    exit(1);
  }
  return ts.tv_nsec;
}

struct gb
{
  struct cpu *cpu;
  struct mem_controller *memory_c;
  struct interrupt_controller *interrupt_c;
  struct lcd_controller *lcd_c;
  struct timing_controller *timing_c;
  struct input_controller *input_c;
  struct sound_controller *sound_c;
};

ssize_t gb_dump(struct gb *gb) {
  FILE *dump;
  if ((dump = fopen("dump.bin", "wb")) == NULL) {
    return 0;

  }
  size_t result = fwrite(gb->memory_c->ram, 1, 0x10000, dump);
  fclose(dump);
  return result;
}

void gb_run(struct gb *gb)
{
  init_lcd();
  init_sound();

  int t_cycles = 0;
  int64_t last_cycle_time_ns = get_time_ns();

  while (1) {
    t_cycles++;

    cpu_tick(gb->cpu);
    lcd_tick(gb->lcd_c);
    input_tick(gb->input_c);
    timing_tick(gb->timing_c);
    sound_tick(gb->sound_c);
    if (t_cycles % ((1<<16) + 1) == 0) {
      int64_t cur_cycle_time_ns = get_time_ns();
      int64_t cur_period_time_ns;
      if (cur_cycle_time_ns > last_cycle_time_ns) {
	 cur_period_time_ns = cur_cycle_time_ns - last_cycle_time_ns;
      } else {
	cur_period_time_ns = 999999999 - last_cycle_time_ns + cur_cycle_time_ns;
      }

      last_cycle_time_ns = cur_cycle_time_ns;
      int sleep_time_ns = 15625000 > cur_period_time_ns ? (15625000 - cur_period_time_ns) : 0;
      sleep_ns(sleep_time_ns);
    }
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
    struct mem_controller memory_c = {0};
    struct input_controller input_c = {0};
    struct interrupt_controller interrupt_c = {0};
    struct lcd_controller lcd_c = {0};
    struct timing_controller timing_c = {0};
    struct sound_controller sound_c = {0};

    lcd_c.memory_c = &memory_c;
    lcd_c.interrupt_c = &interrupt_c;
    lcd_c.regs[rLCDC] = 0x83; // TODO: put in an init?

    memory_c.interrupt_c = &interrupt_c;
    memory_c.lcd_c = &lcd_c;
    memory_c.timing_c = &timing_c;
    memory_c.input_c = &input_c;
    memory_c.sound_c = &sound_c;

    cpu.memory_c = &memory_c;
    cpu.interrupt_c = &interrupt_c;

    sound_c.memory_c = &memory_c;

    gb.cpu = &cpu;
    gb.memory_c = &memory_c;
    gb.lcd_c = &lcd_c;
    gb.timing_c = &timing_c;
    gb.input_c = &input_c;
    gb.sound_c = &sound_c;

    load_rom(gb.memory_c, argv[1]);
    gb_run(&gb);
  } else if (argc == 3) { // disassemble
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }

    struct mem_controller memory_c = {0};
    load_rom(&memory_c, argv[2]);

    int addr = 0x0;
    struct inst *decoded;
    char buf[16];
    while (addr < 0x8000) {
      decoded = mem_read_inst(&memory_c, addr);
      if (decoded) {
	inst_to_str(buf, decoded);
	printf("0x%02X\t%s\n", addr, buf);
	addr+=decoded->bytelen;
      } else {
	printf("0x%02X\tDB 0x%02X\n", addr, memory_c.ram[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
