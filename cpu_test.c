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

/*
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
}*/

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
  char buf[128];
  uint8_t ram[0x2000];

  struct cpu cpu = {0};
  struct inst inst = {0};

  memset(&cpu, 0, sizeof(struct cpu));
  cpu.SP = 0x1FFF;
  cpu.PC = 0x1DDD;
  cpu.ram = ram;
  memset(&inst, 0, sizeof(struct inst));
  init_inst_from_asm(&inst, "CALL 0x1000");

  cpu_to_str(buf, &cpu);
  printf("DEBUG 2: before %s\n", buf);
  cpu_exec_instruction(&cpu, &inst);
  cpu_to_str(buf, &cpu);
  printf("DEBUG: after %s\n", buf);
  //  cpu_equals(cpu_before, cpu_after);
  //  printf("DEBUG: result expected: %d\n", ));

  return 1;
}


int main() {
  // TODO: copy cpu before, cpu after, optionally diff memory
  return test_cpu_exec();
}
