#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#include "inst.h"
#include "types.h"

#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_CY 4

enum interrupt {
  INT_VBLANK = 0,
  INT_LCDC_STAT,
  INT_TIMER_OVERFLOW,
  INT_SERIAL_IO,
  INT_PORT
};

struct interrupt_controller {
  u8 IF, IE;
};

enum cpu_state { CPU_STATE_RUNNING, CPU_STATE_HALTED, CPU_STATE_STOPPED };

void interrupt(struct interrupt_controller*, enum interrupt);

struct cpu {
  enum cpu_state state;

  // 8 bit registers
  u8 A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  u16 PC, SP;

  // interrupt master enable flag
  u8 IME;
  u8 enable_interrupt_after_next_inst;

  // controllers to interface with other hardware
  struct interrupt_controller* interrupt_c;
  struct mem_controller* memory_c;

  // the number of t_cycles since last instruction execution
  s16 t_cycles_since_last_inst;

  // the next instruction to execute
  struct inst* next_inst;

  // the number of t_cycles to count down after an interrupt
  // TODO: put in the interrupt controller?
  s8 interrupt_t_cycles;
};

void init_cpu(struct cpu*, struct interrupt_controller*,
              struct mem_controller*);
void cpu_tick(struct cpu*);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu*, struct inst*);

#endif
