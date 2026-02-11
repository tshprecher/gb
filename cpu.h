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

enum Interrupt {
  VBLANK = 0,
  LCDC_STAT,
  TIMER_OVERFLOW,
  SERIAL_IO,
  PORT
};

struct interrupt_controller {
  uint8_t IF, IE;
};

void interrupt(struct interrupt_controller *, enum Interrupt);

struct cpu {
  // 8 bit registers
  uint8_t A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  uint16_t PC, SP;

  // interrupt master enable flag
  uint8_t IME;

  // controllers to interface with other hardware
  struct mem_controller *memory_c;
  struct interrupt_controller *interrupt_c;

  // the number of t_cycles since last instruction execution
  int8_t t_cycles;

  // the next instruction to execute
  struct inst *next_inst;

  // the number of t_cycles to count down after an interrupt
  int8_t interrupt_t_cycles;
};

void init_cpu(struct cpu *);

void cpu_tick(struct cpu *);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu *, struct inst *);

#endif
