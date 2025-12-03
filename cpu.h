#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define cpu_flag(c, f) ((c->F>>f) & 1)
#define cpu_set_flag(c, f) (c->F |= (1 << f))
#define cpu_clear_flag(c, f) (c->F &= ~(1 << f))

#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_CY 4


// TODO: probably should have debugger support injected here
struct cpu
{
  // 8 bit registers
  uint8_t A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  uint16_t PC, SP;

  // pointer to memory of length 0x10000
  uint8_t *ram;

  // PRIVATE

  // store the remainder cycles when asked to run fewer cycles
  // required for the next instruction.
  uint8_t exec_rem_cycles;
};

// execute a given number of cycles, return number of cycles run
// or -1 on error.
int cpu_exec_cycles(struct cpu *, unsigned int);

// return the instruction at current PC
struct inst cpu_next_instruction(struct cpu *);

// execute a specific instruction, return the number of cycles run
// or -1 on error.
int cpu_exec_instruction(struct cpu *, struct inst *);

#endif
