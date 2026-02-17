#include "timing.h"
#include <stdio.h>

static uint16_t clocks[] = { 1<<9, 1<<3, 1<<5, 1<<7 };

void timing_tick(struct timing_controller *tc) {
  tc->div_t_cycles++;

  if (!(tc->regs[rTAC] & 4)) { // timer is off
    return;
  }

  printf("DEBUG: timer is on!\n");
  tc->timer_t_cycles++;
  // TODO: check overflow
  if (tc->timer_t_cycles == clocks[tc->regs[rTAC] & 3]) {
    tc->regs[rTIMA]++;
    if (tc->regs[rTIMA] == 0x00) { // overflow
      tc->regs[rTIMA] = tc->regs[rTMA];
    }
    tc->timer_t_cycles = 0;
    interrupt(tc->interrupt_c, TIMER_OVERFLOW);
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
