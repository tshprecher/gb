#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "cpu.h"
#include "memory.h"
#include "inst.h"
#include "timing.h"

// flags
#define F_Z 7
#define F_N 6
#define F_H 5
#define F_CY 4

#define flag_get(c, f) ((c->F>>f) & 1)
#define flag_on(c, f) (c->F |= (1 << f))
#define flag_off(c, f) (c->F &= ~(1 << f))
#define flag_set(c, f, v) (c->F = (c->F & ~(1 << f)) |  ((v) << f))

#define upper_8(v) ((v >> 8) & 0xFF)
#define lower_8(v) (v & 0xFF)

#define nn_lower(i, a) (i->args[a].value.word[0])
#define nn_upper(i, a) (i->args[a].value.word[1])
#define bytes_to_word(l, u) ((u16) (u << 8 | l))
#define nn_to_word(i, a) bytes_to_word(nn_lower(i,a),nn_upper(i,a))

// hard coded interrupt handler addresses by interrupt priority
static u16 interrupt_handlers[] = {0x0040, 0x0048, 0x0050, 0x0058, 0x0060};

// TODO: should all enums be capital?
void interrupt(struct interrupt_controller *ic, enum Interrupt interrupt) {
  printf("debug: interrupt %d\n", interrupt);
  ic->IF |= (1 << interrupt);
}

