#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "cpu.h"
#include "macros.h"
#include "memory.h"
#include "inst.h"
#include "timing.h"

// flags
#define F_Z 7
#define F_N 6
#define F_H 5
#define F_CY 4

static inline u8 upper(u16 word) {
  return (word >> 8) & 0xFF;
}

static inline u8 lower(u16 word) {
  return word & 0xFF;
}

static inline u16 bytes_to_word(u8 l, u8 u) {
  return (u16) (u << 8 | l);
}

static inline u16 nn_to_word(struct inst *i, int a) {
  u8 l = i->args[a].value.word[0];
  u8 u = i->args[a].value.word[1];
  return bytes_to_word(l, u);
}

// hard coded interrupt handler addresses by interrupt priority
static u16 interrupt_handlers[] = {0x0040, 0x0048, 0x0050, 0x0058, 0x0060};

// TODO: should all enums be capital?
void interrupt(struct interrupt_controller *ic, enum Interrupt interrupt) {
  b_set_on(ic->IF, interrupt);
}

static inline void check_interrupt(struct cpu *cpu) {
  if (cpu->t_cycles_since_last_inst == 0 &&
      (cpu->interrupt_c->IF & cpu->interrupt_c->IE)) {

    u16 *handler_addr = interrupt_handlers;
    u8 mask = 1;
    while (mask < (1<<5)) {
      if ((cpu->interrupt_c->IF & mask) && (cpu->interrupt_c->IE & mask))
       break;
      mask <<= 1;
      handler_addr++;
    }

    if (cpu->IME) {
      cpu->interrupt_c->IF ^= mask;
      cpu->is_halted = 0;
      cpu->IME = 0;

      // TODO: consolidate PUSH into one operation?
      mem_write(cpu->memory_c,cpu->SP-1,  upper(cpu->PC));
      mem_write(cpu->memory_c,cpu->SP-2, lower(cpu->PC));
      cpu->SP -= 2;
      cpu->PC = *handler_addr;
      cpu->interrupt_t_cycles = 6 << 2; // TODO: is this correct for every interrupt type?
    } else {
      if (cpu->is_halted) {
	// PC is already set to the instruction after halt, just unhalt
	cpu->is_halted = 0;
      }
    }
  }
}

static u8 * reg(struct cpu *cpu, u8 reg) {
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
  if (reg == rF)
    return &cpu->F;
  if (reg == rH)
    return &cpu->H;
  if (reg == rL)
    return &cpu->L;
  return NULL;
}

static u16 regs_to_word(struct cpu * cpu, u8 upper_reg, u8 lower_reg) {
  return bytes_to_word(*(reg(cpu, lower_reg)), *(reg(cpu, upper_reg)));
}

static void word_to_regs(struct cpu * cpu, u16 word, u8 upper_reg, u8 lower_reg) {
  *(reg(cpu, lower_reg)) = lower(word);
  *(reg(cpu, upper_reg)) = upper(word);
}

static u16 get_qq(struct cpu *cpu, u8 qq) {
  switch(qq) {
  case 0:
    return regs_to_word(cpu, rB, rC);
  case 1:
    return regs_to_word(cpu, rD, rE);
  case 2:
    return regs_to_word(cpu, rH, rL);
  case 3:
    return regs_to_word(cpu, rA, rF) & 0xFFF0; // flags register alyways has lower nibble 0
  }
  // TODO: panic here
  return 0;
}

static void set_qq(struct cpu *cpu, u8 qq, u16 word) {
  switch(qq) {
  case 0:
    cpu->B = upper(word);
    cpu->C = lower(word);
    break;
  case 1:
    cpu->D = upper(word);
    cpu->E = lower(word);
    break;
  case 2:
    cpu->H = upper(word);
    cpu->L = lower(word);
    break;
  case 3:
    cpu->A = upper(word);
    cpu->F = lower(word) & 0xF0; // flags register alyways has lower nibble 0
    break;
    }
}

