#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verify basic usage of sharding_supervisor."""

import os
import subprocess
import sys
import unittest

import sharding_supervisor

SHARDING_SUPERVISOR = os.path.join(os.path.dirname(sys.argv[0]),
                                   'sharding_supervisor.py')
DUMMY_TEST = os.path.join(os.path.dirname(sys.argv[0]), 'dummy_test.py')
NUM_CORES = sharding_supervisor.DetectNumCores()
SHARDS_PER_CORE = sharding_supervisor.SS_DEFAULT_SHARDS_PER_CORE


def generate_expected_output(start, end, num_shards):
  """Generate the expected stdout and stderr for the dummy test."""
  stdout = ''
  stderr = ''
  for i in range(start, end):
    stdout += 'Running shard %d of %d\n' % (i, num_shards)
  stdout += '\nALL SHARDS PASSED!\nALL TESTS PASSED!\n'

  return (stdout, stderr)


class ShardingSupervisorUnittest(unittest.TestCase):
  def test_basic_run(self):
    # Default test.
    expected_shards = NUM_CORES * SHARDS_PER_CORE
    (expected_out, expected_err) = \
        generate_expected_output(0, expected_shards, expected_shards)
    p = subprocess.Popen([SHARDING_SUPERVISOR, '--no-color', DUMMY_TEST],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    (out, err) = p.communicate()
    self.assertEqual(expected_out, out)
    self.assertEqual(expected_err, err)
    self.assertEqual(0, p.returncode)

  def test_shard_per_core(self):
    """Test the --shards_per_core parameter."""
    expected_shards = NUM_CORES * 25
    (expected_out, expected_err) = \
        generate_expected_output(0, expected_shards, expected_shards)
    p = subprocess.Popen([SHARDING_SUPERVISOR, '--no-color',
                          '--shards_per_core', '25', DUMMY_TEST],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    (out, err) = p.communicate()
    self.assertEqual(expected_out, out)
    self.assertEqual(expected_err, err)
    self.assertEqual(0, p.returncode)

  def test_slave_sharding(self):
    """Test the --total-slaves and --slave-index parameters."""
    total_shards = 6
    expected_shards = NUM_CORES * SHARDS_PER_CORE * total_shards

    # Test every single index to make sure they run correctly.
    for index in range(total_shards):
      begin = NUM_CORES * SHARDS_PER_CORE * index
      end = begin + NUM_CORES * SHARDS_PER_CORE
      (expected_out, expected_err) = \
          generate_expected_output(begin, end, expected_shards)
      p = subprocess.Popen([SHARDING_SUPERVISOR, '--no-color',
                            '--total-slaves', str(total_shards),
                            '--slave-index', str(index),
                            DUMMY_TEST],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)

      (out, err) = p.communicate()
      self.assertEqual(expected_out, out)
      self.assertEqual(expected_err, err)
      self.assertEqual(0, p.returncode)


if __name__ == '__main__':
  unittest.main()
