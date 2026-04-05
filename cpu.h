#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "types.h"
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
  u8 IF, IE;
};

void interrupt(struct interrupt_controller *, enum Interrupt);

struct cpu {
  // 8 bit registers
  u8 A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  u16 PC, SP;

  // flag for halted
  u8 is_halted;

  // interrupt master enable flag
  u8 IME;

  // controllers to interface with other hardware
  struct mem_controller *memory_c;
  struct interrupt_controller *interrupt_c;

  // the number of t_cycles since last instruction execution
  s8 t_cycles_since_last_inst;

  // the next instruction to execute
  struct inst *next_inst;

  // the number of t_cycles to count down after an interrupt
  s8 interrupt_t_cycles;
};

void init_cpu(struct cpu *);

void cpu_tick(struct cpu *);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu *, struct inst *);

#endif