static u16 get_dd_or_ss(struct cpu *cpu, u8 dd_or_ss) {
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

static void set_dd_or_ss(struct cpu *cpu, u8 dd_or_ss, u16 word) {
  switch(dd_or_ss) {
  case 0:
    cpu->B = upper(word);
    cpu->C = lower(word);
    break;
  case 1:
    cpu->D = upper(word);
    cpu->E = lower(word);
    break;
  case 2:
    cpu->H = upper(word);
    cpu->L = lower(word);
    break;
  case 3:
    cpu->SP = word;
    break;
  }
}

static int is_cond_true(struct cpu *cpu, enum cond cc) {
  return (cc == NZ && b_is_off(cpu->F, F_Z)) ||
    (cc == Z && b_is_on(cpu->F, F_Z)) ||
    (cc == NC && b_is_off(cpu->F, F_CY)) ||
    (cc == YC && b_is_on(cpu->F, F_CY));
}

// imitates the Z80-based 4-bit ALU for easier tracking of half carry and carry bits
static u8 alu_4bit_add(u8 op1, u8 op2, u8 in_carry, u8 *out_carry) {
  u16 result = (op1 & 0x0F) + (op2 & 0x0F) + (in_carry ? 1 : 0);
  *out_carry = b_is_on(result, 4);
  return result & 0x0F;
}

// handles ALU operations for addition, properly assigning the flags
static u8 alu_add(struct cpu *cpu, u8 op1, u8 op2, u8 in_carry) {
  u8 h_carry, cy_carry;
  u8 lower4 = alu_4bit_add(op1, op2, in_carry, &h_carry);
  u8 upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  u8 result = lower4+(upper4 << 4);

  b_set(cpu->F, F_Z, !result);
  b_set_off(cpu->F, F_N);
  b_set(cpu->F, F_H, h_carry);
  b_set(cpu->F, F_CY, cy_carry);

  return result;
}

// handles ALU operations for subtraction, properly assigning the flags
static u8 alu_sub(struct cpu *cpu, u8 op1, u8 op2) {
  op2 = ~op2;
  u8 h_carry, cy_carry;
  u8 lower4 = alu_4bit_add(op1, op2, 1, &h_carry); // 2's complement means carry is 1 after bits flipped
  u8 upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  u8 result = lower4+(upper4 << 4);

  b_set(cpu->F, F_Z, !result);
  b_set_on(cpu->F, F_N);
  b_set(cpu->F, F_H, !h_carry);
  b_set(cpu->F, F_CY, !cy_carry);

  return result;
}

void init_cpu(struct cpu *cpu) {
  cpu->PC = 0x100;
  cpu->SP = 0xFFFE;
}

void cpu_tick(struct cpu *cpu) {
  if (cpu->t_cycles_since_last_inst < 0) { // catch up for variably timed instructions
      cpu->t_cycles_since_last_inst++;
      return;
  }

  if (cpu->interrupt_t_cycles) {
    cpu->interrupt_t_cycles--;
    return;
  }

  check_interrupt(cpu);

  if (cpu->is_halted)
    return;

  if (!cpu->next_inst || cpu->t_cycles_since_last_inst == 0) // TODO: put this in the init?
    cpu->next_inst = mem_read_inst(cpu->memory_c, cpu->PC);

  cpu->t_cycles_since_last_inst++;
  if (cpu->t_cycles_since_last_inst == (cpu->next_inst->m_cycles << 2)) { // 4 clock "t" cycles per machine "m" cycle
    /*    char buf[128];
    inst_to_str(cpu->next_inst, buf);
    printf("(debug): [t: %d, f: %d]  0x%04X\t%s\n", cpu->next_inst->type, cpu->next_inst->form, cpu->PC, buf);*/
    int consumed_m_cycles = cpu_exec_instruction(cpu, cpu->next_inst);
    if (consumed_m_cycles < 0) {
      char buf[16];
      inst_to_str(cpu->next_inst, buf);
      fprintf(stderr, "error: could not execute instr '%s' @ 0x%04X\n",
	      buf, cpu->PC);
      exit(1);
    }
    cpu->t_cycles_since_last_inst = (cpu->next_inst->m_cycles-consumed_m_cycles)<<2;
  }
}

struct dst {
  u8 *reg;
  u16 addr;
  struct mem_controller *mc;
};

static inline void dst_init_reg(struct dst *dst, struct cpu * cpu, u8 r) {
  dst->reg = reg(cpu, r);
}

static inline void dst_init_addr(struct dst *dst, struct mem_controller *mc, u16 addr) {
  dst->reg = NULL;
  dst->addr = addr;
  dst->mc = mc;
}

static inline u8 dst_val(struct dst *dst) {
  return dst->reg ? *dst->reg : mem_read(dst->mc, dst->addr);
}

static inline void dst_assign(struct dst *dst, s8 byte) {
  dst->reg ? *dst->reg = byte : mem_write(dst->mc, dst->addr, byte);
}

// cpu_exec_instruction takes a executes the instruction.
// it returns the number of m cycles run, -1 on error
// TODO: could use a few more macros for clarity?
// TODO: should this return 0 cycles on error instead? makes some sense
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  u16 hl, word;
  u8 flag_cy, flag_h,  flag_z, byte, dd_or_ss, daa_adj;
  struct dst dst;
  s8 e;
  switch (inst->type) {
  case NOP:
    break;
  case ADC:
    switch (inst->form) {
    case 0:
      cpu->A = alu_add(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)), b_is_on(cpu->F, F_CY));
      break;
    case 1:
      cpu->A = alu_add(cpu, cpu->A, inst->args[0].value.byte, b_is_on(cpu->F, F_CY));
      break;
    case 2:
      cpu->A = alu_add(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), b_is_on(cpu->F, F_CY));
      break;
    }
    break;
  case ADD:
    switch (inst->form) {
    case 0:
      cpu->A = alu_add(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)), 0);
      break;
    case 1:
      cpu->A = alu_add(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), 0);
      break;
    case 2:
      cpu->A = alu_add(cpu, cpu->A, inst->args[0].value.byte, 0);
      break;
    case 3:
      e = (s8) inst->args[0].value.byte;
      alu_add(cpu, lower(cpu->SP), e, 0);

      b_set_off(cpu->F, F_Z);
      b_set_off(cpu->F, F_N);
      cpu->SP += e;
      break;
    case 4:
      flag_z = b_is_on(cpu->F, F_Z); // store before to set back at end
      word = get_dd_or_ss(cpu, inst->args[0].value.byte);

      cpu->L = alu_add(cpu, cpu->L, lower(word), 0);
      cpu->H = alu_add(cpu, cpu->H, upper(word), b_is_on(cpu->F, F_CY));

      b_set_off(cpu->F, F_N);
      b_set(cpu->F, F_Z, flag_z);
      break;
    }
    break;
  case SUB:
    switch (inst->form) {
    case 0:
      cpu->A = alu_sub(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)));
      break;
    case 1:
      cpu->A = alu_sub(cpu, cpu->A, inst->args[0].value.byte);
      break;
    case 2:
      cpu->A = alu_sub(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)));
      break;
    }
    break;
  case SBC:
    flag_cy = b_is_on(cpu->F, F_CY);
    switch (inst->form) {
    case 0:
      cpu->A = alu_sub(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)));
      break;
    case 1:
      cpu->A = alu_sub(cpu, cpu->A, inst->args[0].value.byte);
      break;
    case 2:
      cpu->A = alu_sub(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)));
      break;
    }
    if (flag_cy) { // if initial carry bit on, subtract 1
      flag_cy = b_is_on(cpu->F, F_CY);
      flag_h = b_is_on(cpu->F, F_H);

      cpu->A = alu_sub(cpu, cpu->A, 1);

      flag_cy |= b_is_on(cpu->F, F_CY);
      flag_h |= b_is_on(cpu->F, F_H);

      b_set(cpu->F, F_CY, flag_cy);
      b_set(cpu->F, F_H, flag_h);
    }
    break;
  case SCF:
    // TODO: test
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set_on(cpu->F, F_CY);
    break;
  case INC:
    switch (inst->form) {
    case 0:
      flag_cy = b_is_on(cpu->F, F_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_add(cpu, *(reg(cpu, inst->args[0].value.byte)), 1, 0);
      b_set(cpu->F, F_CY, flag_cy);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) + 1);
      break;
    case 2:
      flag_cy = b_is_on(cpu->F, F_CY);
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL),
		alu_add(cpu, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), 1, 0));
      b_set(cpu->F, F_CY, flag_cy);
      break;
    }
    break;
  case DEC:
    switch (inst->form) {
    case 0:
      flag_cy = b_is_on(cpu->F, F_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_sub(cpu, *(reg(cpu, inst->args[0].value.byte)), 1);
      b_set(cpu->F, F_CY, flag_cy);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) - 1);
      break;
    case 2:
      flag_cy = b_is_on(cpu->F, F_CY);
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL),
		alu_sub(cpu, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), 1));
      b_set(cpu->F, F_CY, flag_cy);
      break;
    }
    break;
  case AND:
    switch (inst->form) {
    case 0:
      cpu->A &= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A &= mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    case 2:
      cpu->A &= inst->args[0].value.byte;
      break;
    }
    cpu->A ? b_set_off(cpu->F, F_Z) : b_set_on(cpu->F, F_Z); // TODO: pass this a value
    b_set_off(cpu->F, F_N);
    b_set_on(cpu->F, F_H);
    b_set_off(cpu->F, F_CY);
    break;
  case OR:
    switch (inst->form) {
    case 0:
      cpu->A |= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A |= mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    case 2:
      cpu->A |= inst->args[0].value.byte;
      break;
    }
    cpu->A ? b_set_off(cpu->F, F_Z) : b_set_on(cpu->F, F_Z); // TODO: create another macro to set/unset based on value?
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set_off(cpu->F, F_CY);
    break;
  case XOR:
    switch (inst->form) {
    case 0:
      cpu->A ^= *(reg(cpu, inst->args[0].value.byte));
      break;
    case 1:
      cpu->A ^= mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    case 2:
      cpu->A ^= inst->args[0].value.byte;
      break;
    }
    cpu->A ? b_set_off(cpu->F, F_Z) : b_set_on(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set_off(cpu->F, F_CY);
    break;
  case CP:
    switch (inst->form) {
    case 0:
      alu_sub(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)));
      break;
    case 1:
      alu_sub(cpu, cpu->A, inst->args[0].value.byte);
      break;
    case 2:
      alu_sub(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)));
      break;
    }
    break;
  case CPL:
    cpu->A = ~cpu->A;
    b_set_on(cpu->F, F_N);
    b_set_on(cpu->F, F_H);
    break;
  case RLCA:
    flag_cy = b_is_on(cpu->A, 7);
    cpu->A = (cpu->A << 1) | flag_cy;
    b_set_off(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RLC:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 7);
    byte = (byte << 1) | flag_cy;
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RLA:
    flag_cy = b_is_on(cpu->A, 7);
    cpu->A = (cpu->A << 1) | b_is_on(cpu->F, F_CY);

    b_set_off(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RL:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 7);
    byte = (byte << 1) | b_is_on(cpu->F, F_CY);
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RRCA:
    flag_cy = b_is_on(cpu->A, 0);
    cpu->A = (cpu->A >> 1) | (flag_cy << 7);
    b_set_off(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RRA:
    flag_cy = b_is_on(cpu->A, 0);
    cpu->A = (cpu->A >> 1) | (b_is_on(cpu->F, F_CY) << 7);

    b_set_off(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RRC:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 0);
    byte = (byte >> 1) | (flag_cy << 7);
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case RR:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 0);
    byte = (byte >> 1) | (b_is_on(cpu->F, F_CY) << 7);
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case SLA:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 7);
    byte <<= 1;
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case SRA:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 0);
    byte = (byte >> 1) | (byte & 0x80);
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case SRL:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    flag_cy = b_is_on(byte, 0);
    byte >>= 1;
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !byte);
    b_set(cpu->F, F_CY, flag_cy);
    break;
  case SWAP:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[0].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    byte = (byte << 4) | (byte >> 4);
    dst_assign(&dst, byte);

    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set_off(cpu->F, F_CY);
    b_set(cpu->F, F_Z, !byte);
    break;
  case BIT:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[1].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    b_set(cpu->F, F_Z, b_is_off(byte, inst->args[0].value.byte));
    b_set_off(cpu->F, F_N);
    b_set_on(cpu->F, F_H);
    break;
  case SET:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[1].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    b_set_on(byte, inst->args[0].value.byte);
    dst_assign(&dst, byte);
    break;
  case DI:
    cpu->IME = 0;
    break;
  case EI:
    cpu->enable_interrupt_after_next_inst = 1;
    break;
  case RES:
    switch (inst->form) {
    case 0:
      dst_init_reg(&dst, cpu, inst->args[1].value.byte);
      break;
    case 1:
      dst_init_addr(&dst, cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    }
    byte = dst_val(&dst);
    b_set_off(byte, inst->args[0].value.byte);
    dst_assign(&dst, byte);
    break;
  case JP:
    switch (inst->form) {
    case 0:
      cpu->PC = nn_to_word(inst, 0);
      return inst->m_cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	cpu->PC = nn_to_word(inst, 1);
	return 4;
      }
      break;
    case 2:
      cpu->PC = regs_to_word(cpu, rH, rL);
      return inst->m_cycles;
    }
    break;
  case JR:
    switch (inst->form) {
    case 0:
      e = (s8) inst->args[0].value.byte;
      cpu->PC = cpu->PC + e + 2;
      return inst->m_cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	e = (s8) inst->args[1].value.byte;
	cpu->PC = cpu->PC + e + 2;
	return 3;
      }
      break;
    }
    break;
  case CALL:
    switch (inst->form) {
    case 0:
      mem_write(cpu->memory_c, cpu->SP-1, upper((cpu->PC + 3)));
      mem_write(cpu->memory_c, cpu->SP-2, lower((cpu->PC + 3)));
      cpu->SP-=2;
      cpu->PC = nn_to_word(inst, 0);
      return inst->m_cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	mem_write(cpu->memory_c, cpu->SP-1, upper((cpu->PC + 3)));
	mem_write(cpu->memory_c, cpu->SP-2, lower((cpu->PC + 3)));
	cpu->SP-=2;
	cpu->PC = nn_to_word(inst, 1);
	return 6;
	}
      break;
    }
    break;
  case CCF:
    // TODO: test
    b_set_off(cpu->F, F_N);
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_CY, b_is_off(cpu->F, F_CY));
    break;
  case RET:
    switch (inst->form) {
    case 0:
      cpu->PC = bytes_to_word(mem_read(cpu->memory_c, cpu->SP), mem_read(cpu->memory_c, cpu->SP+1));
      cpu->SP+=2;
      return inst->m_cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	cpu->PC = bytes_to_word(mem_read(cpu->memory_c, cpu->SP), mem_read(cpu->memory_c, cpu->SP+1));
	cpu->SP+=2;
	return 5;
      }
      break;
    }
    break;
  case RETI:
    cpu->IME = 1;
    cpu->PC = bytes_to_word(mem_read(cpu->memory_c, cpu->SP), mem_read(cpu->memory_c, cpu->SP+1));
    cpu->SP+=2;
    return inst->m_cycles;
  case RST:
    mem_write(cpu->memory_c, cpu->SP-1, upper((cpu->PC+1)));
    mem_write(cpu->memory_c, cpu->SP-2, lower((cpu->PC+1)));
    cpu->SP-=2;
    switch (inst->args[0].value.byte) {
    case 0:
      cpu->PC = 0;
      break;
    case 1:
      cpu->PC = 0x08;
      break;
    case 2:
      cpu->PC = 0x10;
      break;
    case 3:
      cpu->PC = 0x18;
      break;
    case 4:
      cpu->PC = 0x20;
      break;
    case 5:
      cpu->PC = 0x28;
      break;
    case 6:
      cpu->PC = 0x30;
      break;
    case 7:
      cpu->PC = 0x38;
      break;
    }
    return inst->m_cycles;
  case LD:
    switch (inst->form) {
    case 0:
      *(reg(cpu, inst->args[0].value.byte)) = *(reg(cpu, inst->args[1].value.byte));
      break;
    case 1:
      mem_write(cpu->memory_c, regs_to_word(cpu, rB, rC), cpu->A);
      break;
    case 2:
      cpu->A = mem_read(cpu->memory_c, regs_to_word(cpu, rB, rC));
      break;
    case 3:
      mem_write(cpu->memory_c, regs_to_word(cpu, rD, rE), cpu->A);
      break;
    case 4:
      cpu->A =  mem_read(cpu->memory_c,regs_to_word(cpu, rD, rE));
      break;
    case 5:
      hl = regs_to_word(cpu, rH, rL);
      mem_write(cpu->memory_c, hl++, cpu->A);
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 6:
      hl = regs_to_word(cpu, rH, rL);
      cpu->A = mem_read(cpu->memory_c, hl++);
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 7:
      hl = regs_to_word(cpu, rH, rL);
      mem_write(cpu->memory_c, hl--, cpu->A);
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 8:
      hl = regs_to_word(cpu, rH, rL);
      cpu->A = mem_read(cpu->memory_c, hl--);
      word_to_regs(cpu, hl, rH, rL);
      break;
    case 9:
      *(reg(cpu, inst->args[0].value.byte)) = mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL));
      break;
    case 10:
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL),
		*(reg(cpu, inst->args[0].value.byte)));
      break;
    case 11:
      mem_write(cpu->memory_c, 0xFF00 + cpu->C, cpu->A);
      break;
    case 12:
      cpu->A = mem_read(cpu->memory_c, 0xFF00 + cpu->C);
      break;
    case 13:
      cpu->SP = regs_to_word(cpu, rH, rL);
      break;
    case 14:
      *(reg(cpu, inst->args[0].value.byte)) = inst->args[1].value.byte;
      break;
    case 15:
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL), inst->args[0].value.byte);
      break;
    case 16:
      mem_write(cpu->memory_c, 0xFF00 + inst->args[0].value.byte, cpu->A);
      break;
    case 17:
      cpu->A = mem_read(cpu->memory_c, 0xFF00 + inst->args[0].value.byte);
      break;
    case 18:
      set_dd_or_ss(cpu, inst->args[0].value.byte, nn_to_word(inst, 1));
      break;
    case 19:
      mem_write(cpu->memory_c, nn_to_word(inst,0), cpu->A);
      break;
    case 20:
      cpu->A = mem_read(cpu->memory_c,nn_to_word(inst,0));
      break;
    case 21:
      word = nn_to_word(inst,0);
      mem_write(cpu->memory_c, word, lower(cpu->SP));
      mem_write(cpu->memory_c, word+1, upper(cpu->SP));
      break;
    }
    break;
  case LDHL:
    e = (s8) inst->args[0].value.byte;
    alu_add(cpu, lower(cpu->SP), e, 0);

    b_set_off(cpu->F, F_Z);
    b_set_off(cpu->F, F_N);
    word_to_regs(cpu, cpu->SP + e, rH, rL);
    break;
  case DAA:
    daa_adj = 0;
    if (b_is_off(cpu->F, F_N)) {
      if (b_is_on(cpu->F, F_H) || (cpu->A & 0x0F) > 0x9) {
	daa_adj |= 0x06;
      }
      if (b_is_on(cpu->F, F_CY) || cpu->A > 0x99) {
	daa_adj |= 0x60;
	b_set_on(cpu->F, F_CY);
      }
      cpu->A += daa_adj;
    } else {
      if (b_is_on(cpu->F, F_H)) {
	daa_adj |= 0x06;
      }
      if (b_is_on(cpu->F, F_CY)) {
	daa_adj |= 0x60;
      }
      cpu->A -= daa_adj;
    }
    b_set_off(cpu->F, F_H);
    b_set(cpu->F, F_Z, !cpu->A);
    break;
  case PUSH:
    word = get_qq(cpu, inst->args[0].value.byte);
    mem_write(cpu->memory_c,cpu->SP-1,  upper(word));
    mem_write(cpu->memory_c,cpu->SP-2, lower(word));
    cpu->SP-=2;
    break;
  case POP:
    set_qq(cpu, inst->args[0].value.byte, bytes_to_word(mem_read(cpu->memory_c,cpu->SP), mem_read(cpu->memory_c, cpu->SP+1)));
    cpu->SP+=2;
    break;
  case HALT:
    cpu->is_halted = 1;
    break;
  case STOP:
    // TODO: implement
    //    return -1;
    break;
  default:
    return -1;
  }
  cpu->PC += inst->bytelen;

  if (inst->type != EI && cpu->enable_interrupt_after_next_inst == 1) {
    cpu->enable_interrupt_after_next_inst = 0;
    if (inst->type != DI)
      cpu->IME = 1;
  }

  return inst->m_cycles;
}
