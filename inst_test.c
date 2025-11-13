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
      for (int c = 0; c < err_cnt; c++)
	printf("    %s\n", err_msgs[c]);
      err_cnt = 0;
    } else
      print_green("[ok]\n");
    fclose(f);
  }
  closedir(dir);
  return global_failed_cnt != 0;
}


int main () {
  int result = test_inst_parsing();
  err_cnt = 0;

  if (result)
    print_red("FAILED\n");
  else
    print_green("SUCCESS!\n");
}
