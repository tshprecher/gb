#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define cpu_flag_Z(c) ((c->F>>7) & 1)
#define cpu_flag_N(c) ((c->F>>6) & 1)
#define cpu_flag_H(c) ((c->F>>5) & 1)
#define cpu_flag_CY(c) ((c->F>>4) & 1)

// TODO: probably should have debugger support injected here
struct cpu
{
  // 8 bit registers
  int8_t A, B, C, D, E, F /*flags register*/, H, L;

  // 16 bit registers
  uint16_t PC, SP;

  // pointer to memory of length 0x10000
  uint8_t *ram;

  // PRIVATE

  // store the remainder cycles when asked to run fewer cycles
  // required for the next instruction.
  uint8_t exec_rem_cycles;

};


// execute a given number of cycles
int cpu_exec(struct cpu *, unsigned int);

#endif
