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
    {
      "CALL 0x1000", {.PC = 0x1DDD, .SP = 0x1FFF}, 6, {.PC = 0x1000, .SP = 0x1FFD},
      {0x1FFD, 0x1FFE}, {0xDD, 0x1D}
    },
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

    // through section 2.1
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
