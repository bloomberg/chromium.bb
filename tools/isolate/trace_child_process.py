#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Executes with the command to be run and optionally waits for the go signal.

Not meant to be used directly, only meant to be used by trace_inputs.py.

This script is used by the tracer as a signal for the log parser that the child
process is the process we care about. It is especially important on kernel based
tracer because we want it to trace the relevant process tree.
"""

import subprocess
import sys


def main():
  cmd = sys.argv[2:]

  # The reason os.execve() is not used is that we don't want the modules
  # imported here to influence the executable being traced, so we need a fresh
  # pid and need to fork.
  # TODO(maruel): Use CreateProcess() on Windows and fork manually on OSX.
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
