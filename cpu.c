#include <stdio.h>
#include "cpu.h"
#include "inst.h"


int exec(struct cpu *cpu , struct inst *inst) {
  switch (inst->type) {
  case (CALL):
    printf("DEBUG: inside execute CALL\n");
    return 0;
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
