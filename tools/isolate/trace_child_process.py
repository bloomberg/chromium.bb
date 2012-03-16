#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Waits for the go signal and replaces itself with the command to be run.

Not meant to be used directly, only meant to be used by trace_inputs.py.
"""

import os
import subprocess
import sys


def main():
  signal = 'Go!'
  value = sys.stdin.read(len(signal))
  assert value == signal
  sys.stdin.close()
  # Replace the executable with an absolute path to make it easier to grok.
  cmd = sys.argv[1:]
  cmd[0] = os.path.abspath(cmd[0])
  if cmd[0].endswith('.py'):
    cmd.insert(0, sys.executable)
  p = subprocess.Popen(cmd)
  #print 'Child pid: %d' % p.pid
  p.wait()
  return p.returncode


if __name__ == '__main__':
  sys.exit(main())
