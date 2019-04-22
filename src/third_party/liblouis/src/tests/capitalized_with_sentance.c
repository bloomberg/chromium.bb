/* liblouis Braille Translation and Back-Translation Library

Copyright (C) 2008 James Teh <jamie@jantrid.net>

Copying and distribution of this file, with or without modification,
are permitted in any medium without royalty provided the copyright
notice and this notice are preserved. This file is offered as-is,
without any warranty. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "liblouis.h"
#include "brl_checks.h"
#include "default_table.h"

int
main (int argc, char **argv)
{
  const char *str1 = "This is a test ";
  const int expected_pos1[]={0,1,2,3,2,3,4,5,6,7,8,9,10,11,11};

  int result = check_cursor_pos(TRANSLATION_TABLE, str1, expected_pos1);

  lou_free();

  return result;
}
