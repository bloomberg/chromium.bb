#!/usr/bin/python

import sys

f = open(sys.argv[1], 'wb')
f.write('Hello from emit.py\n')
f.close()
