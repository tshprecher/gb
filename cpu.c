#include <stdio.h>
#include "cpu.h"
#include "inst.h"

#define upper_8(v) ((v >> 8) & 0xFF)
#define lower_8(v) (v & 0xFF)

#define push_PC(c) c->ram[c->SP--] = upper_8(c->PC); \
                   c->ram[c->SP--] = lower_8(c->PC)

#define nn_lower(i, a) (i->args[a].value.word[0])
#define nn_upper(i, a) (i->args[a].value.word[1])
#define nn_to_uint16(i, a) ((uint16_t) ((nn_upper(i, a) << 8) | nn_lower(i,a)))

int exec(struct cpu *cpu , struct inst *inst) {
  switch (inst->type) {
  case (CALL):
    switch (inst->subtype) {
    case 0:
      return 0;
    case 1:
      push_PC(cpu);
      cpu->PC = nn_to_uint16(inst, 0);
      return inst->bytelen;
    default:
      return 0;
    }
  default:
    return 0;
  }
}


// cpu_exec_cycles takes a cpu and execute 'cycles' number of cycles.
// It returns the number of cycles run.
int cpu_exec_cycles(struct cpu *cpu, unsigned int cycles)
{
  if (cycles == 0)
    return 0;
  fprintf(stderr, "error cpu_exec_cycles: not implemented");
  return -1;
}

// cpu_exec_instructions takes a cpu and executes 'insts' number of instructions.
// It returns the number of instructions run.
// TODO: change to return the number of cycles?
int cpu_exec_instructions(struct cpu *cpu, unsigned int insts) {
  struct inst inst = {0};
  char buf[16];
  for (int i = 0; i < insts; i++) {
    if (!init_inst_from_bytes(&inst, &cpu->ram[cpu->PC]))
      return -1;

    if (!exec(cpu, &inst)) {
      inst_to_str(&inst, buf);
      fprintf(stderr, "error: could not execute '%s' instruction", buf);
      return -1;
    }
  }
  return insts;
}
