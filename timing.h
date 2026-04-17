#ifndef TIMING_H
#define TIMING_H

#include "cpu.h"
#include "types.h"
#include <stdint.h>

enum timing_reg {
  rDIV = 0, rTIMA, rTMA, rTAC
};

struct timing_controller {
  u8 regs[4];
  u32 t_cycles;

  struct interrupt_controller * interrupt_c;
};

void timing_tick(struct timing_controller *);

u8 timing_reg_read(struct timing_controller*, enum timing_reg);
void timing_reg_write(struct timing_controller*, enum timing_reg, u8);

#endif
