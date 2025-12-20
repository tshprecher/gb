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


struct interrupt_controller {
  uint8_t IF, IE;
};

void interrupt_vblank(struct interrupt_controller *ic);


struct timer_controller {
  uint32_t div_t_cycles;

  // divider register
  uint8_t DIV;

  // timer registers
  uint8_t TIMA, TMA, TAC;
};

void timer_tick(struct timer_controller *tc);


// TODO: probably should have debugger support injected here
struct cpu {
  // 8 bit registers
  uint8_t A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  uint16_t PC, SP;

  // memory controller monitors access to ram
  struct mem_controller *mc;

  struct interrupt_controller *ic;

  // interrupt master enable flag
  uint8_t IME;

  // the number of t_cycles since last instruction execution
  int8_t t_cycles;

  // the next instruction to execute
  struct inst *next_inst;
};

void init_cpu(struct cpu *);

void cpu_tick(struct cpu *);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu *, struct inst *);

#endif
