#include <stdio.h>
#include <stdarg.h>

#define TERM_COLOR_RED "\x1B[31m"
#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_RESET "\033[0m"

#define MAX_TEST_ERRORS 10

struct test_suite {
  // suite variables
  int suite_error_count;

  // test variables
  int test_error_count;
  char test_errors[MAX_TEST_ERRORS][256];
};

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

void suite_start(struct test_suite *ts, char *name) {
  ts->suite_error_count = 0;
  printf("Testing %s...\n", name);
}

void suite_test_start(struct test_suite *ts, char *name) {
  ts->test_error_count = 0;
  printf("\ttesting '%s'...", name);
}

void suite_test_end(struct test_suite *ts) {
  if (ts->test_error_count) {
    print_red("\t\t[error]\n");
    for (int e = 0; e < ts->test_error_count; e++)
      printf("%s\n", ts->test_errors[e]);
  } else {
    print_green("\t\t[ok]\n");
  }
}

// return true on success
int suite_result(struct test_suite *ts) {
  printf("\n");
  return ts->suite_error_count == 0;
}

void suite_test_error(struct test_suite *ts, char *fmt, ...) {
  if (ts->test_error_count <= MAX_TEST_ERRORS) {
    va_list args;
    va_start(args, fmt);
    vsprintf(ts->test_errors[ts->test_error_count], fmt, args);
    va_end(args);

    ts->test_error_count++;
    ts->suite_error_count++;
  }
}
