#ifndef INST_H
#define INST_H

enum inst_type
{
    ADC = 1,
    ADD,
    AND,
    BIT,
    CALL,
    CCF,
    CP,
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

// TODO: deal with copied type
struct inst
{
    enum inst_type type;
    int8_t cond;

    // source and destination registers
    int8_t r_d;
    int8_t r_s;

    // RAM addressing from source and destination registers
    int8_t raddr_d;
    int8_t raddr_s;

    int8_t lit_8bit;      // signed 8 bit literal
    int16_t lit_16bit;    // signed 16-bit literal
    u_int16_t addr_16bit; // 16-bit address
    u_int8_t addr_8bit;   // 8-bit address that's added to 0xFF00

    int8_t incr : 1;
    int8_t decr : 1;
    int8_t has_lit_8bit : 1;
    int8_t has_lit_16bit : 1;
    int8_t has_addr_16bit : 1;
    int8_t has_addr_8bit: 1;

};

// TODO: deal with copying this
enum reg
{
    // 8-bit
    A = 1,
    B,
    C,
    D,
    E,
    H,
    L,

    // 16-bit
    AF,
    BC,
    DE,
    HL,
    PC,
    SP,
};

enum cond
{
    NZ = 1,
    Z,
    NC,
    YC
};

int inst_write(struct inst *, char *);
int inst_decode(unsigned char *, struct inst *);

#endif