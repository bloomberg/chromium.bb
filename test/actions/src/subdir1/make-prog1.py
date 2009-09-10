#!/usr/bin/env python
import sys

contents = r"""
#include <stdio.h>

void prog1(void)
{
  printf("Hello from make-prog1.py\n");
}
"""

open(sys.argv[1], 'w').write(contents)

sys.exit(0)
