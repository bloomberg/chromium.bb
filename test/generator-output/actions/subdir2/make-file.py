#!/usr/bin/env python

import sys

contents = "Hello from make-file.py\n"

open(sys.argv[1], 'wb').write(contents)
