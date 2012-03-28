#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "brl_checks.h"

int
main(int argc, char **argv)
{

  int result = 0;

  result = check_translation("letterDefTest.ctb", "ms", NULL, "\x280e\x280d");
  return result;
}
