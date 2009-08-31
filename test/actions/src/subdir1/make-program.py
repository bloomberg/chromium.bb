#!/usr/bin/env python
import sys

contents = r"""
#include <stdio.h>

int main(int argc, char *argv[])
{
  printf("Hello from make-program.py\n");
  return 0;
}
"""

open(sys.argv[1], 'w').write(contents)

sys.exit(0)
