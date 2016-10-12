#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "liblouis.h"
#include "default_table.h"
#include "brl_checks.h"

int main(int argc, char **argv) {
  int result = 0;
  const char *input = "but b can ";
  const char *expected = "b ;b c ";

  result |= check_translation(TRANSLATION_TABLE, input, NULL, expected);
  result |= check_backtranslation(TRANSLATION_TABLE, expected, NULL, input);

  lou_free();
  return result;
}
