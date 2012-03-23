#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "brl_checks.h"

int
main(int argc, char **argv)
{

  int result = 0;

  /* First check a plain word to see if the table works */
  result = check_translation("pass2.ctb", "Rene", NULL, "rene");

  /* then try a word which uses pass2 */
  result |= check_translation("pass2.ctb", "Reno", NULL, "ren'o");

  return result;
}
