#!/usr/bin/env python

import sys

contents = 'Hello from make-subdir-file.py\n'

open(sys.argv[1], 'wb').write(contents)
