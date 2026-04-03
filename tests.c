#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "inst.h"
#include "cpu.h"
#include "memory.h"
#include "testing.c"

#define F_ON 0xF0
#define F_OFF 0x00

static int cpu_equals_ignore_flags(struct cpu first, struct cpu second) {
  if (first.A == second.A &&
      first.B == second.B &&
      first.C == second.C &&
      first.D == second.D &&
      first.E == second.E &&
      first.H == second.H &&
      first.L == second.L &&
      first.PC == second.PC &&
      first.SP == second.SP) {
    return 1;
  }
  return 0;
}

static void cpu_to_str(char *buf, struct cpu *cpu) {
  int pos = sprintf(buf, "{A: 0x%02X,B: 0x%02X,", cpu->A, cpu->B);
  pos += sprintf(buf+pos, "C: 0x%02X,D: 0x%02X,", cpu->C, cpu->D);
  pos += sprintf(buf+pos, "E: 0x%02X,F: 0x%02X,", cpu->E, cpu->F);
  pos += sprintf(buf+pos, "H: 0x%02X,L: 0x%02X,", cpu->H, cpu->L);
  sprintf(buf+pos, "PC: 0x%04X,SP: 0x%04X}", cpu->PC, cpu->SP);
}

// returns 1 on failure
int test_cpu_exec() {
  struct test_suite ts = {0};
  suite_start(&ts, "cpu execution");

  char buf1[128], buf2[128];

  struct test {
    char *asm_command;
    uint8_t initial_flags;
    struct cpu initial_cpu;

    // expected state after instruction exec
    int cycles;
    struct cpu expected;
    uint8_t expected_flags;
    uint16_t modified_addrs[2];
    uint8_t modified_addr_values[2];
  } tests[] = {
    // LD

    {"LD A, E", F_OFF, {.E = 0x01}, 1, {.A = 0x01, .E = 0x01, .PC = 0x0001}, 0, {}, {}},
    {"LD A, E", F_ON, {.E = 0x01}, 1, {.A = 0x01, .E = 0x01, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD B, 0xFF", F_OFF, {}, 2, {.B = 0xFF, .PC = 0x0002}, 0, {}, {}},
    {"LD B, 0xFF", F_ON, {}, 2, {.B = 0xFF, .PC = 0x0002}, 0xF0, {}, {}},

    {"LD C, (HL)", F_OFF, {.C = 0xAA, .H = 0x10, .L = 0x11}, 2, {.H = 0x10, .L = 0x11, .PC = 0x0001}, 0, {}, {}},
    {"LD C, (HL)", F_ON, {.C = 0xAA, .H = 0x10, .L = 0x11}, 2, {.H = 0x10, .L = 0x11, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD (HL), D", F_OFF, {.D = 0x1F, .H = 0x80, .L = 0x11}, 2, {.D = 0x1F, .H = 0x80, .L = 0x11, .PC = 0x0001},  0, {0x8011}, {0x1F}},
    {"LD (HL), D", F_ON, {.D = 0x1F, .H = 0x80, .L = 0x11}, 2, {.D = 0x1F, .H = 0x80, .L = 0x11, .PC = 0x0001},  0xF0, {0x8011}, {0x1F}},

    {"LD (HL), 0xAB", F_OFF, {.H = 0x80, .L = 0x11}, 3, {.H = 0x80, .L = 0x11, .PC = 0x0002}, 0, {0x8011}, {0xAB}},
    {"LD (HL), 0xAB", F_ON, {.H = 0x80, .L = 0x11}, 3, {.H = 0x80, .L = 0x11, .PC = 0x0002}, 0xF0, {0x8011}, {0xAB}},

    {"LD A, (BC)", F_OFF, {.A = 0xAA, .B = 0x0A, .C = 0xA0}, 2, {.B = 0x0A, .C = 0xA0, .PC = 0x0001},  0, {}, {}},
    {"LD A, (BC)", F_ON, {.A = 0xAA, .B = 0x0A, .C = 0xA0}, 2, {.B = 0x0A, .C = 0xA0, .PC = 0x0001},  0xF0, {}, {}},

    {"LD A, (DE)", F_OFF, {.A = 0xBB, .D = 0x0D, .E = 0xE0}, 2, {.D = 0x0D, .E = 0xE0, .PC = 0x0001}, 0, {}, {}},
    {"LD A, (DE)", F_ON, {.A = 0xBB, .D = 0x0D, .E = 0xE0}, 2, {.D = 0x0D, .E = 0xE0, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD (BC), A", F_OFF, {.A = 0xAA, .B = 0x8B, .C=0x0C}, 2, {.A = 0xAA, .B = 0x8B, .C=0x0C, .PC = 0x0001},  0, {0x8B0C}, {0xAA}},
    {"LD (BC), A", F_ON, {.A = 0xAA, .B = 0x8B, .C=0x0C}, 2, {.A = 0xAA, .B = 0x8B, .C=0x0C, .PC = 0x0001}, 0xF0, {0x8B0C}, {0xAA}},

    {"LD (DE), A", F_OFF, {.A = 0xAA, .D = 0x8D, .E=0x0E}, 2, {.A = 0xAA, .D = 0x8D, .E=0x0E, .PC = 0x0001},  0, {0x8D0E}, {0xAA}},
    {"LD (DE), A", F_ON, {.A = 0xAA, .D = 0x8D, .E=0x0E}, 2, {.A = 0xAA, .D = 0x8D, .E=0x0E, .PC = 0x0001}, 0xF0, {0x8D0E}, {0xAA}},

    {"LD A, (C)", F_OFF, {.A = 0xBB, .C = 0xCC}, 2, {.C = 0xCC, .PC = 0x0001}, 0, {}, {}},
    {"LD A, (C)", F_ON, {.A = 0xBB, .C = 0xCC}, 2, {.C = 0xCC, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD (C), A", F_OFF, {.A = 0xAA, .C = 0xCC}, 2, {.A = 0xAA, .C = 0xCC, .PC = 0x0001},  0, {0xFFCC}, {0xAA}},
    {"LD (C), A", F_ON, {.A = 0xAA, .C = 0xCC}, 2, {.A = 0xAA, .C = 0xCC, .PC = 0x0001}, 0xF0, {0xFFCC}, {0xAA}},

    {"LD SP, HL", F_OFF, {.H = 0xFA, .L = 0xCE}, 2, {.H = 0xFA, .L = 0xCE, .SP = 0xFACE, .PC = 0x0001},  0, {}, {}},
    {"LD SP, HL", F_ON, {.H = 0xFA, .L = 0xCE}, 2, {.H = 0xFA, .L = 0xCE, .SP = 0xFACE, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD DE, 0xFACE", F_OFF, {}, 3, {.D = 0xFA, .E = 0xCE, .PC = 0x0003}, 0, {}, {}},
    {"LD DE, 0xFACE", F_ON, {}, 3, {.D = 0xFA, .E = 0xCE, .PC = 0x0003}, 0xF0, {}, {}},

    {"LD (0xFACE), SP", F_OFF, {.SP = 0xDEAF}, 5, {.SP = 0xDEAF, .PC = 0x0003},  0, {0xFACE, 0xFACF}, {0xAF, 0xDE}},
    {"LD (0xFACE), SP", F_ON, {.SP = 0xDEAF}, 5, {.SP = 0xDEAF, .PC = 0x0003},  0xF0, {0xFACE, 0xFACF}, {0xAF, 0xDE}},

    {"LD A, (0xFF34)", F_OFF, {.A = 0xAA}, 4, {.PC = 0x0003}, 0, {}, {}},
    {"LD A, (0xFF34)", F_ON, {.A = 0xAA}, 4, {.PC = 0x0003}, 0xF0, {}, {}},

    {"LD A, (0x34)", F_OFF, {.A = 0xAA}, 3, {.PC = 0x0002}, 0, {}, {}},
    {"LD A, (0x34)", F_ON, {.A = 0xAA}, 3, {.PC = 0x0002}, 0xF0, {}, {}},

    {"LD (0xAA34), A", F_OFF, {.A = 0xAA}, 4, {.A = 0xAA, .PC = 0x0003}, 0, {0xAA34}, {0xAA}},
    {"LD (0xAA34), A", F_ON, {.A = 0xAA}, 4, {.A = 0xAA, .PC = 0x0003}, 0xF0, {0xAA34}, {0xAA}},

    {"LD (0x34), A", F_OFF, {.A = 0xAA}, 3, {.A = 0xAA, .PC = 0x0002},  0, {0xFF34}, {0xAA}},
    {"LD (0x34), A", F_ON, {.A = 0xAA}, 3, {.A = 0xAA, .PC = 0x0002}, 0xF0, {0xFF34}, {0xAA}},

    {"LD A, (HLI)", F_OFF, {.A = 0xAA, .H = 0x01, .L = 0xFF}, 2, {.H = 0x02, .L = 0x00, .PC = 0x0001}, 0, {}, {}},
    {"LD A, (HLI)", F_ON, {.A = 0xAA, .H = 0x01, .L = 0xFF}, 2, {.H = 0x02, .L = 0x00, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD A, (HLD)", F_OFF, {.A = 0xAA, .H = 0x02, .L = 0x00}, 2, {.H = 0x01, .L = 0xFF, .PC = 0x0001}, 0, {}, {}},
    {"LD A, (HLD)", F_ON, {.A = 0xAA, .H = 0x02, .L = 0x00}, 2, {.H = 0x01, .L = 0xFF, .PC = 0x0001}, 0xF0, {}, {}},

    {"LD (HLI), A", F_OFF, {.A = 0xAA, .H = 0x81, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x81, .L = 0xDE, .PC = 0x0001}, 0, {0x81DD}, {0xAA}},
    {"LD (HLI), A", F_ON, {.A = 0xAA, .H = 0x81, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x81, .L = 0xDE, .PC = 0x0001}, 0xF0, {0x81DD}, {0xAA}},

    {"LD (HLD), A", F_OFF, {.A = 0xAA, .H = 0x81, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x81, .L = 0xDC, .PC = 0x0001}, 0, {0x81DD}, {0xAA}},
    {"LD (HLD), A", F_ON, {.A = 0xAA, .H = 0x81, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x81, .L = 0xDC, .PC = 0x0001}, 0xF0, {0x81DD}, {0xAA}},


    // LDHL

    {"LDHL SP, 2", F_OFF, {.SP = 0xFFF8}, 3, {.H = 0xFF, .L = 0xFA, .PC = 0x0002, .SP = 0xFFF8}, 0, {}, {}},
    {"LDHL SP, 2", F_ON, {.SP = 0xFFF8}, 3, {.H = 0xFF, .L = 0xFA, .PC = 0x0002, .SP = 0xFFF8}, 0, {}, {}},

    {"LDHL SP, -1", F_OFF, {.SP = 0xFACE}, 3, {.H = 0xFA, .L = 0xCD, .PC = 0x0002, .SP = 0xFACE}, 0, {}, {}},
    {"LDHL SP, -1", F_ON, {.SP = 0xFACE}, 3, {.H = 0xFA, .L = 0xCD, .PC = 0x0002, .SP = 0xFACE}, 0, {}, {}},

    {"LDHL SP, 1", F_OFF, {.SP = 0x0FFF}, 3, {.H = 0x10, .L = 0x00, .PC = 0x0002, .SP = 0x0FFF}, 0x20, {}, {}},
    {"LDHL SP, 1", F_ON, {.SP = 0x0FFF}, 3, {.H = 0x10, .L = 0x00, .PC = 0x0002, .SP = 0x0FFF}, 0x20, {}, {}},

    {"LDHL SP, 15", F_OFF, {.SP = 0xFFF1}, 3, {.H = 0x00, .L = 0x00, .PC = 0x0002, .SP = 0xFFF1}, 0x30, {}, {}},
    {"LDHL SP, 15", F_ON, {.SP = 0xFFF1}, 3, {.H = 0x00, .L = 0x00, .PC = 0x0002, .SP = 0xFFF1}, 0x30, {}, {}},

    // PUSH

    {"PUSH BC", F_OFF, {.B = 0xFA, .C = 0xCE, .SP = 0xFFFE}, 4, {.B = 0xFA, .C = 0xCE, .PC = 0x0001, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0xFA, 0xCE}},
    {"PUSH BC", F_ON, {.B = 0xFA, .C = 0xCE, .SP = 0xFFFE}, 4, {.B = 0xFA, .C = 0xCE, .PC = 0x0001, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0xFA, 0xCE}},

    {"PUSH DE", F_OFF, {.D = 0xDE, .E = 0xAD, .SP = 0xFFFE}, 4, {.D = 0xDE, .E = 0xAD, .PC = 0x0001, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0xDE, 0xAD}},
    {"PUSH DE", F_ON, {.D = 0xDE, .E = 0xAD, .SP = 0xFFFE}, 4, {.D = 0xDE, .E = 0xAD, .PC = 0x0001, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0xDE, 0xAD}},

    {"PUSH HL", F_OFF, {.H = 0xBE, .L = 0xEF, .SP = 0xFFFE}, 4, {.H = 0xBE, .L = 0xEF, .PC = 0x0001, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0xBE, 0xEF}},
    {"PUSH HL", F_ON, {.H = 0xBE, .L = 0xEF, .SP = 0xFFFE}, 4, {.H = 0xBE, .L = 0xEF, .PC = 0x0001, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0xBE, 0xEF}},

    {"PUSH AF", F_OFF, {.A = 0xBE, .SP = 0xFFFE}, 4, {.A = 0xBE, .PC = 0x0001, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0xBE, 0x00}},
    {"PUSH AF", F_ON, {.A = 0xBE, .SP = 0xFFFE}, 4, {.A = 0xBE,  .PC = 0x0001, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0xBE, 0xF0}},
    {"PUSH AF", 0xFF, {.A = 0xBE, .SP = 0xFFFE}, 4, {.A = 0xBE,  .PC = 0x0001, .SP = 0xFFFC}, 0xFF, {0xFFFD, 0xFFFC}, {0xBE, 0xF0}},

    // POP

    {"POP BC", F_OFF, {.B = 0xFA, .C = 0xCE, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE},  0, {}, {}},
    {"POP BC", F_ON, {.B = 0xFA, .C = 0xCE, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE}, 0xF0, {}, {}},

    {"POP DE", F_OFF, {.D = 0xDE, .E = 0xAD, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE}, 0, {}, {}},
    {"POP DE", F_ON, {.D = 0xDE, .E = 0xAD, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE}, 0xF0, {}, {}},

    {"POP HL", F_OFF, {.H = 0xBE, .L = 0xEF, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE},  0, {}, {}},
    {"POP HL", F_ON, {.H = 0xBE, .L = 0xEF, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE}, 0xF0, {}, {}},

    {"POP AF", F_OFF, {.A = 0xFE, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE},  0, {}, {}},
    {"POP AF", F_ON, {.A = 0xFE, .SP = 0xFFFC}, 3, {.PC = 0x0001, .SP = 0xFFFE},  0, {}, {}},

    // ADC

    {"ADC A, E", F_OFF, {.A = 0xE1, .E = 0x0F}, 1, {.A = 0xF0, .E = 0x0F, .PC = 0x0001}, 0x20, {}, {}},
    {"ADC A, E", F_ON,  {.A = 0xE1, .E = 0x0F}, 1, {.A = 0xF1, .E = 0x0F, .PC = 0x0001}, 0x20, {}, {}},

    {"ADC A, 0x3B", F_OFF, {.A = 0xE1}, 2, {.A = 0x1C, .PC = 0x0002}, 0x10, {}, {}},
    {"ADC A, 0x3B", F_ON, {.A = 0xE1}, 2, {.A = 0x1D, .PC = 0x0002}, 0x10, {}, {}},

    {"ADC A, 0x00", F_OFF, {.A = 0x0F}, 2, {.A = 0x0F, .PC = 0x0002}, 0, {}, {}},
    {"ADC A, 0x00", F_ON, {.A = 0x0F}, 2, {.A = 0x10, .PC = 0x0002}, 0x20, {}, {}},

    {"ADC A, (HL)", F_OFF, {.A = 0xE1}, 2, {.A = 0xE1,.F = 0x00, .PC = 0x0001}, 0, {}, {}},
    {"ADC A, (HL)", F_ON, {.A = 0xE1}, 2, {.A = 0xE2,.F = 0x00, .PC = 0x0001}, 0, {}, {}},

    {"ADC A, L", F_OFF, {.A = 0xE1, .L = 0x1E}, 1, {.A = 0xFF, .L = 0x1E, .PC = 0x0001}, 0, {}, {}},
    {"ADC A, L", F_ON, {.A = 0xE1, .L = 0x1E}, 1, {.A = 0x00, .L = 0x1E, .PC = 0x0001}, 0xB0, {}, {}},

    // ADD

    {"ADD A, B", F_OFF, {.A = 0x3A, .B = 0xC6}, 1, {.A = 0x00, .B = 0xC6, .PC = 0x0001}, 0xB0, {}, {}},
    {"ADD A, B", F_ON, {.A = 0x3A, .B = 0xC6}, 1, {.A = 0x00, .B = 0xC6, .PC = 0x0001}, 0xB0, {}, {}},

    {"ADD A, C", F_OFF, {.A = 0xF0, .C = 0x10}, 1, {.A = 0x00, .C = 0x10, .PC = 0x0001}, 0x90, {}, {}},
    {"ADD A, C", F_ON, {.A = 0xF0, .C = 0x10}, 1, {.A = 0x00, .C = 0x10, .PC = 0x0001}, 0x90, {}, {}},

    {"ADD A, D", F_OFF, {.A = 0x00, .D = 0x01}, 1, {.A = 0x01, .D = 0x01, .PC = 0x0001}, 0, {}, {}},
    {"ADD A, D", F_ON, {.A = 0x00, .D = 0x01}, 1, {.A = 0x01, .D = 0x01, .PC = 0x0001}, 0, {}, {}},

    {"ADD A, E", F_OFF, {.A = 0x0F, .E = 0x01}, 1, {.A = 0x10, .E = 0x01, .PC = 0x0001}, 0x20, {}, {}},
    {"ADD A, E", F_ON, {.A = 0x0F, .E = 0x01}, 1, {.A = 0x10, .E = 0x01, .PC = 0x0001}, 0x20, {}, {}},


    {"ADD A, (HL)", F_OFF, {.A = 0x00}, 2, {.A = 0x00, .PC = 0x0001}, 0x80, {}, {}},
    {"ADD A, (HL)", F_ON, {.A = 0x00}, 2, {.A = 0x00, .PC = 0x0001}, 0x80, {}, {}},

    {"ADD A, 0x01", F_OFF, {.A = 0x00}, 2, {.A = 0x01, .PC = 0x0002}, 0, {}, {}},
    {"ADD A, 0x01", F_ON, {.A = 0x00}, 2, {.A = 0x01, .PC = 0x0002}, 0, {}, {}},

    {"ADD A, 0xA0", F_OFF, {.A = 0x0F}, 2, {.A = 0xAF, .PC = 0x0002}, 0, {}, {}},
    {"ADD A, 0xA0", F_ON, {.A = 0x0F}, 2, {.A = 0xAF, .PC = 0x0002}, 0, {}, {}},

    {"ADD SP, 2", F_OFF, {.SP = 0xFFF8}, 4, {.SP = 0xFFFA, .PC = 0x0002}, {}, {}},
    {"ADD SP, 2", F_ON, {.SP = 0xFFF8}, 4, {.SP = 0xFFFA, .PC = 0x0002}, {}, {}},

    {"ADD SP, -2", F_OFF, {.SP = 0x1000}, 4, {.SP = 0x0FFE, .PC = 0x0002}, 0x20, {}, {}},
    {"ADD SP, -2", F_ON, {.SP = 0x1000}, 4, {.SP = 0x0FFE, .PC = 0x0002}, 0x20, {}, {}},

    {"ADD SP, -1", F_OFF, {.SP = 0x0000}, 4, {.SP = 0xFFFF, .PC = 0x0002}, 0x30, {}, {}},
    {"ADD SP, -1", F_ON, {.SP = 0x0000}, 4, {.SP = 0xFFFF, .PC = 0x0002}, 0x30, {}, {}},

    {"ADD SP, 1", F_OFF, {.SP = 0xFFFF}, 4, {.SP = 0x0000, .PC = 0x0002}, 0x30, {}, {}},
    {"ADD SP, 1", F_ON, {.SP = 0xFFFF}, 4, {.SP = 0x0000, .PC = 0x0002}, 0x30, {}, {}},

    {"ADD HL, BC", F_OFF, {.B = 0x06, .C = 0x05, .H = 0x8A, .L = 0x23}, 2,
     {.B = 0x06, .C = 0x05, .H = 0x90, .L = 0x28, .F = 0x20, .PC = 0x0001}, 0x20, {}, {}},
    {"ADD HL, BC", F_ON, {.B = 0x06, .C = 0x05, .H = 0x8A, .L = 0x23}, 2,
     {.B = 0x06, .C = 0x05, .H = 0x90, .L = 0x28, .PC = 0x0001}, 0xA0, {}, {}},

    {"ADD HL, HL", F_OFF, {.H = 0x8A, .L = 0x23}, 2,
     {.H = 0x14, .L = 0x46, .PC = 0x0001}, 0x30, {}, {}},
    {"ADD HL, HL", F_ON, {.H = 0x8A, .L = 0x23}, 2,
     {.H = 0x14, .L = 0x46, .PC = 0x0001}, 0xB0, {}, {}},

    // AND

    {"AND L", F_OFF, {.A = 0x5A, .L = 0x3F}, 1, {.A = 0x1A, .L = 0x3F, .PC = 0x0001}, 0x20, {}, {}},
    {"AND L", F_ON, {.A = 0x5A, .L = 0x3F}, 1, {.A = 0x1A, .L = 0x3F, .PC = 0x0001}, 0x20, {}, {}},

    {"AND 0x38", F_OFF, {.A = 0x5A, .L = 0x3F}, 2, {.A = 0x18, .L = 0x3F, .PC = 0x0002}, 0x20, {}, {}},
    {"AND 0x38", F_ON, {.A = 0x5A, .L = 0x3F}, 2, {.A = 0x18, .L = 0x3F, .PC = 0x0002}, 0x20, {}, {}},

    {"AND (HL)", F_OFF, {.A = 0x5A, .L = 0x3F}, 2, {.L = 0x3F, .PC = 0x0001}, 0xA0, {}, {}},
    {"AND (HL)", F_ON, {.A = 0x5A, .L = 0x3F}, 2, {.L = 0x3F, .PC = 0x0001}, 0xA0, {}, {}},

    // INC

    {"INC A", F_OFF, {.A = 0xFF}, 1, {.PC = 0x0001}, 0xA0, {}, {}},
    {"INC A", F_ON, {.A = 0xFF}, 1, {.PC = 0x0001}, 0xB0, {}, {}},

    {"INC B", F_OFF, {}, 1, {.B = 0x01, .PC = 0x0001}, 0, {}, {}},
    {"INC B", F_ON, {}, 1, {.B = 0x01, .PC = 0x0001}, 0x10, {}, {}},

    {"INC BC", F_OFF, {.B = 0xF0, .C = 0x08}, 2, {.B = 0xF0, .C = 0x09, .PC = 0x0001}, 0, {}, {}},
    {"INC BC", F_ON, {.B = 0xF0, .C = 0x08}, 2, {.B = 0xF0, .C = 0x09, .PC = 0x0001}, 0xF0, {}, {}},

    {"INC DE", F_OFF, {.D = 0xFF, .E = 0xFF}, 2, {.PC = 0x0001}, 0, {}, {}},
    {"INC DE", F_ON, {.D = 0xFF, .E = 0xFF}, 2, {.PC = 0x0001}, 0xF0, {}, {}},

    {"INC DE", F_OFF, {.D = 0x23, .E = 0x5F}, 2, {.D = 0x23, .E = 0x60, .PC = 0x0001}, 0, {}, {}},
    {"INC DE", F_ON, {.D = 0x23, .E = 0x5F}, 2, {.D = 0x23, .E = 0x60, .PC = 0x0001}, 0xF0, {}, {}},

    {"INC SP", F_OFF, {.SP = 0xFF00}, 2, {.SP = 0xFF01, .PC = 0x0001}, 0, {}, {}},
    {"INC SP", F_ON, {.SP = 0xFF00}, 2, {.SP = 0xFF01, .PC = 0x0001}, 0xF0, {}, {}},

    {"INC SP", F_OFF, {.SP = 0xFFFF}, 2, {.SP = 0x0, .PC = 0x0001}, 0, {}, {}},
    {"INC SP", F_ON, {.SP = 0xFFFF}, 2, {.SP = 0x0, .PC = 0x0001}, 0xF0, {}, {}},

    {"INC (HL)", F_OFF, {.H = 0xFA, .L = 0xCE}, 3, {.H = 0xFA, .L = 0xCE, .PC = 0x0001}, 0, {0xFACE}, {0x01}},
    {"INC (HL)", F_ON, {.H = 0xFA, .L = 0xCE}, 3, {.H = 0xFA, .L = 0xCE, .PC = 0x0001}, 0x10, {0xFACE}, {0x01}},

    // DEC

    {"DEC L", F_OFF, {.L = 0x01}, 1, {.F = 0xC0, .PC = 0x0001}, 0xC0, {}, {}},
    {"DEC L", F_ON, {.L = 0x01}, 1, {.F = 0xC0, .PC = 0x0001}, 0xD0, {}, {}},

    {"DEC DE", F_OFF, {.D = 0x23, .E = 0x5F}, 2, {.D = 0x23, .E = 0x5E, .PC = 0x0001}, 0, {}, {}},
    {"DEC DE", F_ON, {.D = 0x23, .E = 0x5F}, 2, {.D = 0x23, .E = 0x5E, .PC = 0x0001}, 0xF0,{}, {}},

    {"DEC SP", F_OFF, {.SP = 0xFF00}, 2, {.SP = 0xFEFF, .PC = 0x0001}, 0, {}, {}},
    {"DEC SP", F_ON, {.SP = 0xFF00}, 2, {.SP = 0xFEFF, .PC = 0x0001}, 0xF0, {}, {}},

    {"DEC SP", F_OFF, {.SP = 0x0000}, 2, {.SP = 0xFFFF, .PC = 0x0001}, 0, {}, {}},
    {"DEC SP", F_ON, {.SP = 0x0000}, 2, {.SP = 0xFFFF, .PC = 0x0001}, 0xF0, {}, {}},

    {"DEC (HL)", F_OFF, {.H = 0xFA, .L = 0xCE}, 3, {.H = 0xFA, .L = 0xCE, .PC = 0x0001}, 0x60, {0xFACE}, {0xFF}},
    {"DEC (HL)", F_ON, {.H = 0xFA, .L = 0xCE}, 3, {.H = 0xFA, .L = 0xCE,  .PC = 0x0001}, 0x70, {0xFACE}, {0xFF}},

    // OR

    {"OR A", F_OFF, {.A = 0x5A}, 1, {.A = 0x5A, .PC = 0x0001}, 0, {}, {}},
    {"OR A", F_ON, {.A = 0x5A}, 1, {.A = 0x5A, .PC = 0x0001}, 0, {}, {}},

    {"OR 0x03", F_OFF, {.A = 0x5A}, 2, {.A = 0x5B, .PC = 0x0002}, 0, {}, {}},
    {"OR 0x03", F_ON, {.A = 0x5A}, 2, {.A = 0x5B, .PC = 0x0002}, 0, {}, {}},

    {"OR 0x00", F_OFF, {}, 2, {.PC = 0x0002}, 0x80, {}, {}},
    {"OR 0x00", F_ON, {}, 2, {.PC = 0x0002}, 0x80, {}, {}},

    {"OR (HL)", F_OFF, {.A = 0x5A}, 2, {.A = 0x5A, .PC = 0x0001}, 0, {}, {}},
    {"OR (HL)", F_ON, {.A = 0x5A}, 2, {.A = 0x5A, .PC = 0x0001}, 0, {}, {}},

    // RLCA

    {"RLCA", F_OFF, {.A = 0x85}, 1, {.A = 0x0B, .PC = 0x0001}, 0x10, {}, {}},
    {"RLCA", F_ON, {.A = 0x85}, 1, {.A = 0x0B, .PC = 0x0001}, 0x10, {}, {}},

    // RLA

    {"RLA", F_OFF, {.A = 0x95}, 1, {.A = 0x2A, .PC = 0x0001}, 0x10, {}, {}},
    {"RLA", F_ON, {.A = 0x95}, 1, {.A = 0x2B, .PC = 0x0001}, 0x10, {}, {}},

    {"RLA", F_OFF, {.A = 0x15}, 1, {.A = 0x2A, .PC = 0x0001}, 0, {}, {}},
    {"RLA", F_ON, {.A = 0x15}, 1, {.A = 0x2B, .PC = 0x0001}, 0, {}, {}},

    // RRCA

    {"RRCA", F_OFF, {.A = 0x3B}, 1, {.A = 0x9D, .PC = 0x0001}, 0x10, {}, {}},
    {"RRCA", F_ON, {.A = 0x3B}, 1, {.A = 0x9D, .PC = 0x0001}, 0x10, {}, {}},

    {"RRCA", F_OFF, {.A = 0x32}, 1, {.A = 0x19, .PC = 0x0001}, 0x00, {}, {}},
    {"RRCA", F_ON, {.A = 0x32}, 1, {.A = 0x19, .PC = 0x0001}, 0x00, {}, {}},

    // RRA

    {"RRA", F_OFF, {.A = 0x81}, 1, {.A = 0x40, .PC = 0x0001}, 0x10, {}, {}},
    {"RRA", F_ON, {.A = 0x81}, 1, {.A = 0xC0, .PC = 0x0001}, 0x10, {}, {}},

    {"RRA", F_OFF, {.A = 0x80}, 1, {.A = 0x40, .PC = 0x0001}, 0, {}, {}},
    {"RRA", F_ON, {.A = 0x80}, 1, {.A = 0xC0, .PC = 0x0001}, 0, {}, {}},

    // RLC

    {"RLC B", F_OFF, {.B = 0x85}, 2, {.B = 0x0B, .PC = 0x0002}, 0x10, {}, {}},
    {"RLC B", F_ON, {.B = 0x85}, 2, {.B = 0x0B, .PC = 0x0002}, 0x10, {}, {}},

    {"RLC B", F_OFF, {.B = 0x45}, 2, {.B = 0x8A, .PC = 0x0002}, 0, {}, {}},
    {"RLC B", F_ON, {.B = 0x45}, 2, {.B = 0x8A, .PC = 0x0002}, 0, {}, {}},

    {"RLC A", F_OFF, {.A = 0x80}, 2, {.A = 0x01, .PC = 0x0002}, 0x10, {}, {}},
    {"RLC A", F_ON, {.A = 0x80}, 2, {.A = 0x01, .PC = 0x0002}, 0x10, {}, {}},

    {"RLC (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"RLC (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .F = 0x80, .PC = 0x0002}, 0x80, {}, {}},

    // RL

    {"RL L", F_OFF, {.L = 0x80}, 2, {.PC = 0x0002}, 0x90, {}, {}},
    {"RL L", F_ON, {.L = 0x80}, 2, {.L = 0x01, .PC = 0x0002}, 0x10, {}, {}},

    {"RL D", F_OFF, {.D = 0x11}, 2, { .D = 0x22, .PC = 0x0002}, 0, {}, {}},
    {"RL D", F_ON, {.D = 0x11}, 2, { .D = 0x23, .PC = 0x0002}, 0, {}, {}},

    {"RL (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"RL (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0, {}, {}},

    // RRC

    {"RRC C", F_OFF, {.C = 0x01}, 2, {.C = 0x80, .PC = 0x0002}, 0x10, {}, {}},
    {"RRC C", F_ON, {.C = 0x01}, 2, {.C = 0x80, .PC = 0x0002}, 0x10, {}, {}},

    {"RRC (HL)", F_OFF, { .C = 0x01, .H = 0x80, .L = 0x00}, 4, {.C = 0x01, .H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"RRC (HL)", F_ON, { .C = 0x01, .H = 0x80, .L = 0x00}, 4, {.C = 0x01, .H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},

    // RR

    {"RR A", F_OFF, {.A = 0x01}, 2, {.PC = 0x0002}, 0x90, {}, {}},
    {"RR A", F_ON, {.A = 0x01}, 2, { .A = 0x80, .PC = 0x0002}, 0x10, {}, {}},

    {"RR L", F_OFF, {.L = 0x8A}, 2, { .L = 0x45, .PC = 0x0002}, 0, {}, {}},
    {"RR L", F_ON, {.L = 0x8A}, 2, { .L = 0xC5, .PC = 0x0002}, 0, {}, {}},

    {"RR (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"RR (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x00, {0x8000}, {0x80}},

    // SLA

    {"SLA D", F_OFF, {.D = 0x80}, 2, {.PC = 0x0002}, 0x90, {}, {}},
    {"SLA D", F_ON, {.D = 0x80}, 2, {.PC = 0x0002}, 0x90, {}, {}},

    {"SLA E", F_OFF, {.E = 0xFF}, 2, { .E = 0xFE, .PC = 0x0002}, 0x10, {}, {}},
    {"SLA E", F_ON, {.E = 0xFF}, 2, { .E = 0xFE, .PC = 0x0002}, 0x10, {}, {}},

    {"SLA (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"SLA (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},

    // SRA

    {"SRA D", F_OFF, {.D = 0x8A}, 2, { .D = 0xC5, .PC = 0x0002}, 0, {}, {}},
    {"SRA D", F_ON, {.D = 0x8A}, 2, { .D = 0xC5, .PC = 0x0002}, 0, {}, {}},

    {"SRA H", F_OFF, {.H = 0x01}, 2, {.PC = 0x0002}, 0x90, {}, {}},
    {"SRA H", F_ON, {.H = 0x01}, 2, {.PC = 0x0002}, 0x90, {}, {}},

    {"SRA (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"SRA (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},

    // SRL

    {"SRL A", F_OFF, {.A = 0x01}, 2, {.PC = 0x0002}, 0x90, {}, {}},
    {"SRL A", F_ON, {.A = 0x01}, 2, {.PC = 0x0002}, 0x90, {}, {}},

    {"SRL C", F_OFF, {.C = 0xFF}, 2, { .C = 0x7F, .PC = 0x0002}, 0x10, {}, {}},
    {"SRL C", F_ON, {.C = 0xFF}, 2, { .C = 0x7F, .PC = 0x0002}, 0x10, {}, {}},

    {"SRL (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"SRL (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},

    // SWAP

    {"SWAP A", F_OFF,  {.A = 0x00}, 2, {.PC = 0x0002}, 0x80, {}, {}},
    {"SWAP A", F_ON, {.A = 0x00}, 2, {.PC = 0x0002}, 0x80, {}, {}},

    {"SWAP H", F_OFF, {.H = 0xF0}, 2, { .H = 0x0F, .PC = 0x0002}, 0, {}, {}},
    {"SWAP H", F_ON, {.H = 0xF0}, 2, { .H = 0x0F, .PC = 0x0002}, 0, {}, {}},

    {"SWAP (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},
    {"SWAP (HL)", F_ON, {.H = 0x80, .L = 0x00}, 4, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0x80, {}, {}},

    // SUB

    {"SUB E", F_OFF, {.A = 0x3E, .E = 0x3E}, 1, {.E = 0x3E, .PC = 0x0001}, 0xC0, {}, {}},
    {"SUB E", F_ON, {.A = 0x3E, .E = 0x3E}, 1, {.E = 0x3E, .PC = 0x0001}, 0xC0, {}, {}},

    {"SUB 0x0F", F_OFF, {.A = 0x3E}, 2, {.A = 0x2F, .PC = 0x0002}, 0x60, {}, {}},
    {"SUB 0x0F", F_ON, {.A = 0x3E}, 2, {.A = 0x2F, .PC = 0x0002}, 0x60, {}, {}},

    {"SUB 0x40", F_OFF, {.A = 0x3E}, 2, {.A = 0xFE, .PC = 0x0002}, 0x50, {}, {}},
    {"SUB 0x40", F_ON, {.A = 0x3E}, 2, {.A = 0xFE, .PC = 0x0002}, 0x50, {}, {}},

    // SBC

    {"SBC A, H", F_OFF, {.A = 0x3B, .H = 0x2A}, 1, {.A = 0x11, .H = 0x2A, .PC = 0x0001}, 0x40, {}, {}},
    {"SBC A, H", F_ON,{.A = 0x3B, .H = 0x2A}, 1, {.A = 0x10, .H = 0x2A, .F = 0x40, .PC = 0x0001}, 0x40, {}, {}},

    {"SBC A, 0x3A", F_OFF, {.A = 0x3B}, 2, {.A = 1, .PC = 0x0002}, 0x40, {}, {}},
    {"SBC A, 0x3A", F_ON, {.A = 0x3B}, 2, {.A = 0, .PC = 0x0002}, 0xC0, {}, {}},

    {"SBC A, 0x4F", F_OFF, {.A = 0x3B}, 2, {.A = 0xEC, .PC = 0x0002}, 0x70, {}, {}},
    {"SBC A, 0x4F", F_ON, {.A = 0x3B}, 2, {.A = 0xEB, .PC = 0x0002}, 0x70, {}, {}},

    {"SBC A, 0x00", F_OFF, {.A = 0x10}, 2, {.A = 0x10, .PC = 0x0002}, 0x40, {}, {}},
    {"SBC A, 0x00", F_ON, {.A = 0x10}, 2, {.A = 0x0F, .PC = 0x0002}, 0x60, {}, {}},

    {"SBC A, 0x01", F_OFF, {.A = 0x10}, 2, {.A = 0x0F, .PC = 0x0002}, 0x60, {}, {}},
    {"SBC A, 0x01", F_ON, {.A = 0x10}, 2, {.A = 0x0E, .PC = 0x0002}, 0x60, {}, {}},

    {"SBC A, (HL)", F_OFF, {.A = 0x3B, .H = 0x80, .L = 0x00}, 2, {.A = 0x3B, .H = 0x80, .L = 0x00, .PC = 0x0001}, 0x40, {}, {}},
    {"SBC A, (HL)", F_ON, {.A = 0x3B, .H = 0x80, .L = 0x00}, 2, {.A = 0x3A, .H = 0x80, .L = 0x00, .PC = 0x0001}, 0x40, {}, {}},

    // XOR

    {"XOR A", F_OFF, {.A = 0xFF}, 1, {.PC = 0x0001}, 0x80, {}, {}},
    {"XOR A", F_ON, {.A = 0xFF}, 1, {.PC = 0x0001}, 0x80, {}, {}},

    {"XOR 0x0F", F_OFF, {.A = 0xFF}, 2, {.A = 0xF0, .PC = 0x0002}, 0, {}, {}},
    {"XOR 0x0F", F_ON, {.A = 0xFF}, 2, {.A = 0xF0, .PC = 0x0002}, 0, {}, {}},

    {"XOR 0x8A", F_OFF, {.A = 0xFF}, 2, {.A = 0x75, .PC = 0x0002}, 0, {}, {}},
    {"XOR 0x8A", F_ON, {.A = 0xFF}, 2, {.A = 0x75, .PC = 0x0002}, 0, {}, {}},

    {"XOR (HL)", F_OFF, {.A = 0xFF, .H = 0x80, .L = 0x00}, 2, {.A = 0xFF, .H = 0x80, .L = 0x00,.PC = 0x0001}, 0, {}, {}},
    {"XOR (HL)", F_ON, {.A = 0xFF, .H = 0x80, .L = 0x00}, 2, {.A = 0xFF, .H = 0x80, .L = 0x00,.PC = 0x0001}, 0, {}, {}},

    // CP

    {"CP B", F_OFF, {.A = 0x3C, .B = 0x2F}, 1, {.A = 0x3C, .B = 0x2F, .PC = 0x0001}, 0x60, {}, {}},
    {"CP B", F_ON, {.A = 0x3C, .B = 0x2F}, 1, {.A = 0x3C, .B = 0x2F, .PC = 0x0001}, 0x60, {}, {}},

    {"CP 0x3C", F_OFF, {.A = 0x3C}, 2, {.A = 0x3C, .PC = 0x0002}, 0xC0, {}, {}},
    {"CP 0x3C", F_ON, {.A = 0x3C}, 2, {.A = 0x3C, .PC = 0x0002}, 0xC0, {}, {}},

    {"CP 0x40", F_OFF, {.A = 0x3C}, 2, {.A = 0x3C, .PC = 0x0002}, 0x50, {}, {}},
    {"CP 0x40", F_ON, {.A = 0x3C}, 2, {.A = 0x3C, .PC = 0x0002}, 0x50, {}, {}},

    {"CP 0x94", F_OFF, {.A = 0x94}, 2, {.A = 0x94, .PC = 0x0002}, 0xC0, {}, {}},
    {"CP 0x94", F_ON, {.A = 0x94}, 2, {.A = 0x94, .PC = 0x0002}, 0xC0, {}, {}},

    {"CP (HL)", F_OFF, {.A = 0x3C, .H = 0x80, .L = 0x00}, 2, {.A = 0x3C, .H = 0x80, .L = 0x00, .PC = 0x0001}, 0x40, {}, {}},
    {"CP (HL)", F_ON, {.A = 0x3C, .H = 0x80, .L = 0x00}, 2, {.A = 0x3C, .H = 0x80, .L = 0x00, .PC = 0x0001}, 0x40, {}, {}},

    // CPL

    {"CPL", F_OFF, {.A = 0x35}, 1, {.A = 0xCA, .PC = 0x0001}, 0x60, {}, {}},
    {"CPL", F_ON, {.A = 0x35}, 1, {.A = 0xCA, .PC = 0x0001}, 0xF0, {}, {}},

    // BIT

    {"BIT 7, A", F_OFF, {.A = 0x80}, 2, {.A = 0x80, .PC = 0x0002}, 0x20, {}, {}},
    {"BIT 7, A", F_ON, {.A = 0x80}, 2, {.A = 0x80, .PC = 0x0002}, 0x30, {}, {}},

    {"BIT 4, L", F_OFF, {.L = 0xEF}, 2, {.L = 0xEF, .PC = 0x0002}, 0xA0, {}, {}},
    {"BIT 4, L", F_ON, {.L = 0xEF}, 2, {.L = 0xEF, .PC = 0x0002}, 0xB0, {}, {}},

    {"BIT 0, (HL)", F_OFF, {.H = 0x80, .L = 0x00}, 3, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0xA0, {}, {}},
    {"BIT 0, (HL)", F_ON, {.H = 0x80, .L = 0x00}, 3, {.H = 0x80, .L = 0x00, .PC = 0x0002}, 0xB0, {}, {}},

    // SET

    {"SET 3, A", F_OFF, {.A = 0x80}, 2, {.A = 0x88, .PC = 0x0002}, 0, {}, {}},
    {"SET 3, A", F_ON, {.A = 0x80}, 2, {.A = 0x88, .PC = 0x0002}, 0xF0, {}, {}},

    {"SET 7, L", F_OFF, {.L = 0x3B}, 2, {.L = 0xBB, .PC = 0x0002}, 0, {}, {}},
    {"SET 7, L", F_ON, {.L = 0x3B}, 2, {.L = 0xBB, .PC = 0x0002}, 0xF0, {}, {}},

    {"SET 2, (HL)", F_OFF, {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, 0, {0xFACE}, {0x04}},
    {"SET 2, (HL)", F_ON, {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, 0xF0, {0xFACE}, {0x04}},

    // RES

    {"RES 7, A", F_OFF, {.A = 0x80}, 2, {.PC = 0x0002}, 0, {}, {}},
    {"RES 7, A", F_ON, {.A = 0x80}, 2, {.PC = 0x0002}, 0xF0, {}, {}},

    {"RES 1, L", F_OFF, {.L = 0x3B}, 2, {.L = 0x39, .PC = 0x0002}, 0, {}, {}},
    {"RES 1, L", F_ON, {.L = 0x3B}, 2, {.L = 0x39, .PC = 0x0002}, 0xF0, {}, {}},

    {"RES 7, (HL)", F_OFF, {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, 0, {0xFACE}, {0x00}},
    {"RES 7, (HL)", F_ON, {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, 0xF0, {0xFACE}, {0x00}},

    // JP

    {"JP 0x8000", F_OFF, {.F = 0xF0}, 4, {.F = 0xF0, .PC = 0x8000}, 0, {}, {}},
    {"JP 0x8000", F_ON, {.F = 0xF0}, 4, {.F = 0xF0, .PC = 0x8000}, 0xF0, {}, {}},

    {"JP NZ, 0x8000", F_OFF, {}, 4, {.PC = 0x8000}, 0, {}, {}},
    {"JP NZ, 0x8000", F_ON, {}, 3, {.PC = 0x0003}, 0xF0, {}, {}},

    {"JP Z, 0x8000", F_OFF, {}, 3, {.PC = 0x0003}, 0,{}, {}},
    {"JP Z, 0x8000", F_ON, {}, 4, {.PC = 0x8000}, 0xF0, {}, {}},

    {"JP C, 0x8000", F_OFF, {}, 3, {.PC = 0x0003}, 0, {}, {}},
    {"JP C, 0x8000", F_ON, {}, 4, {.PC = 0x8000}, 0xF0, {}, {}},

    {"JP NC, 0x8000", F_OFF, {}, 4, {.PC = 0x8000}, 0, {}, {}},
    {"JP NC, 0x8000", F_ON, {}, 3, {.PC = 0x0003}, 0xF0, {}, {}},

    {"JP HL", F_OFF, {.H = 0xFA, .L = 0xCE}, 1, {.H = 0xFA, .L = 0xCE, .PC = 0xFACE}, 0, {}, {}},
    {"JP HL", F_ON, {.H = 0xFA, .L = 0xCE}, 1, {.H = 0xFA, .L = 0xCE, .PC = 0xFACE}, 0xF0, {}, {}},

    // JR

    {"JR -126", F_OFF, {.PC = 0x1000}, 3, {.PC = 0x0F82}, 0, {}, {}},
    {"JR -126", F_ON, {.PC = 0x1000}, 3, {.PC = 0x0F82}, 0xF0, {}, {}},

    {"JR -10", F_OFF, {.PC = 0x1000}, 3, {.PC = 0xFF6}, 0, {}, {}},
    {"JR -10", F_ON, {.PC = 0x1000}, 3, {.PC = 0xFF6}, 0xF0, {}, {}},

    {"JR 10", F_OFF, {.PC = 0x1000}, 3, {.PC = 0x100A}, 0, {}, {}},
    {"JR 10", F_ON, {.PC = 0x1000}, 3, {.PC = 0x100A}, 0xF0, {}, {}},

    {"JR 129", F_OFF, {.PC = 0x1000}, 3, {.PC = 0x1081}, 0, {}, {}},
    {"JR 129", F_ON, {.PC = 0x1000}, 3, {.PC = 0x1081}, 0xF0, {}, {}},

    {"JR NZ, 10", F_OFF, {}, 3, {.PC = 0x000A}, 0, {}, {}},
    {"JR NZ, 10", F_ON, {}, 2, {.PC = 0x0002}, 0xF0, {}, {}},

    {"JR Z, -10", F_OFF, {.PC = 0x8000}, 2, {.PC = 0x8002}, 0, {}, {}},
    {"JR Z, -10", F_ON, {.PC = 0x8000}, 3, {.PC = 0x7FF6}, 0xF0, {}, {}},

    {"JR NC, 10", F_OFF, {}, 3, {.PC = 0x000A}, 0, {}, {}},
    {"JR NC, 10", F_ON, {}, 2, {.PC = 0x0002}, 0xF0, {}, {}},

    {"JR C, -10", F_OFF, {.PC = 0x8000}, 2, {.PC = 0x8002}, 0, {}, {}},
    {"JR C, -10", F_ON, {.PC = 0x8000}, 3, {.PC = 0x7FF6}, 0xF0, {}, {}},

    // CALL

    {"CALL 0x1234", F_OFF, {.PC = 0x8000, .SP = 0xFFFE}, 6, {.PC = 0x1234, .SP = 0xFFFC}, 0, {0xFFFC, 0xFFFD}, {0x03, 0x80}},
    {"CALL 0x1234", F_ON, {.PC = 0x8000, .SP = 0xFFFE}, 6, {.PC = 0x1234, .SP = 0xFFFC}, 0xF0, {0xFFFC, 0xFFFD}, {0x03, 0x80}},

    {"CALL NZ, 0x1234", F_OFF, {.PC = 0x7FFD, .SP = 0xFFFE}, 6, {.PC = 0x1234, .SP = 0xFFFC},0 , {0xFFFC, 0xFFFD}, {0x00, 0x80}},
    {"CALL NZ, 0x1234", F_ON, {.PC = 0x7FFD, .SP = 0xFFFE}, 3, {.PC = 0x8000, .SP = 0xFFFE}, 0xF0, {}, {}},

    {"CALL Z, 0x1234", F_OFF, {.PC = 0x8000, .SP = 0xFFFE}, 3, {.PC = 0x8003, .SP = 0xFFFE}, 0, {}, {}},
    {"CALL Z, 0x1234", F_ON, {.PC = 0x8000, .SP = 0xFFFE}, 6, {.PC = 0x1234, .SP = 0xFFFC}, 0xF0, {0xFFFC, 0xFFFD}, {0x03, 0x80}},

    // RET

    {"RET", F_OFF, {.PC = 0x8000, .SP = 0xFFFC}, 4, { .PC = 0x0000, .SP = 0xFFFE}, 0, {}, {} },
    {"RET", F_ON, {.PC = 0x8000, .SP = 0xFFFC}, 4, { .PC = 0x0000, .SP = 0xFFFE}, 0xF0, {}, {} },

    {"RET NZ", F_OFF, {.PC = 0x9000, .SP = 0xFFFC}, 5, {.PC = 0x0000, .SP = 0xFFFE}, 0, {}, {} },
    {"RET NZ", F_ON, {.PC = 0x9000, .SP = 0xFFFC}, 2, {.PC = 0x9001, .SP = 0xFFFC}, 0xF0, {}, {} },

    {"RET Z", F_OFF, {.PC = 0x9000, .SP = 0xFFFC}, 2, {.PC = 0x9001, .SP = 0xFFFC}, 0, {}, {} },
    {"RET Z", F_ON, {.PC = 0x9000, .SP = 0xFFFC}, 5, {.PC = 0x0000, .SP = 0xFFFE}, 0xF0, {}, {} },

    // RST

    {"RST 0", F_OFF, {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0000, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 0", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0000, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 1", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0008, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 1", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0008, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 2", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0010, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 2", F_ON, {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0010, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 3", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0018, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 3", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0018, .SP = 0xFFFC}, 0xF0,{0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 4", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0020, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 4", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0020, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 5", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0028, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 5", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0028, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 6", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0030, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 6", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0030, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    {"RST 7", F_OFF,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0038, .SP = 0xFFFC}, 0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},
    {"RST 7", F_ON,  {.PC = 0x9000, .SP = 0xFFFE}, 4, {.PC = 0x0038, .SP = 0xFFFC}, 0xF0, {0xFFFD, 0xFFFC}, {0x90, 0x01}},

    // DAA

    {"DAA", F_OFF, {.A = 0x7D, .B = 0x38}, 1, {.A = 0x83, .B = 0x38, .PC = 0x0001}, 0, {}, {}},
    {"DAA", 0xE0, {.A = 0x4B, .B = 0x38}, 1, {.A = 0x45, .B = 0x38, .PC = 0x0001}, 0x40, {}, {}},

  };

  for (int t = 0; t < sizeof(tests) / sizeof(struct test); t++) {
    struct inst inst = {0};
    struct test tst = tests[t];
    struct cpu cpu = tst.initial_cpu;
    cpu.F = tst.initial_flags;
    struct mem_controller mc = {0};
    memset(mc.ram, 0, sizeof(mc.ram)); // TODO: remove this line?

    struct rom rom = {0};

    rom.num_banks = 1;
    uint8_t rom_mem[0x8000] = {0};
    struct inst rom_cached_insts[0x8000] = {0};
    uint8_t rom_is_cached_bitmap[0x8000 >> 3] = {0};
    rom.mem = rom_mem;
    rom.cached_insts = rom_cached_insts;
    rom.is_cached_bitmap = rom_is_cached_bitmap;

    mc.rom = &rom;
    cpu.memory_c = &mc;
    char test_name[64] = {0};
    sprintf(test_name, "%s (init Flags = 0x%02X)", tst.asm_command, cpu.F);
    suite_test_start(&ts, test_name);
    if (!init_inst_from_asm(&inst, tst.asm_command)) {
      suite_test_error(&ts, "\t\tcould not parse asm '%s'\n", tst.asm_command);
      suite_test_end(&ts);
      continue;
    }
    int cycles = cpu_exec_instruction(&cpu, &inst);
    if (cycles != tst.cycles) {
      suite_test_error(&ts, "\tfound cycles:\t%d\n\t\texpected:\t%d\n", cycles, tst.cycles);
    }
    if (!cpu_equals_ignore_flags(cpu, tst.expected)) {
      cpu_to_str(buf1, &cpu);
      cpu_to_str(buf2, &tst.expected);
      suite_test_error(&ts, "\t\tfound cpu:\t%s\n\t\texpected:\t%s\n", buf1, buf2);
    }
    if (cpu.F != tst.expected_flags) {
      suite_test_error(&ts, "\t\tfound flags:\t0x%02X\n\t\texpected:\t0x%02X\n", cpu.F, tst.expected_flags);
    }
    if ((tst.modified_addrs[0] || tst.modified_addrs[1]) &&
	((mem_read(cpu.memory_c, tst.modified_addrs[0]) != tst.modified_addr_values[0]) ||
	 (mem_read(cpu.memory_c, tst.modified_addrs[1]) != tst.modified_addr_values[1]))
	) {
      suite_test_error(&ts, "\t\tfound ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}\n\t\texpected ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}\n",
		       tst.modified_addrs[0],
		       mem_read(cpu.memory_c, tst.modified_addrs[0]),
		       tst.modified_addrs[1],
		       mem_read(cpu.memory_c, tst.modified_addrs[1]),
		       tst.modified_addrs[0],
		       tst.modified_addr_values[0],
		       tst.modified_addrs[1],
		       tst.modified_addr_values[1]);
    }
    suite_test_end(&ts);
  }
  return suite_result(&ts);
}

int test_inst_parsing() {
  struct test_suite ts = {0};
  suite_start(&ts, "instruction parsing");

  DIR *dir;
  struct dirent *entry;
  char *dirname = "testdata/inst";
  dir = opendir(dirname);

  if (dir == NULL) {
    perror("error opening test directory 'testdata/inst'\n");
    return 1;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;

    FILE *f;
    char filename[512];
    char line[32];

    sprintf(filename, "testdata/inst/%s", entry->d_name);
    f = fopen(filename, "r");
    if (f == NULL) {
      fprintf(stderr, "error opening file '%s'\n", filename);
      return 1;
    }

    suite_test_start(&ts, entry->d_name);
    while (fgets(line, 32, f) != NULL) {
      struct inst inst;
      int len;
      line[31] = '\0'; // ensure no buffer overruns
      len = strlen(line);
      if (len == 1)
	continue;
      line[len-1]='\0'; // remove trailing newline
      if (!init_inst_from_asm(&inst, line))
	suite_test_error(&ts, "\t\tcould not parse '%s'\n", line);

    }
    suite_test_end(&ts);
    fclose(f);
  }
  closedir(dir);
  return suite_result(&ts);
}

int test_inst_init() {
  struct test_suite ts = {0};
  suite_start(&ts, "instruction init");

  struct test {
    // input
    char *asm_command;

    // expected output
    enum inst_type type;
    struct inst_arg args[3];
    uint8_t args_count;
  } tests[] = {
    { .asm_command = "ADC A, (HL)", .type = ADC, {}, 0},
    { .asm_command = "ADD A, B", .type = ADD, {{.type = R, .value.byte = 0}} , 1},
    { .asm_command = "ADD A, (HL)", .type = ADD, {} , 0},
    { .asm_command = "ADD A, 0x00", .type = ADD, {{.type = N, .value.byte = 0}} , 1},
    { .asm_command = "ADD A, 0x20", .type = ADD, {{.type = N, .value.byte = 32}} , 1},
    { .asm_command = "BIT 7, A", .type = BIT, {{.type = B, .value.byte = 7}, {.type = R, .value.byte = 7} } , 2},
    { .asm_command = "CALL NZ, 0x1011", .type = CALL, {{.type = CC, .value.byte = 0}, {.type = NN, .value.word = {0x11, 0x10}}} , 2},
    { .asm_command = "CALL 0xface", .type = CALL, {{.type = NN, .value.word = {0xce, 0xfa}}} , 1},
    { .asm_command = "DEC SP", .type = DEC, {{.type = SS, .value.byte = 3}} , 1},
    { .asm_command = "JR 10", .type = JR, {{.type = E, .value.byte = 8}} , 1},
    { .asm_command = "JR -10", .type = JR, {{.type = E, .value.byte = 0xF4}} , 1},
    { .asm_command = "LD DE, 0xdEaD", .type = LD, {{.type = DD, .value.byte = 1}, {.type = NN, .value.word = {0xAD, 0xDE}}} , 2},
    { .asm_command = "LDHL SP, 127", .type = LDHL, {{.type = E, .value.byte = 127}} , 1},
    { .asm_command = "LDHL SP, -127", .type = LDHL, {{.type = E, .value.byte = -127}} , 1},
    { .asm_command = "LDHL SP, -128", .type = LDHL, {{.type = E, .value.byte = -128}} , 1},
    { .asm_command = "POP HL", .type = POP, {{.type = QQ, .value.byte = 2}} , 1},
    { .asm_command = "RST 4", .type = RST, {{.type = T, .value.byte = 4}} , 1},
  };

  for (int t = 0; t < sizeof(tests)/sizeof(struct test); t++) {
    struct inst inst = {0};
    struct test tst = tests[t];

    suite_test_start(&ts, tst.asm_command);

    if (!init_inst_from_asm(&inst, tst.asm_command)) {
      suite_test_error(&ts, "\tcould not initialize instruction\n");
    } else {
      if (tst.type != inst.type) {
	suite_test_error(&ts, "\t\tfound type:\t%d\n\t\texpected type:\t%d\n",
		    inst.type, tst.type);
      }
      if (tst.args_count != inst.args_count) {
	suite_test_error(&ts, "\t\tfound args count:\t%d\n\t\texpected args count:\t%d\n", inst.args_count, tst.args_count);
      } else {
	for (int a = 0; a < tst.args_count; a++) {
	  if (
	       tst.args[a].type != inst.args[a].type ||
	       tst.args[a].value.byte != inst.args[a].value.byte ||
	       tst.args[a].value.word[0] != inst.args[a].value.word[0] ||
	       tst.args[a].value.word[1] != inst.args[a].value.word[1]
	      )
	    {
	      suite_test_error(&ts,
		      "\t\tfound:\t\t{type: %u, value: { byte: %u, word: [%u, %u]}}\n\t\texpected:\t{type: %u, value: { byte: %u, word: [%u, %u]}}\n",
		      inst.args[a].type,
		      inst.args[a].value.byte,
		      inst.args[a].value.word[0],
		      inst.args[a].value.word[1],
		      tst.args[a].type,
		      tst.args[a].value.byte,
		      tst.args[a].value.word[0],
		      tst.args[a].value.word[1]
		      );
	    }
	}
      }
    }
    suite_test_end(&ts);
  }

  return suite_result(&ts);
}


int main () {
  int success = test_inst_parsing();
  success &= test_inst_init();
  success &= test_cpu_exec();

  if (success)
    print_green("SUCCESS!\n");
  else
    print_red("FAILED\n");

  return !success;
}
