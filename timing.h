#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

#include "cpu.h"
#include "types.h"

enum timing_reg { rDIV = 0, rTIMA, rTMA, rTAC };

struct timing_controller {
  u8 regs[4];
  u32 t_cycles;

  struct interrupt_controller* interrupt_c;
};

void init_timing_controller(struct timing_controller*,
                            struct interrupt_controller*);

void timing_tick(struct timing_controller*);
u8 timing_read_reg(struct timing_controller*, enum timing_reg);
void timing_write_reg(struct timing_controller*, enum timing_reg, u8);

#endif
