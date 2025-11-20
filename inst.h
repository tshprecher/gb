#ifndef INST_H
#define INST_H

#include <stdint.h>


// enum value for each cpu instruction in lexicographic order
// NOTE: DO NOT REORDER! Tests rely on ADC being the first and XOR
//   being the last.
enum inst_type
{
    ADC = 1,
    ADD,
    AND,
    BIT,
    CALL,
    CCF,
    CP,
    CPA,
    CPL,
    DAA,
    DB,
    DEC,
    DI,
    EI,
    HALT,
    INC,
    JP,
    JR,
    LD,
    LDHL,
    NOP,
    OR,
    POP,
    PUSH,
    RES,
    RET,
    RETI,
    RL,
    RLA,
    RLC,
    RLCA,
    RR,
    RRA,
    RRC,
    RRCA,
    RST,
    SBC,
    SCF,
    SET,
    SLA,
    SRA,
    SRL,
    STOP,
    SUB,
    SWAP,
    XOR
};

enum arg_type {
  R = 1,
  B,
  T,
  CC,
  DD,
  QQ,
  SS,
  E,
  N,
  NN
};

enum reg { // Z80 index of each register in opcodes
  rB=0,
  rC,
  rD,
  rE,
  rH,
  rL,
  _na_,
  rA
};

struct inst_arg {
  enum arg_type type;
  union {
    uint8_t byte;
    uint8_t word[2]; // little endian: first byte is lower part
  } value;
};

struct inst {
  // indicates the type of instructions (e.g. "ADD", "LD",...)
  enum inst_type type;

  // assigned during creation to distinguish
  // between the multiple forms of an instruction type.
  // I prefer to avoid doing string comparisons of the
  // txt_pattern or fancier bitmask schemes like a PLA
  // in hardware.
  uint8_t subtype;

  // the length of the opcode in bytes
  uint8_t bytelen;

  // the number of cycles needed to execute. note, few
  // instructions take variable number of cycles. that's
  // delegated to the cpu at execution time.
  uint8_t cycles;

  // human readable bit pattern used to decode an instruction from bytes
  char *bit_pattern;

  // human readable text used to initiate an instruction from asm
  char *txt_pattern;

  // store up to 3 args associated with the instruction
  struct inst_arg args[3];
  uint8_t args_count;
};

enum cond { NZ = 1, Z, NC, YC };


int init_inst_from_bytes(struct inst*, void *);
int init_inst_from_asm(struct inst*, char *);

// TODO: swap the order of these args
// What does this return?
int inst_to_str(struct inst *, char *);

#endif
