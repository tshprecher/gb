#include "timing.h"
#include <stdio.h>


#define MAIN_CLOCK_FREQ 4194304

static int clocks[] = {
  1 << 9,
  1 << 3,
  1 << 5,
  1 << 7
};

static inline u8 DIV(struct timing_controller *tc) {
  // TODO: get this from a macro
  return (u8) ((tc->t_cycles >> 8) & 0xFF);
}

void timing_tick(struct timing_controller *tc) {
  tc->t_cycles++;

  if (!(tc->regs[rTAC] & 4)) { // timer is off
    return;
  }

  printf("DEBUG (timing): incremented timer to %d\n", DIV(tc));

  if (tc->t_cycles % clocks[tc->regs[rTAC] & 3] == 0) {
    tc->regs[rTIMA]++;
    printf("debug: timing TIMA incremented to %d, freq: %d\n", tc->regs[rTIMA], clocks[tc->regs[rTAC] & 3]);
    if (tc->regs[rTIMA] == 0x00) { // overflow
      printf("debug: timing overflow!\n");
      tc->regs[rTIMA] = tc->regs[rTMA];
      tc->t_cycles = 0;
      interrupt(tc->interrupt_c, TIMER_OVERFLOW);
    }
  }
}

u8 timing_reg_read(struct timing_controller* tc, enum timing_reg reg) {
  switch (reg) {
  case rDIV:
    return DIV(tc);
  default:
    return tc->regs[reg];
  }
}

void timing_reg_write(struct timing_controller* tc, enum timing_reg reg, u8 value) {
  printf("DEBUG (timing): writing 0x%02X to timing reg %d\n", value, reg);
  tc->regs[reg] = value;
  switch (reg) {
  case rDIV:
    tc->t_cycles = 0;
    break;
  case rTAC:
    if (tc->regs[rTAC] & 4) {
      // reset the timer when turning on
      tc->regs[rTIMA] = tc->regs[rTMA];
      tc->t_cycles = 0;
    }
    break;
  }
}
