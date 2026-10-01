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
#define BILLION 1000000000

static s64 get_time() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    perror("error: clock_gettime() failed");
    exit(1);
  }
  return (ts.tv_sec * BILLION) + ts.tv_nsec;
}

struct clock {
  int freq;
  int ticks_per_period;
  int ticks_remaining;

  s64 period_ns;
  s64 period_start_time_ns;
};

static void init_clock(struct clock *c, int freq, int freq_slices) {
  c->freq = freq;
  c->ticks_per_period = freq / freq_slices;
  c->period_ns = BILLION / freq * c->ticks_per_period;
  c->ticks_remaining = c->ticks_per_period;
  c->period_start_time_ns = get_time();
}

static inline void clock_tick(struct clock *c) {
  c->ticks_remaining--;
  if (!c->ticks_remaining) {
    s64 runtime_ns = get_time() - c->period_start_time_ns;
    if (c->period_ns > runtime_ns) {
      s64 wait_time_ns = c->period_ns - runtime_ns;
      struct timespec ts = {.tv_nsec = .9*wait_time_ns};
      struct timespec rem;
      if (nanosleep(&ts, &rem)) {
       perror("error: clock failed sleep");
       exit(1);
      }
      s64 awake_time_ns = c->period_start_time_ns + runtime_ns + wait_time_ns;
      while (get_time() < awake_time_ns) {} // naive polling
    } else {
      // increase the period and ticks by 1/16
      c->period_ns += (c->period_ns >> 4);
      c->ticks_per_period += (c->ticks_per_period >> 4);
    }
    c->ticks_remaining = c->ticks_per_period;
    c->period_start_time_ns = get_time();
  }
};

struct gb
{
  struct clock *clock;
  struct cpu *cpu;
  struct mem_controller *memory_c;
  struct interrupt_controller *interrupt_c;
  struct lcd_controller *lcd_c;
  struct timing_controller *timing_c;
  struct input_controller *input_c;
  struct sound_controller *sound_c;
};

void gb_run(struct gb *gb)
{
  init_lcd();
  init_sound();

  while (1) {
    clock_tick(gb->clock);

    cpu_tick(gb->cpu);
    lcd_tick(gb->lcd_c);
    input_tick(gb->input_c);
    timing_tick(gb->timing_c);
    //    sound_tick(gb->sound_c);
  }
}

// TODO: use getopt
int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "error: missing arguments.\n");
    return 1;
  } else if (argc == 2) { // run game
    struct gamepak gpk = {0};
    init_gamepak(&gpk, argv[1]);

    struct cpu cpu = {0};
    init_cpu(&cpu);

    struct gb gb = {0};
    struct clock clock = {0};
    struct mem_controller memory_c = {0};
    struct input_controller input_c = {0};
    struct interrupt_controller interrupt_c = {0};
    struct lcd_controller lcd_c = {0};
    struct timing_controller timing_c = {0};
    struct sound_controller sound_c = {0};

    init_clock(&clock, CLOCK_FREQ, 128);

    input_c.interrupt_c = &interrupt_c;

    lcd_c.interrupt_c = &interrupt_c;
    lcd_c.regs[rLCDC] = 0x83; // TODO: put in an init?

    memory_c.gpk = &gpk;
    memory_c.interrupt_c = &interrupt_c;
    memory_c.lcd_c = &lcd_c;
    memory_c.timing_c = &timing_c;
    memory_c.input_c = &input_c;
    memory_c.sound_c = &sound_c;

    cpu.memory_c = &memory_c;
    cpu.interrupt_c = &interrupt_c;

    timing_c.interrupt_c = &interrupt_c;

    gb.clock = &clock;
    gb.cpu = &cpu;
    gb.memory_c = &memory_c;
    gb.lcd_c = &lcd_c;
    gb.timing_c = &timing_c;
    gb.input_c = &input_c;
    gb.sound_c = &sound_c;

    gb_run(&gb);
  } else if (argc == 3) { // disassemble, TODO: handle MBCs
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }
    struct gamepak gpk = {0};
    init_gamepak(&gpk, argv[2]);

    int addr = 0x100;
    struct inst decoded;
    char buf[16];
    while (addr < 0x8000) {
      int ok = init_inst_from_bytes(&decoded, &gpk.rom[addr]);
      if (ok) {
	inst_to_str(&decoded, buf);
	printf("0x%02X\t%s\n", addr, buf);
	addr+=decoded.bytelen;
      } else {
	printf("0x%02X\tDB 0x%02X\n", addr, gpk.rom[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
