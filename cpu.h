#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "inst.h"

#define cpu_flag(c, f) ((c->F>>f) & 1)
#define cpu_set_flag(c, f) (c->F |= (1 << f))
#define cpu_clear_flag(c, f) (c->F &= ~(1 << f))
#define cpu_flip_flag(c, f) (cpu_flag(c, f) ? cpu_clear_flag(c, f) : cpu_set_flag(c,f))

#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_CY 4


// TODO: probably should have debugger support injected here
struct cpu {
  // 8 bit registers
  uint8_t A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  uint16_t PC, SP;

  // memory controller monitors access to ram
  struct mem_controller *mc;

  // interrupt master enable flag
  uint8_t IME;
};

// execute a given number of cycles, return number of cycles run
// or -1 on error.
int cpu_exec_cycles(struct cpu *, unsigned int);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu *, struct inst *);

#endif
