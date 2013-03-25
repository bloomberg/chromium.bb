#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a google-test shard.

This makes a simple interface to run a shard on the command line independent of
the interpreter, e.g. cmd.exe vs bash.
"""

import optparse
import os
import subprocess
import sys

import run_isolated


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def main():
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(usage='%prog <options> [gtest]')
  parser.disable_interspersed_args()
  parser.add_option(
      '-I', '--index',
      type='int',
      default=os.environ.get('GTEST_SHARD_INDEX'),
      help='Shard index to run')
  parser.add_option(
      '-S', '--shards',
      type='int',
      default=os.environ.get('GTEST_TOTAL_SHARDS'),
      help='Total number of shards to calculate from the --index to run')
  options, args = parser.parse_args()
  env = os.environ.copy()
  env['GTEST_TOTAL_SHARDS'] = str(options.shards)
  env['GTEST_SHARD_INDEX'] = str(options.index)
  return subprocess.call(fix_python_path(args), env=env)


if __name__ == '__main__':
  sys.exit(main())
