#!/usr/bin/env python
import sys

contents = r"""
#include <stdio.h>

void prog2(void)
{
  printf("Hello from make-prog2.py\n");
}
"""

open(sys.argv[1], 'w').write(contents)

sys.exit(0)
