#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import contextlib
import os
import subprocess
import sys


_SCRIPT_PATH = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        'asan_device_setup.sh'))


@contextlib.contextmanager
def Asan(args):
  env = os.environ.copy()
  env['ADB'] = args.adb

  try:
    setup_cmd = [_SCRIPT_PATH, '--lib', args.lib]
    if args.device:
      setup_cmd += ['--device', args.device]
    subprocess.check_call(setup_cmd, env=env)
    yield
  finally:
    teardown_cmd = [_SCRIPT_PATH, '--revert']
    if args.device:
      teardown_cmd += ['--device', args.device]
    subprocess.check_call(teardown_cmd, env=env)


def main(raw_args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--adb', type=os.path.realpath, required=True,
      help='Path to adb binary.')
  parser.add_argument(
      '--device',
      help='Device serial.')
  parser.add_argument(
      '--lib', type=os.path.realpath, required=True,
      help='Path to asan library.')
  parser.add_argument(
      'command', nargs='*',
      help='Command to run with ASAN installed.')
  args = parser.parse_args()

  with Asan(args):
    if args.command:
      return subprocess.call(args.command)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
