#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defer to run_test_cases.py."""

import os
import optparse
import sys

ROOT_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def pop_gtest_output(args):
  """Extracts --gtest_output from the args if present."""
  for index, arg in enumerate(args):
    if arg.startswith('--gtest_output='):
      return args.pop(index)


def main():
  parser = optparse.OptionParser()

  group = optparse.OptionGroup(
      parser, 'Compability flag with the old sharding_supervisor')
  group.add_option(
      '--no-color', action='store_true', help='Ignored')
  group.add_option(
      '--retry-failed', action='store_true', help='Ignored')
  group.add_option(
      '-t', '--timeout', type='int', help='Kept as --timeout')
  group.add_option(
      '--total-slaves', type='int', default=1, help='Converted to --index')
  group.add_option(
      '--slave-index', type='int', default=0, help='Converted to --shards')
  parser.add_option_group(group)

  parser.disable_interspersed_args()
  options, args = parser.parse_args()

  swarm_client_dir = os.path.join(ROOT_DIR, 'tools', 'swarm_client')
  sys.path.insert(0, swarm_client_dir)

  cmd = [
    '--shards', str(options.total_slaves),
    '--index', str(options.slave_index),
    '--no-dump',
    '--no-cr',
  ]
  if options.timeout is not None:
    cmd.extend(['--timeout', str(options.timeout)])
  gtest_output = pop_gtest_output(args)
  if gtest_output:
    # It is important that --gtest_output appears before the '--' so it is
    # properly processed by run_test_cases.
    cmd.append(gtest_output)

  import run_test_cases  # pylint: disable=F0401

  return run_test_cases.main(cmd + ['--'] + args)


if __name__ == '__main__':
  sys.exit(main())