static inline void check_interrupt(struct cpu *cpu) {
  //printf("debug: t_cycles_since_last_inst: %d, is_halted: %d\n", cpu->t_cycles_since_last_inst, cpu->is_halted);
  if (cpu->t_cycles_since_last_inst == 0 &&
      (cpu->interrupt_c->IF & cpu->interrupt_c->IE)) {
    printf("\tdebug: noticed interrupt: IF -> 0x%02X, IE -> 0x%02X\n", cpu->interrupt_c->IF, cpu->interrupt_c->IE);

    u16 *handler_addr = interrupt_handlers;
    u8 mask = 1;
    while (mask < (1<<5)) {
      if ((cpu->interrupt_c->IF & mask) && (cpu->interrupt_c->IE & mask))
       break;
      mask <<= 1;
      handler_addr++;
    }

    printf("\tdebug: interrupt found mask 0x%02X\n", mask);

    if (cpu->IME) {
      printf("\tdebug: interrupt IME is true, so handle and mark unhalted\n");

      cpu->interrupt_c->IF ^= mask;
      cpu->is_halted = 0;
      cpu->IME = 0;
      printf("\tdebug: handling interrupt addr 0x%04X\n", *handler_addr);

      // TODO: consolidate PUSH into one operation?
      //       printf("\tdebug: pushing interrupt return address: 0x%04X\n", cpu->PC);
      mem_write(cpu->memory_c,cpu->SP-1,  upper_8(cpu->PC));
      mem_write(cpu->memory_c,cpu->SP-2, lower_8(cpu->PC));
      cpu->SP -= 2;
      cpu->PC = *handler_addr;
      cpu->interrupt_t_cycles = 6 << 2; // TODO: is this correct for every interrupt type?

      printf("\tdebug: exit interrupt: IF -> 0x%02X, IE -> 0x%02X\n", cpu->interrupt_c->IF, cpu->interrupt_c->IE);
    } else {
      printf("\tdebug: interrupt IME is false\n");

      if (cpu->is_halted) {
       printf("\tdebug: interrupt is halted\n");
       // PC is already set to the instruction after halt, just unhalt
       cpu->is_halted = 0;
      } else {
       // do noting here (TODO: remove this?)
      }
      printf("\tdebug: exit interrupt: IF -> 0x%02X, IE -> 0x%02X\n", cpu->interrupt_c->IF, cpu->interrupt_c->IE);
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

static u16 regs_to_word(struct cpu * cpu, u8 upper, u8 lower) {
  u8 l, u;
  l = *(reg(cpu, lower));
  u = *(reg(cpu, upper));
  return bytes_to_word(l, u);
}

static void word_to_regs(struct cpu * cpu, u16 word, u8 upper, u8 lower) {
  *(reg(cpu, lower)) = lower_8(word);
  *(reg(cpu, upper)) = upper_8(word);
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
    cpu->A = upper_8(word);
    cpu->F = lower_8(word) & 0xF0; // flags register alyways has lower nibble 0
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
  if ( (cc == NZ && !flag_get(cpu, F_Z)) ||
       (cc == Z && flag_get(cpu, F_Z)) ||
       (cc == NC && !flag_get(cpu, F_CY)) ||
       (cc == YC && flag_get(cpu, F_CY)))
    return 1;

  return 0;
}

// imitates the Z80-based 4-bit ALU for easier tracking of half carry and carry bits
static u8 alu_4bit_add(u8 op1, u8 op2, u8 in_carry, u8 *out_carry) {
  u16 result = (op1 & 0x0F) + (op2 & 0x0F) + (in_carry ? 1 : 0);
  *out_carry = (result & (1 << 4)) > 0;
  return result & 0x0F;
}

// handles ALU operations for addition, properly assigning the flags
static u8 alu_add(struct cpu *cpu, u8 op1, u8 op2, u8 in_carry) {
  u8 h_carry, cy_carry;
  u8 lower4 = alu_4bit_add(op1, op2, in_carry, &h_carry);
  u8 upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  u8 result = lower4+(upper4 << 4);

  flag_set(cpu, F_Z, !result);
  flag_off(cpu, F_N);
  flag_set(cpu, F_H, h_carry);
  flag_set(cpu, F_CY, cy_carry);

  return result;
}

// handles ALU operations for subtraction, properly assigning the flags
static u8 alu_sub(struct cpu *cpu, u8 op1, u8 op2) {
  op2 = ~op2;
  u8 h_carry, cy_carry;
  u8 lower4 = alu_4bit_add(op1, op2, 1, &h_carry); // 2's complement means carry is 1 after bits flipped
  u8 upper4 = alu_4bit_add(op1 >> 4, op2 >> 4, h_carry, &cy_carry);
  u8 result = lower4+(upper4 << 4);

  flag_set(cpu, F_Z, !result);
  flag_on(cpu, F_N);
  flag_set(cpu, F_H, !h_carry);
  flag_set(cpu, F_CY, !cy_carry);

  return result;
}

void init_cpu(struct cpu *cpu) {
  cpu->PC = 0x150;
  cpu->SP = 0xFFFE;
}

// FOR DEBUGGING: remove
static void cpu_to_str(char *buf, struct cpu *cpu) {
  int pos = sprintf(buf, "{A: 0x%02X,B: 0x%02X,", cpu->A, cpu->B);
  pos += sprintf(buf+pos, "C: 0x%02X,D: 0x%02X,", cpu->C, cpu->D);
  pos += sprintf(buf+pos, "E: 0x%02X,F: 0x%02X,", cpu->E, cpu->F);
  pos += sprintf(buf+pos, "H: 0x%02X,L: 0x%02X,", cpu->H, cpu->L);
  sprintf(buf+pos, "PC: 0x%04X,SP: 0x%04X}", cpu->PC, cpu->SP);
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

  // 4 clock "t" cycles per machine "m" cycle
  s8 exec_cycle = (cpu->next_inst->cycles << 2);
  cpu->t_cycles_since_last_inst++;
  if (cpu->t_cycles_since_last_inst == exec_cycle) {
    char buf[128];
    inst_to_str(cpu->next_inst, buf);
    printf("(DEBUG): [t: %d, f: %d]  0x%04X\t%s\n", cpu->next_inst->type, cpu->next_inst->form, cpu->PC, buf);
    int cycles = cpu_exec_instruction(cpu, cpu->next_inst);
    cpu_to_str(buf, cpu);
    printf("\t(DEBUG): cpu -> %s\n", buf);

    if (cycles < 0) {
      char buf[16];
      inst_to_str(cpu->next_inst, buf);
      fprintf(stderr, "error: could not execute instr '%s' @ 0x%04X\n",
	      buf, cpu->PC);
      exit(1);
    }

    if (exec_cycle > cpu->t_cycles_since_last_inst) {
      cpu->t_cycles_since_last_inst -= exec_cycle; // go negative to catch up
    } else {
      cpu->t_cycles_since_last_inst = 0;
    }
  }
}


struct dst {
  u8 *reg;
  u16 addr;
  struct mem_controller *mc;
};


// TODO: find out why removing these declarations craps out the linker
void dst_init_reg(struct dst *dst, struct cpu * cpu, u8 r);
void dst_init_addr(struct dst *dst, struct mem_controller *mc, u16 addr);
u8 dst_val(struct dst *dst);
void dst_assign(struct dst *dst, s8 byte);

inline void dst_init_reg(struct dst *dst, struct cpu * cpu, u8 r) {
  dst->reg = reg(cpu, r);
}

inline void dst_init_addr(struct dst *dst, struct mem_controller *mc, u16 addr) {
  dst->reg = NULL;
  dst->addr = addr;
  dst->mc = mc;
}

inline u8 dst_val(struct dst *dst) {
  return dst->reg ? *dst->reg : mem_read(dst->mc, dst->addr);
}

inline void dst_assign(struct dst *dst, s8 byte) {
  dst->reg ? *dst->reg = byte : mem_write(dst->mc, dst->addr, byte);
}

// cpu_exec_instruction takes a executes the instruction.
// It returns the number of cycles run, -1 on error
// TODO: could use a few more macros for clarity?
// TODO: should this return 0 cycles on error instead? makes some sense
int cpu_exec_instruction(struct cpu *cpu , struct inst *inst) {
  u16 hl, word;
  u8 flag_cy=0, flag_h=0, flag_n=0, flag_z=0, byte,
    lower, upper, dd_or_ss, daa_adj, lower_nib, upper_nib;
  struct dst dst = {0};
  s8 e;
  switch (inst->type) {
  case NOP:
    break;
  case ADC:
    switch (inst->form) {
    case 0:
      cpu->A = alu_add(cpu, cpu->A, *(reg(cpu, inst->args[0].value.byte)), flag_get(cpu, F_CY));
      break;
    case 1:
      cpu->A = alu_add(cpu, cpu->A, inst->args[0].value.byte, flag_get(cpu, F_CY));
      break;
    case 2:
      cpu->A = alu_add(cpu, cpu->A, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), flag_get(cpu, F_CY));
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
      alu_add(cpu, lower_8(cpu->SP), e, 0);

      flag_off(cpu, F_Z);
      flag_off(cpu, F_N);
      cpu->SP += e;
      break;
    case 4:
      flag_z = flag_get(cpu, F_Z); // store before to set back at end
      word = get_dd_or_ss(cpu, inst->args[0].value.byte);

      cpu->L = alu_add(cpu, cpu->L, lower_8(word), 0);
      cpu->H = alu_add(cpu, cpu->H, upper_8(word), flag_get(cpu, F_CY));

      flag_off(cpu, F_N);
      flag_set(cpu, F_Z, flag_z);
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
    flag_cy = flag_get(cpu, F_CY);
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
      flag_cy = flag_get(cpu, F_CY);
      flag_h = flag_get(cpu, F_H);

      cpu->A = alu_sub(cpu, cpu->A, 1);

      flag_cy |= flag_get(cpu, F_CY);
      flag_h |= flag_get(cpu, F_H);

      flag_set(cpu, F_CY, flag_cy);
      flag_set(cpu, F_H, flag_h);
    }
    break;
  case SCF:
    // TODO: test
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_on(cpu, F_CY);
    break;
  case INC:
    switch (inst->form) {
    case 0:
      flag_cy = flag_get(cpu, F_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_add(cpu, *(reg(cpu, inst->args[0].value.byte)), 1, 0);
      flag_set(cpu, F_CY, flag_cy);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) + 1);
      break;
    case 2:
      flag_cy = flag_get(cpu, F_CY);
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL),
		alu_add(cpu, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), 1, 0));
      flag_set(cpu, F_CY, flag_cy);
      break;
    }
    break;
  case DEC:
    switch (inst->form) {
    case 0:
      flag_cy = flag_get(cpu, F_CY);
      *(reg(cpu, inst->args[0].value.byte)) = alu_sub(cpu, *(reg(cpu, inst->args[0].value.byte)), 1);
      flag_set(cpu, F_CY, flag_cy);
      break;
    case 1:
      dd_or_ss = inst->args[0].value.byte;
      set_dd_or_ss(cpu, dd_or_ss, get_dd_or_ss(cpu, dd_or_ss) - 1);
      break;
    case 2:
      flag_cy = flag_get(cpu, F_CY);
      mem_write(cpu->memory_c, regs_to_word(cpu, rH, rL),
		alu_sub(cpu, mem_read(cpu->memory_c, regs_to_word(cpu, rH, rL)), 1));
      flag_set(cpu, F_CY, flag_cy);
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
    cpu->A ? flag_off(cpu, F_Z) : flag_on(cpu, F_Z);
    flag_off(cpu, F_N);
    flag_on(cpu, F_H);
    flag_off(cpu, F_CY);
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
    cpu->A ? flag_off(cpu, F_Z) : flag_on(cpu, F_Z); // TODO: create another macro to set/unset based on value?
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_off(cpu, F_CY);
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
    cpu->A ? flag_off(cpu, F_Z) : flag_on(cpu, F_Z);
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_off(cpu, F_CY);
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
    flag_on(cpu, F_N);
    flag_on(cpu, F_H);
    break;
  case RLCA:
    flag_cy = (cpu->A & 0x80) != 0;
    cpu->A = (cpu->A << 1) | flag_cy;
    flag_off(cpu, F_Z);
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = (byte & 0x80) != 0;
    byte = (byte << 1) | flag_cy;
    dst_assign(&dst, byte);
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
    break;
  case RLA:
    flag_cy = (cpu->A & 0x80) != 0;
    cpu->A = (cpu->A << 1) | flag_get(cpu, F_CY);
    flag_off(cpu, F_Z); // TODO: be able to set/clear multiple flags on a single line?
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = (byte & 0x80) != 0;
    byte = (byte << 1) | flag_get(cpu, F_CY);
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
    break;
  case RRCA:
    flag_cy = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (flag_cy << 7);
    flag_off(cpu, F_Z);
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, flag_cy);
    break;
  case RRA:
    flag_cy = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (flag_get(cpu, F_CY) << 7);
    flag_off(cpu, F_Z);
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = byte & 1;
    byte = (byte >> 1) | (flag_cy << 7);
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = byte & 1;
    byte = (byte >> 1) | (flag_get(cpu, F_CY) << 7);
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = (byte & 0x80) != 0;
    byte <<= 1;
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = byte & 1;
    byte = (byte >> 1) | (byte & 0x80);
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
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
    flag_cy = byte & 1;
    byte >>= 1;
    dst_assign(&dst, byte);

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_Z, !byte);
    flag_set(cpu, F_CY, flag_cy);
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

    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_off(cpu, F_CY);
    flag_set(cpu, F_Z, !byte);
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
    flag_set(cpu, F_Z, !(byte & (1 << inst->args[0].value.byte)));
    flag_off(cpu, F_N);
    flag_on(cpu, F_H);
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
    byte |= (1 << inst->args[0].value.byte);
    dst_assign(&dst, byte);
    break;
  case DI:
    cpu->IME = 0;
    break;
  case EI:
    cpu->IME = 1;
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
    byte &= ~(1 << inst->args[0].value.byte);
    dst_assign(&dst, byte);
    break;
  case JP:
    switch (inst->form) {
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
      cpu->PC = regs_to_word(cpu, rH, rL);
      return inst->cycles;
    }
    break;
  case JR:
    switch (inst->form) {
    case 0:
      e = (s8) inst->args[0].value.byte;
      cpu->PC = cpu->PC + e + 2;
      return inst->cycles;
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
      mem_write(cpu->memory_c, cpu->SP-1, upper_8((cpu->PC + 3)));
      mem_write(cpu->memory_c, cpu->SP-2, lower_8((cpu->PC + 3)));
      cpu->SP-=2;
      cpu->PC = nn_to_word(inst, 0);
      return inst->cycles;
    case 1:
      if (is_cond_true(cpu, inst->args[0].value.byte)) {
	mem_write(cpu->memory_c, cpu->SP-1, upper_8((cpu->PC + 3)));
	mem_write(cpu->memory_c, cpu->SP-2, lower_8((cpu->PC + 3)));
	cpu->SP-=2;
	cpu->PC = nn_to_word(inst, 1);
	return 6;
	}
      break;
    }
    break;
  case CCF:
    // TODO: test
    flag_off(cpu, F_N);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, !flag_get(cpu, F_CY));
    break;
  case RET:
    switch (inst->form) {
    case 0:
      cpu->PC = bytes_to_word(mem_read(cpu->memory_c, cpu->SP), mem_read(cpu->memory_c, cpu->SP+1));
      cpu->SP+=2;
      return inst->cycles;
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
    return inst->cycles;
  case RST:
    mem_write(cpu->memory_c, cpu->SP-1, upper_8((cpu->PC+1)));
    mem_write(cpu->memory_c, cpu->SP-2, lower_8((cpu->PC+1)));
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
    return inst->cycles;
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
      mem_write(cpu->memory_c, word, lower_8(cpu->SP));
      mem_write(cpu->memory_c, word+1, upper_8(cpu->SP));
      break;
    }
    break;
  case LDHL:
    e = (s8) inst->args[0].value.byte;
    alu_add(cpu, lower_8(cpu->SP), e, 0);

    flag_off(cpu, F_Z);
    flag_off(cpu, F_N);
    word_to_regs(cpu, cpu->SP + e, rH, rL);
    break;
  case DAA:
    // direct implementation from the table defined in the Nintendo/Z80 manual.
    daa_adj = 0;
    lower_nib = cpu->A & 0x0F;
    upper_nib = (cpu->A>>4) & 0x0F;
    if (!flag_get(cpu, F_N)) {
      if (!flag_get(cpu, F_CY)) {
	if (!flag_get(cpu, F_H)) {
	  if (upper_nib <= 9 && lower_nib <= 9) {
	    daa_adj = 0;
	    flag_cy = 0;
	  } else if (upper_nib <= 8 && lower_nib >= 0xA) {
	    daa_adj = 0x06;
	    flag_cy = 0;
	  } else if (upper_nib >= 0xA && lower_nib <= 9) {
	    daa_adj = 0x60;
	    flag_cy = 1;
	  } else if (upper_nib >= 0x9 && lower_nib >= 0xA) {
	    daa_adj = 0x66;
	    flag_cy = 1;
	  }
	} else {
	  if (upper_nib <= 9 && lower_nib <= 3) {
	    daa_adj = 0x06;
	    flag_cy = 0;
	  } else if (upper_nib >= 0xA  && lower_nib <= 3) {
	    daa_adj = 0x66;
	    flag_cy = 1;
	  }
	}
      } else {
	if (!flag_get(cpu, F_H)) {
	  if (upper_nib <= 2 && lower_nib <= 9) {
	    daa_adj = 0x60;
	    flag_cy = 1;
	  } else if (upper_nib <= 2 && lower_nib >= 0xA) {
	    daa_adj = 0x66;
	    flag_cy = 1;
	  }
	} else {
	  if (upper_nib <= 3 && lower_nib <= 3) {
	    daa_adj = 0x66;
	    flag_cy = 1;
	  }
	}
      }
    } else {
      if (!flag_get(cpu, F_CY)) {
	if (!flag_get(cpu, F_H) && upper_nib <=9 && lower_nib <= 9) {
	  daa_adj = 0;
	  flag_cy =0;
	} else if (flag_get(cpu, F_H) && upper_nib <= 8 && lower_nib >= 6) {
	  daa_adj = 0xFA;
	  flag_cy = 0;
	}
      } else {
	if (!flag_get(cpu, F_H) && upper_nib >= 7 && lower_nib <= 9) {
	  daa_adj = 0xA0;
	  flag_cy = 1;
	} else if (flag_get(cpu, F_H) && upper_nib >= 6 && lower_nib >= 6) {
	  daa_adj = 0x9A;
	  flag_cy = 1;
	}
      }
    }
    flag_n = flag_get(cpu, F_N);
    cpu->A = alu_add(cpu, cpu->A, daa_adj, 0);
    flag_off(cpu, F_H);
    flag_set(cpu, F_CY, flag_cy);
    flag_set(cpu, F_N, flag_n);
    break;
  case PUSH:
    word = get_qq(cpu, inst->args[0].value.byte);
    mem_write(cpu->memory_c,cpu->SP-1,  upper_8(word));
    mem_write(cpu->memory_c,cpu->SP-2, lower_8(word));
    cpu->SP-=2;
    break;
  case POP:
    set_qq(cpu, inst->args[0].value.byte, bytes_to_word(mem_read(cpu->memory_c,cpu->SP), mem_read(cpu->memory_c, cpu->SP+1)));
    cpu->SP+=2;
    break;
  case HALT:
    printf("debug: IME -> 0x%02X, IF -> 0x%02X, IE -> 0x%02X\n", cpu->IME, cpu->interrupt_c->IF, cpu->interrupt_c->IE);
    cpu->is_halted = 1;
    break;
  default:
    return -1;
  }
  cpu->PC += inst->bytelen;
  return inst->cycles;
}
