#ifndef TIMING_H
#define TIMING_H

#include "cpu.h"
#include <stdint.h>

enum timing_reg {
  rDIV = 0, rTIMA, rTMA, rTAC
};

struct timing_controller {
  // registers
  u8 regs[4];
  u16 timer_t_cycles;
  uint32_t div_t_cycles; // for DIV psuedoregister

  struct interrupt_controller * interrupt_c;
};

void timing_tick(struct timing_controller *);

u8 timing_reg_read(struct timing_controller*, enum timing_reg);
void timing_reg_write(struct timing_controller*, enum timing_reg, u8);

#endif
