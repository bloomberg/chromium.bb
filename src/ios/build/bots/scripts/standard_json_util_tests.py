# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for standard_json_util.py."""

import collections
import os
import unittest

import standard_json_util as sju


class UnitTest(unittest.TestCase):

  def test_base_cases(self):
    """Test invalid test names are skipped"""
    passed = ['', None]
    failed = ['', None]

    output = sju.StdJson(passed=passed, failed=failed)
    self.assertFalse(output.tests)

  def test_single_constructor(self):
    """Test one test passing, failing, flaking via constructor"""
    tests = ['a']

    output = sju.StdJson(passed=tests)
    self.assertEqual(output.tests['a']['actual'], 'PASS')

    output = sju.StdJson(failed=tests)
    self.assertEqual(output.tests['a']['actual'], 'FAIL')

    output = sju.StdJson(flaked=tests)
    self.assertEqual(output.tests['a']['actual'], 'PASS')
    self.assertTrue(output.tests['a']['is_flaky'])

  def test_multi_run(self):
    """Test multiple executions of the same test"""
    test = 'a'

    output = sju.StdJson(passed=[test])
    self.assertEqual(output.tests['a']['actual'], 'PASS')

    output.mark_failed(test)
    self.assertEqual(output.tests['a']['actual'], 'PASS FAIL')

    output.mark_passed(test, flaky=True)
    self.assertEqual(output.tests['a']['actual'], 'PASS FAIL PASS')
    self.assertTrue(output.tests['a']['is_flaky'])
    self.assertIsNot(output.tests['a'].get('is_unexpected'), True)

  def test_multi_scenario(self):
    """Test a scenario where some tests pass, fail and flake"""
    passed = ['a', 'b', 'c']
    failed = ['d']
    flaked = ['e']

    output = sju.StdJson(passed=passed, failed=failed, flaked=flaked)
    self.assertEqual(len(output.tests), 5)
    # Ensure that the flaked is set as passed, with is_flaky=True
    self.assertTrue(output.tests['e']['is_flaky'])

    # A retry that re-runs failed fails again
    output.mark_failed('d')
    self.assertEqual(output.tests['d']['actual'], 'FAIL FAIL')
    self.assertTrue(output.tests['d']['is_unexpected'], True)

    # Another retry of 'd' passes, so we set it as a flaky pass
    output.mark_passed('d', flaky=True)
    self.assertEqual(output.tests['d']['actual'], 'FAIL FAIL PASS')
    self.assertTrue(output.tests['d']['is_flaky'])
    self.assertIsNot(output.tests['d'].get('is_unexpected'), True)

  def test_flaky_without_explicit(self):
    """Test setting pass on an already failed test, w/o explicit flaky"""
    test = 'e'
    output = sju.StdJson()
    output.mark_failed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL')

    output.mark_passed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL PASS')
    self.assertTrue(output.tests['e']['is_flaky'])
    self.assertIsNot(output.tests['e'].get('is_unexpected'), True)

  def test_skip(self):
    """Test setting expected skip."""
    test = 'f'
    output = sju.StdJson()
    output.mark_skipped(test)
    self.assertEqual(output.tests['f']['actual'], 'SKIP')
    self.assertFalse(output.tests['f'].get('is_unexpected', False))

  def test_timeout(self):
    """Test setting timeout"""
    test = 'e'
    output = sju.StdJson()
    output.mark_timeout(test)
    self.assertEqual(output.tests['e']['actual'], 'TIMEOUT')
    self.assertTrue(output.tests['e']['is_unexpected'], True)

    output = sju.StdJson()
    output.mark_failed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL')
    output.mark_timeout(test)
    self.assertTrue(output.tests['e']['actual'], 'FAIL TIMEOUT')
    self.assertTrue(output.tests['e']['is_unexpected'], True)

  def test_shard(self):
    """Test shard into is written to test result."""
    test = 'f'
    output = sju.StdJson()
    output.mark_passed(test)
    self.assertEqual(output.tests['f']['shard'],
                     os.getenv('GTEST_SHARD_INDEX', 0))


if __name__ == '__main__':
  unittest.main()
