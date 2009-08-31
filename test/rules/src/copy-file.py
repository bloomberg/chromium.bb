#!/usr/bin/env python
import sys

contents = open(sys.argv[1], 'r').read()
open(sys.argv[2], 'wb').write(contents)

sys.exit(0)
