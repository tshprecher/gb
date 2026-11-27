#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cpu.h"
#include "input.h"
#include "inst.h"
#include "macros.h"
#include "memory.h"
#include "sound.h"

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

static void init_clock(struct clock* c, int freq, int freq_slices) {
  c->freq = freq;
  c->ticks_per_period = freq / freq_slices;
  c->period_ns = BILLION / freq * c->ticks_per_period;
  c->ticks_remaining = c->ticks_per_period;
  c->period_start_time_ns = get_time();
}

static inline void clock_tick(struct clock* c) {
  c->ticks_remaining--;
  if (!c->ticks_remaining) {
    s64 runtime_ns = get_time() - c->period_start_time_ns;
    if (c->period_ns > runtime_ns) {
      s64 wait_time_ns = c->period_ns - runtime_ns;
      struct timespec ts = {.tv_nsec = .8 * wait_time_ns};
      struct timespec rem;
      if (nanosleep(&ts, &rem)) {
        perror("error: clock failed sleep");
        exit(1);
      }
      s64 awake_time_ns = c->period_start_time_ns + runtime_ns + wait_time_ns;
      while (get_time() < awake_time_ns) {
      }  // naive polling
    } else {
      // increase the period and ticks by 1/16
      c->period_ns += (c->period_ns >> 4);
      c->ticks_per_period += (c->ticks_per_period >> 4);
    }
    c->ticks_remaining = c->ticks_per_period;
    c->period_start_time_ns = get_time();
  }
};

struct gb {
  struct clock* clock;
  struct cpu* cpu;
  struct input_controller* input_c;
  struct lcd_controller* lcd_c;
  struct mem_controller* memory_c;
  struct sound_controller* sound_c;
  struct timing_controller* timing_c;
};

void init_gb(struct gb* gb, struct clock* clk, struct cpu* cpu,
             struct input_controller* inp_c, struct lcd_controller* lc,
             struct sound_controller* sc, struct timing_controller* tc) {
  gb->clock = clk;
  gb->cpu = cpu;
  gb->lcd_c = lc;
  gb->timing_c = tc;
  gb->input_c = inp_c;
  gb->sound_c = sc;
}

void gb_run(struct gb* gb) {
  while (1) {
    clock_tick(gb->clock);
    input_tick(gb->input_c);
    cpu_tick(gb->cpu);  // handles STOPPED and HALTED states internally
    sound_tick(gb->sound_c);

    if (gb->cpu->state != CPU_STATE_STOPPED) {
      lcd_tick(gb->lcd_c);
      timing_tick(gb->timing_c);
    }
  }
}

// TODO: use getopt
int main(int argc, char* argv[]) {
  global_init_lcd();
  global_init_sound();

  if (argc < 2) {
    fprintf(stderr, "error: missing arguments.\n");
    return 1;
  } else if (argc == 2) {  // run game
    struct gamepak gpk = {0};
    init_gamepak(&gpk, argv[1]);

    struct clock clock = {0};
    init_clock(&clock, CLOCK_FREQ, 256);

    struct interrupt_controller interrupt_c = {0};
    struct input_controller input_c = {0};
    init_input_controller(&input_c, &interrupt_c);

    struct timing_controller timing_c = {0};
    init_timing_controller(&timing_c, &interrupt_c);

    struct lcd_controller lcd_c = {0};
    init_lcd_controller(&lcd_c, &interrupt_c);

    struct sound_controller sound_c = {0};
    init_sound(&sound_c);

    struct mem_controller memory_c = {0};
    init_mem_controller(&memory_c, &gpk, &input_c, &interrupt_c, &lcd_c,
                        &sound_c, &timing_c);

    struct cpu cpu = {0};
    init_cpu(&cpu, &interrupt_c, &memory_c);

    struct gb gb = {0};
    init_gb(&gb, &clock, &cpu, &input_c, &lcd_c, &sound_c, &timing_c);

    gb_run(&gb);
  } else if (argc == 3) {  // disassemble, TODO: handle MBCs
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
        addr += decoded.bytelen;
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
