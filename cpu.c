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

static uint16_t get_dd_or_ss(struct cpu *cpu, uint8_t dd_or_ss) {
  switch(dd_or_ss) {
  case 0:
    return regs_to_word(cpu, rB, rC);
  case 1:
    return regs_to_word(cpu, rD, rE);
  case 2:
    return regs_to_word(cpu, rH, rL);
  case 3:
    return cpu->SP;
  }
  return 0;
}

static void set_dd_or_ss(struct cpu *cpu, uint8_t dd_or_ss, uint16_t word) {
  switch(dd_or_ss) {
  case 0:
    cpu->B = upper_8(word);
    cpu->C = lower_8(word);
    break;
  case 1:
    cpu->D = upper_8(word);
    cpu->E = lower_8(word);
    break;
  case 2:
    cpu->H = upper_8(word);
    cpu->L = lower_8(word);
    break;
  case 3:
    cpu->SP = word;
    break;
  }
}

// handles ALU operations for addition, properly assigning the flags
static uint8_t alu_add(struct cpu *cpu, uint8_t op1, uint8_t op2, uint8_t has_carry) {
  uint16_t result = (op1 & 0x0F) + (op2 & 0x0F) + (has_carry ? 1 : 0);
  (result & (1 << 4)) ? cpu_set_flag(cpu, FLAG_H) : cpu_clear_flag(cpu, FLAG_H);

  result += (op1 & 0xF0);
  result += (op2 & 0xF0);

  (result & (1 << 8)) ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);

  cpu_clear_flag(cpu, FLAG_N);
  result &= 0xFF;
  result == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
  printf("DEBUG: alu final result -> 0x%02X\n", result & 0xFF);
  return result;
}

static uint8_t alu_sub(struct cpu *cpu, uint8_t op1, uint8_t op2) {
  uint8_t result = alu_add(cpu, op1,  ~op2 + 1, 0);
  cpu_set_flag(cpu, FLAG_N);
  cpu_flip_flag(cpu, FLAG_H);
  cpu_flip_flag(cpu, FLAG_CY);
  return result;
}


// cpu_exec_instruction takes a cpu and instruction and executes
// the instruction. It returns the number of cycles run, -1 on error
// TODO: could use a few more macros for clarity?
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  printf("DEBUG: found inst: txt pattern -> %s, subtype -> %d\n", inst->txt_pattern, inst->subtype);
  uint16_t hl, nn_word;
  switch (inst->type) {
  case (ADC):
    switch (inst->subtype) {
    case 0:
      cpu->A = alu_add(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)), cpu_flag(cpu, FLAG_CY));
      break;
    case 1:
      cpu->A = alu_add(cpu, cpu->A, inst->args[0].value.byte, cpu_flag(cpu, FLAG_CY));
      break;
    case 2:
      cpu->A = alu_add(cpu, cpu->A, cpu->ram[regs_to_word(cpu, rH, rL)], cpu_flag(cpu, FLAG_CY));
      break;
    }
    break;
  case (ADD):
    switch (inst->subtype) {
    case 0:
      cpu->A = alu_add(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)), 0);
      break;
    case 1:
      cpu->A = alu_add(cpu, cpu->A, cpu->ram[regs_to_word(cpu, rH, rL)], 0);
      break;
    case 2:
      cpu->A = alu_add(cpu, cpu->A, inst->args[0].value.byte, 0);
      break;
    case 3:
      alu_add(cpu, lower_8(cpu->SP), inst->args[0].value.byte, 0); // to set the carry bits
      cpu_clear_flag(cpu, FLAG_Z);
      cpu_clear_flag(cpu, FLAG_N);
      cpu->SP += inst->args[0].value.byte;
      break;
    case 4:
      hl = regs_to_word(cpu, rH, rL);
      uint16_t dd_or_ss = get_dd_or_ss(cpu, inst->args[0].value.byte);
      int lower = lower_8(dd_or_ss);
      int upper = upper_8(dd_or_ss);
      int carry = (cpu->L + lower) >= (1<<8) ? 1 : 0;
      alu_add(cpu, cpu->H, upper + carry, 0); // to set upper carry bits
      word_to_regs(cpu, hl+dd_or_ss, rH, rL);
      break;
    }
    break;
  case (SUB):
    switch (inst->subtype) {
    case 0:
      cpu->A = alu_sub(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)));
      break;
    case 1:
      cpu->A = alu_sub(cpu, cpu->A, inst->args[0].value.byte);
      break;
    case 2:
      cpu->A = alu_sub(cpu, cpu->A, cpu->ram[regs_to_word(cpu, rH, rL)]);
      break;
    }
    break;
  case (CALL):
    switch (inst->subtype) {
    case 0:
      return -1;
    case 1:
      push_PC(cpu);
      cpu->PC = nn_to_word(inst, 0);
      return inst->cycles;
    }
    break;
  case (LD):
    switch (inst->subtype) {
    case 0:
      *(reg(cpu, inst->args[0].value.byte)) = *(reg(cpu, inst->args[1].value.byte));
      break;
    case 1:
      cpu->ram[regs_to_word(cpu, rB, rC)] = cpu->A;
      break;
    case 2:
      cpu->A = cpu->ram[regs_to_word(cpu, rB, rC)];
      break;
    case 3:
      cpu->ram[regs_to_word(cpu, rD, rE)] = cpu->A;
      break;
    case 4:
      cpu->A =  cpu->ram[regs_to_word(cpu, rD, rE)];
      break;
    case 5:
      hl = regs_to_word(cpu, rH, rL);
      cpu->ram[hl++] = cpu->A;
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 6:
      hl = regs_to_word(cpu, rH, rL);
      cpu->A = cpu->ram[hl++];
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 7:
      hl = regs_to_word(cpu, rH, rL);
      cpu->ram[hl--] = cpu->A;
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 8:
      hl = regs_to_word(cpu, rH, rL);
      cpu->A = cpu->ram[hl--];
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 9:
      *(reg(cpu, inst->args[0].value.byte)) = cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    case 10:
      cpu->ram[regs_to_word(cpu, rH, rL)] = *(reg(cpu, inst->args[0].value.byte));
      break;
    case 11:
      cpu->ram[0xFF00 + cpu->C] = cpu->A;
      break;
    case 12:
      cpu->A = cpu->ram[0xFF00 + cpu->C];
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
      cpu->ram[0xFF00 + inst->args[0].value.byte] = cpu->A;
      break;
    case 17:
      cpu->A = cpu->ram[0xFF00 + inst->args[0].value.byte];
      break;
    case 18:
      set_dd_or_ss(cpu, inst->args[0].value.byte, nn_to_word(inst, 1));
      break;
    case 19:
      cpu->ram[nn_to_word(inst,0)] = cpu->A;
      break;
    case 20:
      cpu->A = cpu->ram[nn_to_word(inst,0)];
      break;
    case 21:
      nn_word = nn_to_word(inst,0);
      cpu->ram[nn_word] = lower_8(cpu->SP);
      cpu->ram[nn_word+1] = upper_8(cpu->SP);
      break;
    }
    break;
  default:
    return -1;
  }
  cpu->PC += inst->bytelen;
  return inst->cycles;
}
