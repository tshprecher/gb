#ifndef CPU_H
#define CPU_H

#include <stdint.h>

// TODO: probably should have debugger support injected here
struct cpu
{
    // registers
    int8_t A, F,
        B, C,
        D, E,
        H, L;
    uint16_t PC, SP;

    // flags
    int8_t Z : 1;
    int8_t N : 1;
    int8_t FH : 1;
    int8_t CY : 1;

    // pointer to memory of length 0x10000
    char *ram;
};

struct cpu cpu_copy(struct cpu *);
int cpu_exec(struct cpu *, int);

#endif