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

struct inst_arg {
  enum arg_type type;
  union {
    uint8_t byte;
    uint8_t word[2]; // little endian: first byte is lower part
  } value;
};

struct inst {
  enum inst_type type;
  uint8_t bytelen;
  uint8_t cycles;
  char *bit_pattern;
  char *txt_pattern;

  struct inst_arg args[3];
  uint8_t args_count;
};

enum cond { NZ = 1, Z, NC, YC };


int init_inst_from_bytes(struct inst*, char *);
int inst_to_str(struct inst *, char *);

#endif
