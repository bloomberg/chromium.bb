#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Outputs on both stderr and stdout."""

import re
import sys
import time


def main():
  for command in sys.argv[1:]:
    if re.match(r'^[0-9\.]+$', command):
      time.sleep(float(command))
      continue

    if command.startswith('out_'):
      pipe = sys.stdout
    elif command.startswith('err_'):
      pipe = sys.stderr
    else:
      return 1

    command = command[4:]
    if command == 'print':
      pipe.write('printing')
    elif command == 'sleeping':
      pipe.write('Sleeping.\n')
    elif command == 'slept':
      pipe.write('Slept.\n')
    elif command == 'lf':
      pipe.write('\n')
    elif command == 'flush':
      pipe.flush()
    else:
      return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
