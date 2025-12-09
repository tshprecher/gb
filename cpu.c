#include <stdio.h>
#include "cpu.h"
#include "inst.h"

#define upper_8(v) ((v >> 8) & 0xFF)
#define lower_8(v) (v & 0xFF)

// TODO: does this need to be a macro? it is used enough?
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

static int is_cond_true(struct cpu *cpu, enum cond cc) {
  if ( (cc == NZ && !cpu_flag(cpu, FLAG_Z)) ||
       (cc == Z && cpu_flag(cpu, FLAG_Z)) ||
       (cc == NC && !cpu_flag(cpu, FLAG_CY)) ||
       (cc == YC && cpu_flag(cpu, FLAG_CY)))
    return 1;

  return 0;
}

// imitates the Z80-based 4-bit ALU for easier tracking of half carry and carry bits
static uint8_t alu_4bit_add(uint8_t op1, uint8_t op2, uint8_t in_carry, uint8_t *out_carry) {
  uint16_t result = (op1 & 0x0F) + (op2 & 0x0F) + (in_carry ? 1 : 0);
  *out_carry = (result & (1 << 4)) > 0;
  return result & 0x0F;
}

// handles ALU operations for addition, properly assigning the flags
static uint8_t alu_add(struct cpu *cpu, uint8_t op1, uint8_t op2, uint8_t in_carry) {
  uint8_t h_carry, cy_carry;
  uint8_t lower4 = alu_4bit_add(op1, op2, in_carry, &h_carry);
  uint8_t upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  uint8_t result = lower4+(upper4 << 4);

  result == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
  cpu_clear_flag(cpu, FLAG_N);
  h_carry > 0 ? cpu_set_flag(cpu, FLAG_H) : cpu_clear_flag(cpu, FLAG_H);
  cy_carry > 0 ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);

  return result;
}

// handles SUB operations for addition, properly assigning the flags
static uint8_t alu_sub(struct cpu *cpu, uint8_t op1, uint8_t op2) {
  op2 = ~op2;

  uint8_t h_carry, cy_carry;
  uint8_t lower4 = alu_4bit_add(op1, op2, 1, &h_carry); // 2's complement means carry is 1 after bits flipped
  uint8_t upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  uint8_t result = lower4+(upper4 << 4);

  result == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
  cpu_set_flag(cpu, FLAG_N);
  h_carry > 0 ? cpu_clear_flag(cpu, FLAG_H) : cpu_set_flag(cpu, FLAG_H);
  cy_carry > 0 ? cpu_clear_flag(cpu, FLAG_CY) : cpu_set_flag(cpu, FLAG_CY);

  return result;
}


