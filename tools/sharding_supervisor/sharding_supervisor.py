#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defer to --brave-new-test-launcher."""

import os
import optparse
import subprocess
import sys


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
  group = optparse.OptionGroup(
      parser, 'Options of run_test_cases.py passed through')
  group.add_option(
      '--retries', type='int', help='Kept as --retries')
  group.add_option(
      '-j', '--jobs', type='int', help='Number of parallel jobs')
  group.add_option(
      '--clusters', type='int', help='Maximum number of tests in a batch')
  group.add_option(
      '--verbose', action='count', default=0, help='Kept as --verbose')
  parser.add_option_group(group)

  parser.disable_interspersed_args()
  options, args = parser.parse_args()

  env = os.environ
  env['GTEST_TOTAL_SHARDS'] = str(options.total_slaves)
  env['GTEST_SHARD_INDEX'] = str(options.slave_index)

  if options.jobs:
    args.append('--test-launcher-jobs=%d' % options.jobs)

  if options.clusters:
    args.append('--test-launcher-batch-limit=%d' % options.clusters)

  return subprocess.Popen(args + ['--brave-new-test-launcher'], env=env).wait()


if __name__ == '__main__':
  sys.exit(main())
