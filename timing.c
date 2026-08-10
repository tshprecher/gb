#include <stdio.h>
#include "macros.h"
#include "timing.h"

static int clocks[] = {
  1024,
  16,
  64,
  256,
};

static inline u8 DIV(struct timing_controller *tc) {
  return (u8) ((tc->t_cycles >> 8) & 0xFF);
}

void timing_tick(struct timing_controller *tc) {
  tc->t_cycles++;

  if (b_is_off(tc->regs[rTAC], 2)) { // timer is off
    return;
  }

  if (tc->t_cycles % clocks[tc->regs[rTAC] & 3] == 0) {
    tc->regs[rTIMA]++;
    if (tc->regs[rTIMA] == 0x00) { // overflow
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
  tc->regs[reg] = value;
  switch (reg) {
  case rDIV:
    printf("debug (timing): wrote to DIV\n");
    tc->t_cycles = 0;
    break;
  case rTAC:
    if (b_is_on(tc->regs[rTAC], 2)) {
      // reset the timer when turning on
      tc->regs[rTIMA] = tc->regs[rTMA];
      tc->t_cycles = 0;
    }
    break;
  default:
    break;
  }
}
