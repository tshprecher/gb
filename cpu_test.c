#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "inst.h"


// TODO: consoloidate with inst tests, remove duplication
#define TERM_COLOR_RED "\x1B[31m"
#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_RESET "\033[0m"

#define MAX_ERRORS 10

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
  uint8_t ram[0x2000];

  struct test {
    char *asm_command;

    // expected state after instruction exec
    struct cpu expected;
    uint16_t modified_addrs[2];
    uint8_t modified_addr_values[2];
  } tests[] = {
    {
      .asm_command = "CALL 0x1000", {.PC = 0x1000, .SP = 0x1FFD},
      {0x1FFD, 0x1FFE}, {0xDD, 0x1D}
    },
  };

  for (int t = 0; (err_cnt < MAX_ERRORS) &&  t < sizeof(tests) / sizeof(struct test); t++) {
    struct cpu cpu = {0};
    struct inst inst = {0};
    struct test tst = tests[t];
    memset(ram, 0, sizeof(ram));

    cpu.SP = 0x1FFF;
    cpu.PC = 0x1DDD;
    cpu.ram = ram;

    init_inst_from_asm(&inst, tst.asm_command);
    cpu_exec_instruction(&cpu, &inst);
    if (!cpu_equals(cpu, tst.expected)) {
      cpu_to_str(buf1, &cpu);
      cpu_to_str(buf2, &tst.expected);
      sprintf(err_msgs[err_cnt++],
	      "\tfound cpu:\t%s\n\t\texpected type:\t%s",
	      buf1, buf2);
    }
    if ((tst.modified_addrs[0] || tst.modified_addrs[1]) &&
	((cpu.ram[tst.modified_addrs[0]] != tst.modified_addr_values[0]) ||
	 (cpu.ram[tst.modified_addrs[1]] != tst.modified_addr_values[1]))
	) {
      sprintf(err_msgs[err_cnt++],
	      "\tfound ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}\n\t\texpected ram values:\t{0x%04X : 0x%02X, 0x%04X : 0x%02X}",
	      tst.modified_addrs[0], cpu.ram[tst.modified_addrs[0]], tst.modified_addrs[1], cpu.ram[tst.modified_addrs[1]],
	      tst.modified_addrs[0], tst.modified_addr_values[0], tst.modified_addrs[1], tst.modified_addr_values[1]);
    }
  }

  for (int e = 0; e < err_cnt; e++)
    printf("\t%s\n", err_msgs[e]);

  return err_cnt > 0;
}

int main() {
  // TODO: copy cpu before, cpu after, optionally diff memory
  return test_cpu_exec();
}
