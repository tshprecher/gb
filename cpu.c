#include <stdio.h>
#include "cpu.h"
#include "inst.h"

#define upper_8(v) ((v >> 8) & 0xFF)
#define lower_8(v) (v & 0xFF)

#define push_PC(c) c->SP--; \
                   c->ram[c->SP--] = upper_8(c->PC); \
                   c->ram[c->SP] = lower_8(c->PC)


#define nn_lower(i, a) (i->args[a].value.word[0])
#define nn_upper(i, a) (i->args[a].value.word[1])
#define bytes_to_word(l, u) ((uint16_t) (u << 8 | l))
#define nn_to_word(i, a) bytes_to_word(nn_lower(i,a),nn_upper(i,a))


// cpu_exec_cycles takes a cpu and execute 'cycles' number of cycles.
// It returns the number of cycles run.
int cpu_exec_cycles(struct cpu *cpu, unsigned int cycles)
{
  if (cycles == 0)
    return 0;
  fprintf(stderr, "error cpu_exec_cycles: not implemented");
  return -1;
}

struct inst cpu_next_instruction(struct cpu *cpu) {
  struct inst inst;
  init_inst_from_bytes(&inst, &cpu->ram[cpu->PC]);
  return inst;
}


static uint8_t * reg(struct cpu *cpu, uint8_t reg) {
  // NOTE: making this a switch throws a compiler warning about
  // putting a label immediately afer a delaraction without using C23
  if (reg == rA)
      return &cpu->A;
  if (reg == rB)
    return &cpu->B;
  if (reg == rC)
    return &cpu->C;
  if (reg == rD)
    return &cpu->D;
  if (reg == rE)
    return &cpu->E;
  if (reg == rH)
    return &cpu->H;
  if (reg == rL)
    return &cpu->L;
  return NULL;
}

static uint16_t regs_to_word(struct cpu * cpu, uint8_t upper, uint8_t lower) {
  uint8_t l, u;
  l = *(reg(cpu, lower));
  u = *(reg(cpu, upper));
  return bytes_to_word(l, u);
}

// cpu_exec_instruction takes a cpu and instruction and executes
// the instruction. It returns the number of cycles run, -1 on error
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  switch (inst->type) {
  case (CALL):
    switch (inst->subtype) {
    case 0:
      return 0;
    case 1:
      push_PC(cpu);
      cpu->PC = nn_to_word(inst, 0);
      return inst->cycles;
    default:
      return -1;
    }
  case (LD):
    switch (inst->subtype) {
    case 0:
      *(reg(cpu, inst->args[0].value.byte)) = *(reg(cpu, inst->args[1].value.byte));
      return inst->cycles;
    case 9:
      *(reg(cpu, inst->args[0].value.byte)) = cpu->ram[regs_to_word(cpu, rH, rL)];
      return inst->cycles;
    case 14:
      *(reg(cpu, inst->args[0].value.byte)) = inst->args[1].value.byte;
      return inst->cycles;
    default:
      return -1;
    }
  default:
    return -1;
  }

}