// cpu_exec_instruction takes a cpu and instruction and executes
// the instruction. It returns the number of cycles run, -1 on error
// TODO: could use a few more macros for clarity?
// TODO: should this return 0 cycles on error instead? makes some sense
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  printf("DEBUG: found inst: txt pattern -> %s, subtype -> %d\n", inst->txt_pattern, inst->subtype);
  uint16_t hl, word;
  uint8_t cy, dd_or_ss;
  uint8_t *bytePtr;
  int8_t e;
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
      e = (int8_t) inst->args[0].value.byte;
      if (e >= 0) {
	cy = (lower_8(cpu->SP) + e) >= (1<<8) ? 1 : 0;
	alu_add(cpu, upper_8(cpu->SP), 0, cy); // to set upper carry bits
      } else {
	cy = (lower_8(cpu->SP) < -e) ? 1 : 0;
	alu_sub(cpu, upper_8(cpu->SP), cy); // to set upper carry bits
      }
      cpu_clear_flag(cpu, FLAG_Z);
      cpu_clear_flag(cpu, FLAG_N);
      cpu->SP += e;
      break;
    case 4:
      hl = regs_to_word(cpu, rH, rL);
      word = get_dd_or_ss(cpu, inst->args[0].value.byte);
      cy = (cpu->L + lower_8(word)) >= (1<<8) ? 1 : 0;
      alu_add(cpu, cpu->H, upper_8(word), cy); // to set upper carry bits
      word_to_regs(cpu, hl+word, rH, rL);
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
  case (INC):
    switch (inst->subtype) {
    case 0:
      cy = cpu_flag(cpu, FLAG_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_add(cpu, *(reg(cpu, inst->args[0].value.byte)), 1, 0);
      cy == 0 ? cpu_clear_flag(cpu, FLAG_CY) : cpu_set_flag(cpu, FLAG_CY);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) + 1);
      break;
    case 2:
      cy = cpu_flag(cpu, FLAG_CY);
      cpu->ram[regs_to_word(cpu, rH, rL)] = alu_add(cpu, cpu->ram[regs_to_word(cpu, rH, rL)], 1, 0);
      cy == 0 ? cpu_clear_flag(cpu, FLAG_CY) : cpu_set_flag(cpu, FLAG_CY);
      break;
    }
    break;
  case (DEC):
    switch (inst->subtype) {
    case 0:
      cy = cpu_flag(cpu, FLAG_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_sub(cpu, *(reg(cpu, inst->args[0].value.byte)), 1);
      cy == 0 ? cpu_clear_flag(cpu, FLAG_CY) : cpu_set_flag(cpu, FLAG_CY);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) - 1);
      break;
    case 2:
      cy = cpu_flag(cpu, FLAG_CY);
      cpu->ram[regs_to_word(cpu, rH, rL)] = alu_sub(cpu, cpu->ram[regs_to_word(cpu, rH, rL)], 1);
      cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
      break;
    }
    break;
  case (AND):
    switch (inst->subtype) {
    case 0:
      cpu->A &= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A &= cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    case 2:
      cpu->A &= inst->args[0].value.byte;
      break;
    }
    cpu->A ? cpu_clear_flag(cpu, FLAG_Z) : cpu_set_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_set_flag(cpu, FLAG_H);
    cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (OR):
    switch (inst->subtype) {
    case 0:
      cpu->A |= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A |= cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    case 2:
      cpu->A |= inst->args[0].value.byte;
      break;
    }
    cpu->A ? cpu_clear_flag(cpu, FLAG_Z) : cpu_set_flag(cpu, FLAG_Z); // TODO: create another macro to set/unset based on value?
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RLCA):
    cy = (cpu->A & 0x80) != 0;
    cpu->A = (cpu->A << 1) | cy;
    cpu_clear_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RLC):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = (*bytePtr & 0x80) != 0;
    *bytePtr = (*bytePtr << 1) | cy;
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RLA):
    cy = (cpu->A & 0x80) != 0;
    cpu->A = (cpu->A << 1) | cpu_flag(cpu, FLAG_CY);
    cpu_clear_flag(cpu, FLAG_Z); // TODO: be able to set/clear multiple flags on a single line?
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RL):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = (*bytePtr & 0x80) != 0;
    *bytePtr = (*bytePtr << 1) | cpu_flag(cpu, FLAG_CY);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RRCA):
    cy = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (cy << 7);
    cpu_clear_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RRA):
    cy = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (cpu_flag(cpu, FLAG_CY) << 7);
    cpu_clear_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RRC):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = *bytePtr & 0x1;
    *bytePtr = (*bytePtr >> 1) | (cy << 7);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (RR):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = *bytePtr & 0x1;
    *bytePtr = (*bytePtr >> 1) | (cpu_flag(cpu, FLAG_CY) << 7);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (SLA):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = (*bytePtr & 0x80) != 0;
    *bytePtr <<= 1;
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (SRA):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = *bytePtr & 0x1; // TODO: replace all 0x01 and 0x1 with 1
    *bytePtr = (*bytePtr >> 1) | (*bytePtr & 0x80);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (SRL):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    cy = *bytePtr & 0x1;
    *bytePtr >>= 1;
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    cy ? cpu_set_flag(cpu, FLAG_CY) : cpu_clear_flag(cpu, FLAG_CY);
    break;
  case (SWAP):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[0].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    *bytePtr = (*bytePtr << 4) | (*bytePtr >> 4);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cpu_clear_flag(cpu, FLAG_CY);
    *bytePtr == 0 ? cpu_set_flag(cpu, FLAG_Z) : cpu_clear_flag(cpu, FLAG_Z);
    break;
  case (BIT):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[1].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    (*bytePtr & (1 << inst->args[0].value.byte)) ? cpu_clear_flag(cpu, FLAG_Z): cpu_set_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_set_flag(cpu, FLAG_H);
    break;
  case (SET):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[1].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    *bytePtr |= (1 << inst->args[0].value.byte);
    break;
  case (RES):
    switch (inst->subtype) {
    case 0:
      bytePtr = reg(cpu, inst->args[1].value.byte);
      break;
    case 1:
      bytePtr = &cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    }
    *bytePtr &= ~(1 << inst->args[0].value.byte);
    break;
  case (JP):
    switch (inst->subtype) {
    case 0:
      cpu->PC = nn_to_word(inst, 0);
      return inst->cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	cpu->PC = nn_to_word(inst, 1);
	return 4;
      }
      break;
    case 2:
      cpu->PC = cpu->ram[regs_to_word(cpu, rH, rL)];
      return inst->cycles;
    }
    break;
  case (JR):
    switch (inst->subtype) {
    case 0:
      e = (int8_t) inst->args[0].value.byte;
      printf("DEBUG: e -> %d\n", e);
      cpu->PC = cpu->PC + e + 2;
      return inst->cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	e = (int8_t) inst->args[1].value.byte;
	cpu->PC = cpu->PC + e + 2;
	return 3;
      }
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
  case (CP):
    return -1;
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
      word = nn_to_word(inst,0);
      cpu->ram[word] = lower_8(cpu->SP);
      cpu->ram[word+1] = upper_8(cpu->SP);
      break;
    }
    break;
  case (XOR):
    switch (inst->subtype) {
    case 0:
      cpu->A ^= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A ^= cpu->ram[regs_to_word(cpu, rH, rL)];
      break;
    case 2:
      cpu->A ^= inst->args[0].value.byte;
      break;
    }
    cpu->A ? cpu_clear_flag(cpu, FLAG_Z) : cpu_set_flag(cpu, FLAG_Z);
    cpu_clear_flag(cpu, FLAG_N);
    cpu_clear_flag(cpu, FLAG_H);
    cpu_clear_flag(cpu, FLAG_CY);
    break;
  default:
    return -1;
  }
  printf("DEBUG: found inst bytelen -> %d\n", inst->bytelen);
  cpu->PC += inst->bytelen;
  return inst->cycles;
}
