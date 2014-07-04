#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "liblouis.h"
#include "brl_checks.h"

typedef struct test {
  char *input;
  char *expected;
} test_s;

test_s tests[] = {
  {"M5", "Men"},
  {"M>k", "Mark"},
  {"K5", "Ken"}
};

int main(int argc, char **argv) {
  int result = 0;
  int number_of_tests = sizeof(tests) / sizeof(tests[0]);
  const char *tbl = "en-us-g2.ctb";

  for (int i = 0; i<2; i++)
    result |= check_backtranslation(tbl, tests[i].input, NULL, tests[i].expected);

  return result;
}
