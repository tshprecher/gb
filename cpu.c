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

static void word_to_regs(struct cpu * cpu, uint16_t word, uint8_t upper, uint8_t lower) {
  *(reg(cpu, lower)) = lower_8(word);
  *(reg(cpu, upper)) = upper_8(word);
}

/* TODO: uncomment when unused
static uint16_t get_dd_or_ss(struct cpu *cpu, uint8_t dd) {
  switch(dd) {
  case 0:
    return regs_to_word(cpu, rB, rC);
  case 1:
    return regs_to_word(cpu, rD, rE);
  case 2:
    return regs_to_word(cpu, rH, rL);
  case 3:
    return cpu->SP;
  default:
    return 0;
  }
}*/

static void set_dd_or_ss(struct cpu *cpu, uint8_t dd_or_ss, uint16_t word) {
  switch(dd_or_ss) {
  case 0:
    *(reg(cpu, rB)) = upper_8(word);
    *(reg(cpu, rC)) = lower_8(word);
    break;
  case 1:
    *(reg(cpu, rD)) = upper_8(word);
    *(reg(cpu, rE)) = lower_8(word);
    break;
  case 2:
    *(reg(cpu, rH)) = upper_8(word);
    *(reg(cpu, rL)) = lower_8(word);
    break;
  case 3:
    cpu->SP = word;
    break;
  }
}

// cpu_exec_instruction takes a cpu and instruction and executes
// the instruction. It returns the number of cycles run, -1 on error
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  printf("DEBUG: found inst: txt pattern -> %s, subtype -> %d\n", inst->txt_pattern, inst->subtype);
  uint16_t word;
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
      break;
    case 1:
      cpu->ram[regs_to_word(cpu, rB, rC)] = *(reg(cpu, rA));
      break;
    case 2:
      *(reg(cpu, rA)) = cpu->ram[regs_to_word(cpu, rB, rC)];
      break;
    case 3:
      cpu->ram[regs_to_word(cpu, rD, rE)] = *(reg(cpu, rA));
      break;
    case 4:
      *(reg(cpu, rA)) =  cpu->ram[regs_to_word(cpu, rD, rE)];
      break;
    case 5:
      word = regs_to_word(cpu, rH, rL);
      cpu->ram[word++] = *(reg(cpu, rA));
      word_to_regs(cpu, word, rH, rL);
      break;
    case 6:
      word = regs_to_word(cpu, rH, rL);
      *(reg(cpu, rA)) = cpu->ram[word++];
      word_to_regs(cpu, word, rH, rL);
      break;
    case 7:
      word = regs_to_word(cpu, rH, rL);
      cpu->ram[word--] = *(reg(cpu, rA));
      word_to_regs(cpu, word, rH, rL);
      break;
    case 8:
      word = regs_to_word(cpu, rH, rL);
      *(reg(cpu, rA)) = cpu->ram[word--];
      word_to_regs(cpu, word, rH, rL);
      break;
    case 9:
      *(reg(cpu, inst->args[0].value.byte)) = cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    case 10:
      cpu->ram[regs_to_word(cpu, rH, rL)] = *(reg(cpu, inst->args[0].value.byte));
      break;
    case 11:
      cpu->ram[0xFF00 + *(reg(cpu, rC))] = *(reg(cpu, rA));
      break;
    case 12:
      *(reg(cpu, rA)) = cpu->ram[0xFF00 + *(reg(cpu, rC))];
      break;
    case 13:
      cpu->SP = regs_to_word(cpu, rH, rL);
      break;
    case 14:
      *(reg(cpu, inst->args[0].value.byte)) = inst->args[1].value.byte;
      break;
    case 15:
      cpu->ram[regs_to_word(cpu, rH, rL)] = inst->args[0].value.byte;
      break;
    case 16:
      cpu->ram[0xFF00 + inst->args[0].value.byte] = *(reg(cpu, rA));
      break;
    case 17:
      *(reg(cpu, rA)) = cpu->ram[0xFF00 + inst->args[0].value.byte];
      break;
    case 18:
      set_dd_or_ss(cpu, inst->args[0].value.byte, nn_to_word(inst, 1));
      break;
    case 19:
      cpu->ram[nn_to_word(inst,0)] = *(reg(cpu, rA));
      break;
    case 20:
      *(reg(cpu, rA)) = cpu->ram[nn_to_word(inst,0)];
      break;
    case 21:
      cpu->ram[nn_to_word(inst,0)] = cpu->SP;
      break;
    default:
      return -1;
    }
    break;
  default:
    return -1;
  }
  cpu->PC += inst->bytelen;
  return inst->cycles;
}
