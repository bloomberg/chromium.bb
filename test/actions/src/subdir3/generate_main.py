#!/usr/bin/env python

import sys

contents = """
#include <stdio.h>

int main(int argc, char *argv[])
{
  printf("Hello from generate_main.py\\n");
  return 0;
}
"""

open(sys.argv[1], 'w').write(contents)

sys.exit(0)
