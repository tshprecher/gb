#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "inst.h"

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


// returns 1 on failure
int test_inst_parsing() {
  DIR *dir;
  struct dirent *entry;
  char *dirname = "testdata/inst";
  dir = opendir(dirname);

  printf("Test instruction parsing...\n");
  if (dir == NULL) {
    perror("error opening test directory 'testdata/inst'\n");
    return 1;
  }

  int global_failed_cnt = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;

    FILE *f;
    char filename[32];
    char line[32];

    sprintf(filename, "testdata/inst/%s", entry->d_name);
    f = fopen(filename, "r");
    if (f == NULL) {
      fprintf(stderr, "error opening file '%s'\n", filename);
      return 1;
    }

    printf("\ttesting file %s\t\t... ", entry->d_name);
    int file_failed_cnt = 0;
    while (fgets(line, 32, f) != NULL) {
      struct inst inst;
      int len;
      line[31] = '\0'; // ensure no buffer overruns
      len = strlen(line);
      if (len == 1)
	continue;
      line[len-1]='\0'; // remove traling newline
      if (!init_inst_from_asm(&inst, line)) {
	file_failed_cnt++;
	global_failed_cnt++;
	sprintf(err_msgs[err_cnt++], "could not parse '%s'", line);
	if (err_cnt >= MAX_ERRORS)
	  break;
      }
    }
    if (file_failed_cnt) {
      print_red("[error]\n");
      for (int e = 0; e < err_cnt; e++)
	printf("    %s\n", err_msgs[e]);
      err_cnt = 0;
    } else
      print_green("[ok]\n");
    fclose(f);
  }
  closedir(dir);
  return global_failed_cnt != 0;
}

// returns 1 on failure.
int test_inst_init() {
  printf("Test instruction initialization...\n");

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
    { .asm_command = "POP HL", .type = POP, {{.type = QQ, .value.byte = 2}} , 1},
    { .asm_command = "RST 4", .type = RST, {{.type = T, .value.byte = 4}} , 1},
  };


  for (int t = 0; (err_cnt < MAX_ERRORS) && (t < sizeof(tests) / sizeof(struct test)); t++) {
    struct inst inst = {0};
    struct test tst = tests[t];
    int test_failed = 0;

    printf("\ttesting '%s'\t\t... ", tst.asm_command);

    if (!init_inst_from_asm(&inst, tst.asm_command)) {
      // somewhat ugly, perhaps could use a helper but it works for now
      if (err_cnt < MAX_ERRORS) {
	test_failed++;
	sprintf(err_msgs[err_cnt++],
		"\tcould not initialize instruction");
      }
    } else {
      if (err_cnt < MAX_ERRORS && tst.type != inst.type) {
	test_failed++;
	sprintf(err_msgs[err_cnt++],
		"\tfound type:\t%d\n\t\texpected type:\t%d",
		inst.type, tst.type);
      }
      if (err_cnt < MAX_ERRORS && tst.args_count != inst.args_count) {
	test_failed++;
	sprintf(err_msgs[err_cnt++],
		"\tfound args count:\t%d\n\t\texpected args count:\t%d",
		inst.args_count, tst.args_count);
      } else {
	for (int a = 0; a < tst.args_count; a++) {
	  if (err_cnt < MAX_ERRORS &&
	      (
	       tst.args[a].type != inst.args[a].type ||
	       tst.args[a].value.byte != inst.args[a].value.byte ||
	       tst.args[a].value.word[0] != inst.args[a].value.word[0] ||
	       tst.args[a].value.word[1] != inst.args[a].value.word[1]
	       )
	      )
	    {
	      test_failed++;
	      sprintf(err_msgs[err_cnt++],
		      "\tfound:\t\t{type: %u, value: { byte: %u, word: [%u, %u]}}\n\t\texpected:\t{type: %u, value: { byte: %u, word: [%u, %u]}}",
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
    if (test_failed) {
      print_red("[error]\n");
      for (int e = 0; e < err_cnt; e++)
	printf("\t%s\n", err_msgs[e]);
    } else
      print_green("[ok]\n");
  }

  return err_cnt > 0;
}


int main () {
  int result = test_inst_parsing();
  printf("\n");
  err_cnt = 0;

  result += test_inst_init();
  printf("\n");
  err_cnt = 0;

  if (result)
    print_red("FAILED\n");
  else
    print_green("SUCCESS!\n");

  return result;
}
