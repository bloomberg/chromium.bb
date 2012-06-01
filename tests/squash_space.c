#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "brl_checks.h"

int main(int argc, char **argv)
{

  int result = 0;
  const char *tests_using_repeated[] = {
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

  int tests_len = sizeof(tests_using_repeated)/sizeof(char*);

  for (int i = 0; i < tests_len; i += 2)
    result |= check_translation("squash_space_with_repeated.utb", tests_using_repeated[i], NULL, tests_using_repeated[i+1]);

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

  tests_len = sizeof(tests)/sizeof(char*);

  for (int i = 0; i < tests_len; i += 2)
    result |= check_translation("squash_space_with_correct.utb", tests[i], NULL, tests[i+1]);

  for (int i = 0; i < tests_len; i += 2)
    result |= check_translation("squash_space_with_context_1.utb", tests[i], NULL, tests[i+1]);

  for (int i = 0; i < tests_len; i += 2)
    result |= check_translation("squash_space_with_context_2.utb", tests[i], NULL, tests[i+1]);


  return result;
}
