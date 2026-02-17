#ifndef TIMING_H
#define TIMING_H

#include "cpu.h"
#include <stdint.h>

enum timing_reg {
  rDIV = 0, rTIMA, rTMA, rTAC
};

struct timing_controller {
  // registers
  uint8_t regs[4];
  uint16_t timer_t_cycles;
  uint32_t div_t_cycles; // for DIV psuedoregister

  struct interrupt_controller * interrupt_c;
};

void timing_tick(struct timing_controller *);

uint8_t timing_reg_read(struct timing_controller*, enum timing_reg);
void timing_reg_write(struct timing_controller*, enum timing_reg, uint8_t);

#endif
