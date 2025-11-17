#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "inst.h"


// TODO: consoloidate with inst tests, remove duplication
#define TERM_COLOR_RED "\x1B[31m"
#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_RESET "\033[0m"

#define MAX_ERRORS 100

void print_red(char *str) {
  printf(TERM_COLOR_RED);
  printf("%s", str);
  printf(TERM_COLOR_RESET);
}

void print_green(char *str) {
  printf(TERM_COLOR_GREEN);
  printf("%s", str);
  printf(TERM_COLOR_RESET);
}


// TODO: put this test suite logic in its own struct
char err_msgs[MAX_ERRORS][64];
int err_cnt = 0;

static int cpu_equals(struct cpu first, struct cpu second) {
  if (first.A == second.A &&
      first.B == second.B &&
      first.C == second.C &&
      first.D == second.D &&
      first.E == second.E &&
      first.F == second.F &&
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
  printf("Test cpu execution...\n");
  char buf1[128], buf2[128];
  uint8_t ram[0x10000];

  struct test {
    char *asm_command;
    struct cpu initial;

    // expected state after instruction exec
    int cycles;
    struct cpu expected;
    uint16_t modified_addrs[2];
    uint8_t modified_addr_values[2];
  } tests[] = {
    {"LD A, E", {.E = 0x01}, 1, {.A = 0x01, .E = 0x01, .PC = 0x0001}, {}, {}},
    {"LD B, 0xFF", {}, 2, {.B = 0xFF, .PC = 0x0002}, {}, {}},
    {"LD C, (HL)", {.C = 0xAA, .H = 0x10, .L = 0x11}, 2, {.H = 0x10, .L = 0x11, .PC = 0x0001}, {}, {}},
    {"LD (HL), D", {.D = 0x1F, .H = 0x10, .L = 0x11}, 2, {.D = 0x1F, .H = 0x10, .L = 0x11, .PC = 0x0001}, {0x1011}, {0x1F}},
    {"LD (HL), 0xAB", {.H = 0x10, .L = 0x11}, 3, {.H = 0x10, .L = 0x11, .PC = 0x0002}, {0x1011}, {0xAB}},
    {"LD A, (BC)", {.A = 0xAA, .B = 0x0A, .C = 0xA0}, 2, {.B = 0x0A, .C = 0xA0, .PC = 0x0001}, {}, {}},
    {"LD A, (DE)", {.A = 0xBB, .D = 0x0D, .E = 0xE0}, 2, {.D = 0x0D, .E = 0xE0, .PC = 0x0001}, {}, {}},
    {"LD (BC), A", {.A = 0xAA, .B = 0x0B, .C=0x0C}, 2, {.A = 0xAA, .B = 0x0B, .C=0x0C, .PC = 0x0001}, {0x0B0C}, {0xAA}},
    {"LD (DE), A", {.A = 0xAA, .D = 0x0D, .E=0x0E}, 2, {.A = 0xAA, .D = 0x0D, .E=0x0E, .PC = 0x0001}, {0x0D0E}, {0xAA}},
    {"LD A, (C)", {.A = 0xBB, .C = 0xCC}, 2, {.C = 0xCC, .PC = 0x0001}, {}, {}},
    {"LD (C), A", {.A = 0xAA, .C = 0xCC}, 2, {.A = 0xAA, .C = 0xCC, .PC = 0x0001}, {0xFFCC}, {0xAA}},
    {"LD SP, HL", {.H = 0xFA, .L = 0xCE}, 2, {.H = 0xFA, .L = 0xCE, .SP = 0xFACE, .PC = 0x0001}, {}, {}},
    {"LD DE, 0xFACE", {}, 3, {.D = 0xFA, .E = 0xCE, .PC = 0x0003}, {}, {}},
    {"LD (0xFACE), SP", {.SP = 0xDEAF}, 5, {.SP = 0xDEAF, .PC = 0x0003}, {0xFACE, 0xFACF}, {0xAF, 0xDE}},
    {"LD A, (0xFF34)", {.A = 0xAA}, 4, {.PC = 0x0003}, {}, {}},
    {"LD A, (0x34)", {.A = 0xAA}, 3, {.PC = 0x0002}, {}, {}},
    {"LD (0xAA34), A", {.A = 0xAA}, 4, {.A = 0xAA, .PC = 0x0003}, {0xAA34}, {0xAA}},
    {"LD (0x34), A", {.A = 0xAA}, 3, {.A = 0xAA, .PC = 0x0002}, {0xFF34}, {0xAA}},
    {"LD A, (HLI)", {.A = 0xAA, .H = 0x01, .L = 0xFF}, 2, {.H = 0x02, .L = 0x00, .PC = 0x0001}, {}, {}},
    {"LD A, (HLD)", {.A = 0xAA, .H = 0x02, .L = 0x00}, 2, {.H = 0x01, .L = 0xFF, .PC = 0x0001}, {}, {}},
    {"LD (HLI), A", {.A = 0xAA, .H = 0x01, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x01, .L = 0xDE, .PC = 0x0001}, {0x01DD}, {0xAA}},
    {"LD (HLD), A", {.A = 0xAA, .H = 0x01, .L = 0xDD}, 2, {.A = 0xAA, .H = 0x01, .L = 0xDC, .PC = 0x0001}, {0x01DD}, {0xAA}},

    {"ADC A, E", {.A = 0xE1, .E = 0x0F, .F = 0x10}, 1, {.A = 0xF1, .E = 0x0F, .F = 0x20, .PC = 0x0001}, {}, {}},
    {"ADC A, L", {.A = 0xE1, .L = 0x1E, .F = 0x10}, 1, {.L = 0x1E, .F = 0xB0, .PC = 0x0001}, {}, {}},
    {"ADC A, 0x3B", {.A = 0xE1, .F = 0x10}, 2, {.A = 0x1D,.F = 0x10, .PC = 0x0002}, {}, {}},
    {"ADC A, (HL)", {.A = 0xE1, .F = 0x10}, 2, {.A = 0xE2,.F = 0x00, .PC = 0x0001}, {}, {}},

    {"ADD A, B", {.A = 0x3A, .B = 0xC6}, 1, {.A = 0x00, .B = 0xC6, .F = 0xB0, .PC = 0x0001}, {}, {}},
    {"ADD A, C", {.A = 0xF0, .C = 0x10}, 1, {.A = 0x00, .C = 0x10, .F = 0x90, .PC = 0x0001}, {}, {}},
    {"ADD A, D", {.A = 0x00, .D = 0x01}, 1, {.A = 0x01, .D = 0x01, .PC = 0x0001}, {}, {}},
    {"ADD A, E", {.A = 0x0F, .E = 0x01}, 1, {.A = 0x10, .E = 0x01, .F = 0x20, .PC = 0x0001}, {}, {}},
    {"ADD A, (HL)", {.A = 0x00}, 2, {.A = 0x00, .F = 0x80, .PC = 0x0001}, {}, {}},
    {"ADD A, 0x01", {.A = 0x00}, 2, {.A = 0x01, .PC = 0x0002}, {}, {}},
    {"ADD A, 0xA0", {.A = 0x0F}, 2, {.A = 0xAF, .PC = 0x0002}, {}, {}},
    {"ADD SP, 2", {.SP = 0xFFF8, .F = 0xF0}, 4, {.SP = 0xFFFA, .PC = 0x0002}, {}, {}},
    {"ADD SP, -2", {.SP = 0x1000, .F = 0xF0}, 4, {.F = 0x20, .SP = 0x0FFE, .PC = 0x0002}, {}, {}},

    {"ADD HL, BC", {.B = 0x06, .C = 0x05, .H = 0x8A, .L = 0x23}, 2, {.B = 0x06, .C = 0x05, .H = 0x90, .L = 0x28, .F = 0x20, .PC = 0x0001}, {}, {}},
    {"ADD HL, HL", {.H = 0x8A, .L = 0x23}, 2, {.H = 0x14, .L = 0x46, .F = 0x30, .PC = 0x0001}, {}, {}},

    {"AND L", {.A = 0x5A, .L = 0x3F}, 1, {.A = 0x1A, .L = 0x3F, .F = 0x20, .PC = 0x0001}, {}, {}},
    {"AND 0x38", {.A = 0x5A, .L = 0x3F}, 2, {.A = 0x18, .L = 0x3F, .F = 0x20, .PC = 0x0002}, {}, {}},
    {"AND (HL)", {.A = 0x5A, .L = 0x3F}, 2, {.L = 0x3F,.F = 0xA0, .PC = 0x0001}, {}, {}},

    {"INC A", {.A = 0xFF}, 1, {.F = 0xA0, .PC = 0x0001}, {}, {}},
    {"INC B", {.A = 0xFF}, 1, {.A = 0xFF, .B = 0x01, .PC = 0x0001}, {}, {}},
    {"INC BC", {.B = 0xF0, .C = 0x08}, 2, {.B = 0xF0, .C = 0x09, .PC = 0x0001}, {}, {}},
    {"INC DE", {.D = 0xFF, .E = 0xFF}, 2, {.PC = 0x0001}, {}, {}},
    {"INC (HL)", {.A = 0xFF, .H = 0xFA, .L = 0xCE}, 3, {.A = 0xFF, .H = 0xFA, .L = 0xCE, .PC = 0x0001}, {0xFACE}, {0x01}},

    {"DEC L", {.L = 0x01}, 1, {.F = 0xC0, .PC = 0x0001}, {}, {}},
    {"DEC DE", {.D = 0x23, .E = 0x5F}, 2, {.D = 0x23, .E = 0x5E, .PC = 0x0001}, {}, {}},
    {"DEC (HL)", {.H = 0xFA, .L = 0xCE}, 3, {.H = 0xFA, .L = 0xCE, .F = 0x60, .PC = 0x0001}, {0xFACE}, {0xFF}},

    {"OR A", {.A = 0x5A}, 1, {.A = 0x5A, .PC = 0x0001}, {}, {}},
    {"OR 0x03", {.A = 0x5A}, 2, {.A = 0x5B, .PC = 0x0002}, {}, {}},
    {"OR 0x00", {}, 2, {.F = 0x80, .PC = 0x0002}, {}, {}},
    {"OR (HL)", {.A = 0x5A}, 2, {.A = 0x5A, .PC = 0x0001}, {}, {}},

    {"RLCA", {.A = 0x85}, 1, {.A = 0x0B, .F = 0x10, .PC = 0x0001}, {}, {}},
    {"RLA", {.A = 0x95, .F = 0x10}, 1, {.A = 0x2B, .F = 0x10, .PC = 0x0001}, {}, {}},

    {"RRCA", {.A = 0x3B}, 1, {.A = 0x9D, .F = 0x10, .PC = 0x0001}, {}, {}},
    {"RRA", {.A = 0x81}, 1, {.A = 0x40, .F = 0x10, .PC = 0x0001}, {}, {}},

    {"RLC B", {.B = 0x85}, 2, {.B = 0x0B, .F = 0x10, .PC = 0x0002}, {}, {}},
    {"RLC (HL)", {}, 4, {.F = 0x80, .PC = 0x0002}, {}, {}},

    {"RL L", {.L = 0x80}, 2, { .F = 0x90, .PC = 0x0002}, {}, {}},
    {"RL D", {.D = 0x11}, 2, { .D = 0x22, .PC = 0x0002}, {}, {}},
    {"RL (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"RRC C", {.C = 0x01}, 2, {.C = 0x80, .F = 0x10, .PC = 0x0002}, {}, {}},
    {"RRC (HL)", {.C = 0x01}, 4, {.C = 0x01, .F = 0x80, .PC = 0x0002}, {}, {}},

    {"RR A", {.A = 0x01}, 2, { .F = 0x90, .PC = 0x0002}, {}, {}},
    {"RR L", {.L = 0x8A}, 2, { .L = 0x45, .PC = 0x0002}, {}, {}},
    {"RR (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"SLA D", {.D = 0x80}, 2, { .F = 0x90, .PC = 0x0002}, {}, {}},
    {"SLA E", {.E = 0xFF}, 2, { .E = 0xFE, .F = 0x10, .PC = 0x0002}, {}, {}},
    {"SLA (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"SRA D", {.D = 0x8A}, 2, { .D = 0xC5, .PC = 0x0002}, {}, {}},
    {"SRA H", {.H = 0x01}, 2, { .F = 0x90, .PC = 0x0002}, {}, {}},
    {"SRA (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"SRL A", {.A = 0x01}, 2, { .F = 0x90, .PC = 0x0002}, {}, {}},
    {"SRL C", {.C = 0xFF}, 2, { .C = 0x7F, .F = 0x10, .PC = 0x0002}, {}, {}},
    {"SRL (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"SWAP A", {.A = 0x00}, 2, { .F = 0x80, .PC = 0x0002}, {}, {}},
    {"SWAP H", {.H = 0xF0}, 2, { .H = 0x0F, .PC = 0x0002}, {}, {}},
    {"SWAP (HL)", {}, 4, { .F = 0x80, .PC = 0x0002}, {}, {}},

    {"SUB E", {.A = 0x3E, .E = 0x3E}, 1, {.E = 0x3E, .F = 0xC0, .PC = 0x0001}, {}, {}},
    {"SUB 0x0F", {.A = 0x3E}, 2, {.A = 0x2F, .F = 0x60, .PC = 0x0002}, {}, {}},
    {"SUB 0x40", {.A = 0x3E}, 2, {.A = 0xFE, .F = 0x50, .PC = 0x0002}, {}, {}},

    {"XOR A", {.A = 0xFF}, 1, {.F = 0x80, .PC = 0x0001}, {}, {}},
    {"XOR 0x0F", {.A = 0xFF}, 2, {.A = 0xF0, .PC = 0x0002}, {}, {}},
    {"XOR 0x8A", {.A = 0xFF}, 2, {.A = 0x75, .PC = 0x0002}, {}, {}},
    {"XOR (HL)", {.A = 0xFF}, 2, {.A = 0xFF, .PC = 0x0001}, {}, {}},

    {"CP B", {.A = 0x3C, .B = 0x2F}, 1, {.A = 0x3C, .B = 0x2F, .F = 0x60, .PC = 0x0001}, {}, {}},
    {"CP 0x3C", {.A = 0x3C}, 2, {.A = 0x3C, .F = 0xC0, .PC = 0x0002}, {}, {}},
    {"CP 0x40", {.A = 0x3C}, 2, {.A = 0x3C, .F = 0x50, .PC = 0x0002}, {}, {}},
    {"CP (HL)", {.A = 0x3C}, 2, {.A = 0x3C, .F = 0x40, .PC = 0x0001}, {}, {}},

    {"CPL", {.A = 0x35}, 1, {.A = 0xCA, .PC = 0x0001}, {}, {}},

    {"BIT 7, A", {.A = 0x80}, 2, {.A = 0x80, .F = 0x20, .PC = 0x0002}, {}, {}},
    {"BIT 4, L", {.L = 0xEF}, 2, {.L = 0xEF, .F = 0xA0, .PC = 0x0002}, {}, {}},

    {"BIT 0, (HL)", {}, 3, {.F = 0xA0, .PC = 0x0002}, {}, {}},

    {"SET 2, A", {.A = 0x80}, 2, {.A = 0x84, .PC = 0x0002}, {}, {}},
    {"SET 7, L", {.L = 0x3B}, 2, {.L = 0xBB, .PC = 0x0002}, {}, {}},

    {"SET 7, (HL)", {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, {0xFACE}, {0x80}},

    {"RES 7, A", {.A = 0x80}, 2, {.PC = 0x0002}, {}, {}},
    {"RES 1, L", {.L = 0x3B}, 2, {.L = 0x39, .PC = 0x0002}, {}, {}},

    {"RES 7, (HL)", {.H = 0xFA, .L = 0xCE}, 4, {.H = 0xFA, .L = 0xCE, .PC = 0x0002}, {0xFACE}, {0x00}},

    {"JP 0x8000", {.F = 0xF0}, 4, {.F = 0xF0, .PC = 0x8000}, {}, {}},

    {"JP NZ, 0x8000", {.F = 0x80}, 3, {.F = 0x80, .PC = 0x0003}, {}, {}},
    {"JP Z, 0x8000", {.F = 0x80}, 4, {.F = 0x80, .PC = 0x8000}, {}, {}},
    {"JP C, 0x8000", {.F = 0x80}, 3, {.F = 0x80, .PC = 0x0003}, {}, {}},
    {"JP NC, 0x8000", {.F = 0x80}, 4, {.F = 0x80, .PC = 0x8000}, {}, {}},

    {"JP (HL)", {.H = 0xFA, .L = 0xCE}, 1, {.H = 0xFA, .L = 0xCE, .PC = 0x0000}, {}, {}},

    {"JR -126", {.F = 0xF0, .PC = 0x1000}, 3, {.F = 0xF0, .PC = 0x0F82}, {}, {}},
    {"JR -10", {.F = 0xF0, .PC = 0x1000}, 3, {.F = 0xF0, .PC = 0xFF6}, {}, {}},
    {"JR 10", {.F = 0xF0, .PC = 0x1000}, 3, {.F = 0xF0, .PC = 0x100A}, {}, {}},
    {"JR 129", {.F = 0xF0, .PC = 0x1000}, 3, {.F = 0xF0, .PC = 0x1081}, {}, {}},

    {
      "CALL 0x1234", {.PC = 0x8000, .SP = 0xFFFE}, 6, {.PC = 0x1234, .SP = 0xFFFC},
      {0xFFFC, 0xFFFD}, {0x03, 0x80}
    },

    {
      "CALL NZ, 0x1234", {.F = 0x80, .PC = 0x7FFD, .SP = 0xFFFE}, 3, {.F = 0x80, .PC = 0x8000, .SP = 0xFFFE},{}, {}
    },

    {
      "CALL Z, 0x1234", {.F = 0x80, .PC = 0x8000, .SP = 0xFFFE}, 6, {.F = 0x80, .PC = 0x1234, .SP = 0xFFFC},
      {0xFFFC, 0xFFFD}, {0x03, 0x80}
    },

    {"RET", {.PC = 0x8000, .SP = 0xFFFC}, 4, { .PC = 0x0000, .SP = 0xFFFE}, {}, {} },
    {"RET NZ", {.F = 0x80, .PC = 0x9000, .SP = 0xFFFC}, 2, {.F = 0x80, .PC = 0x9001, .SP = 0xFFFC}, {}, {} },
    {"RET Z", {.F = 0x80, .PC = 0x9000, .SP = 0xFFFC}, 5, {.F = 0x80, .PC = 0x0000, .SP = 0xFFFE}, {}, {} },

  };


  for (int t = 0; (err_cnt < MAX_ERRORS) &&  t < sizeof(tests) / sizeof(struct test); t++) {
    struct inst inst = {0};
    struct test tst = tests[t];
    memset(ram, 0, sizeof(ram));

    tst.initial.ram = ram;

    init_inst_from_asm(&inst, tst.asm_command);
    int cycles = cpu_exec_instruction(&tst.initial, &inst);
    int initial_error_cnt = err_cnt;
    if (cycles != tst.cycles) {
      sprintf(err_msgs[err_cnt++],"\tfound cycles:\t%d\n\t\texpected:\t%d", cycles, tst.cycles);
    }
    if (!cpu_equals(tst.initial, tst.expected)) {
      cpu_to_str(buf1, &tst.initial);
      cpu_to_str(buf2, &tst.expected);
      sprintf(err_msgs[err_cnt++],
	      "\tfound cpu:\t%s\n\t\texpected type:\t%s",
	      buf1, buf2);
    }
    if ((tst.modified_addrs[0] || tst.modified_addrs[1]) &&
	((tst.initial.ram[tst.modified_addrs[0]] != tst.modified_addr_values[0]) ||
	 (tst.initial.ram[tst.modified_addrs[1]] != tst.modified_addr_values[1]))
	) {
      sprintf(err_msgs[err_cnt++],
	      "\tfound ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}\n\t\texpected ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}",
	      tst.modified_addrs[0], tst.initial.ram[tst.modified_addrs[0]], tst.modified_addrs[1], tst.initial.ram[tst.modified_addrs[1]],
	      tst.modified_addrs[0], tst.modified_addr_values[0], tst.modified_addrs[1], tst.modified_addr_values[1]);
    }
    if (initial_error_cnt<err_cnt) {
      printf("instruction failed: '%s'\n", tst.asm_command);
      for (int e = initial_error_cnt; e < err_cnt; e++)
	printf("\t%s\n", err_msgs[e]);
    }
  }
  return err_cnt > 0;
}

int main() {
  return test_cpu_exec();
}
