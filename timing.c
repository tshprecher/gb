#include "timing.h"
#include <stdio.h>


#define MAIN_CLOCK_FREQ 4194304

static int clocks[] = {
  MAIN_CLOCK_FREQ >> 10,
  MAIN_CLOCK_FREQ >> 4,
  MAIN_CLOCK_FREQ >> 6,
  MAIN_CLOCK_FREQ >> 8
};

void timing_tick(struct timing_controller *tc) {
  tc->div_t_cycles++;

  if (!(tc->regs[rTAC] & 4)) { // timer is off
    return;
  }

  tc->timer_t_cycles++;
  if (tc->timer_t_cycles == clocks[tc->regs[rTAC] & 3]) {
    tc->regs[rTIMA]++;
    printf("debug: timing TIMA incremented to %d, freq: %d\n", tc->regs[rTIMA], clocks[tc->regs[rTAC] & 3]);
    if (tc->regs[rTIMA] == 0x00) { // overflow
      printf("debug: timing overflow!\n");
      tc->regs[rTIMA] = tc->regs[rTMA];
      tc->timer_t_cycles = 0;
      interrupt(tc->interrupt_c, TIMER_OVERFLOW);
    }
  }
}

uint8_t timing_reg_read(struct timing_controller* tc, enum timing_reg reg) {
  switch (reg) {
  case rDIV:
    return (uint8_t) ((tc->div_t_cycles >> 9) & 0xFF);
  default:
    return tc->regs[reg];
  }
}

void timing_reg_write(struct timing_controller* tc, enum timing_reg reg, uint8_t value) {
  switch (reg) {
  case rDIV:
    tc->div_t_cycles = 0;
    break;
  default:
    tc->regs[reg] = value;
    break;
  }
}
