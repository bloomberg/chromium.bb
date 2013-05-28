#include <stdio.h>
#include "brl_checks.h"
#include "liblouis.h"

int
main (int argc, char **argv)
{
  int result = 0;
  char *table = "empty.ctb";
  char rule[18];

  lou_compileString(table, "include latinLetterDef6Dots.uti");

  for (char c1 = 'a'; c1 <= 'z'; c1++) {
    for (char c2 = 'a'; c2 <= 'z'; c2++) {
      for (char c3 = 'a'; c3 <= 'z'; c3++) {
  	for (char c4 = 'a'; c4 <= 'z'; c4++) {
  	  for (char c5 = 'a'; c5 <= 'z'; c5++) {
  	    sprintf(rule, "always aa%c%c%c%c%c 1", c1, c2, c3, c4, c5);
  	    printf("Rule: %s\n", rule);
  	    lou_compileString(table, rule);
  	  }
  	}
      }
    }
  }

  for (int i = 0; i < 1000; i++) {
    result |= check_translation(table, "aaaaaaa", NULL, "1");
    result |= check_translation(table, "aazzzzz", NULL, "1");
  }

  return result;
}
