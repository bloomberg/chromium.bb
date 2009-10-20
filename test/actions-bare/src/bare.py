#!/usr/bin/python

import sys

f = open(sys.argv[1], 'wb')
f.write('Hello from bare.py\n')
f.close()
