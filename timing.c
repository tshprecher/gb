#include "timing.h"

void timing_tick(struct timing_controller *tc) {
  tc->t_cycles++;
}

uint8_t timing_reg_read(struct timing_controller* tc, enum timing_reg reg) {
  switch (reg) {
  case rDIV:
    return (uint8_t) ((tc->t_cycles >> 9) & 0xFF);
  default:
    return tc->regs[reg];
  }
}

void timing_reg_write(struct timing_controller* tc, enum timing_reg reg, uint8_t value) {
  switch (reg) {
  case rDIV:
    tc->t_cycles = 0;
    break;
  default:
    tc->regs[reg] = value;
    break;
  }
}
