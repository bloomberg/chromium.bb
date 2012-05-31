#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "brl_checks.h"

int main(int argc, char **argv)
{

  int result = 0;
  const char *tests[] = {
    /* first column is the text, the second is the expected value */
    " ", " ",
    /* the usual case */
    "                                                                                ", " ",
    /* a very long string */
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                ", " ",
    /* an even longer string */
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                ", " ",
    /* a string containing tabs */
    "    \t",     " \t",
    "    \t    ", " \t ",
    "\t    ",     "\t ",
    /* Strings containing newlines */
    "    \n",     " \n",
    "    \n    ", " \n ",
    "\n    ",     "\n "
  };

  const int tests_len = sizeof(tests)/sizeof(char*);

  for (int i = 0; i < tests_len; i += 2)
    result |= check_translation("repeated.utb", tests[i], NULL, tests[i+1]);

  const char *tests2[] = {
    /* first column is the text, the second is the expected value */
    " ", " ",
    /* the usual case */
    "                                                                                ", " ",
    /* a very long string */
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                ", " ",
    /* an even longer string */
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                " \
    "                                                                                ", " ",
    /* a string containing tabs */
    "    \t",     " ",
    "    \t    ", " ",
    "\t    ",     " ",
    /* Strings containing newlines */
    "    \n",     " ",
    "    \n    ", " ",
    "\n    ",     " ",
    /* All mixed */
    "    \n    \t    \n    \t\t    \n\n\n\t\t    ",     " ",
  };

  const int tests2_len = sizeof(tests2)/sizeof(char*);

  for (int i = 0; i < tests2_len; i += 2) {
    result |= check_translation("repeated_with_correct.utb", tests2[i], NULL, tests2[i+1]);
  }

  return result;
}
