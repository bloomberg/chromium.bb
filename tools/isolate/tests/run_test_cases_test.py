#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_test_cases

SLEEP = os.path.join(ROOT_DIR, 'tests', 'run_test_cases', 'sleep.py')


def to_native_eol(string):
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


class ListTestCasesTest(unittest.TestCase):
  def test_shards(self):
    test_cases = (
      (range(10), 10, 0, 1),

      ([0, 1], 5, 0, 3),
      ([2, 3], 5, 1, 3),
      ([4   ], 5, 2, 3),

      ([0], 5, 0, 7),
      ([1], 5, 1, 7),
      ([2], 5, 2, 7),
      ([3], 5, 3, 7),
      ([4], 5, 4, 7),
      ([ ], 5, 5, 7),
      ([ ], 5, 6, 7),

      ([0, 1], 4, 0, 2),
      ([2, 3], 4, 1, 2),
    )
    for expected, range_length, index, shards in test_cases:
      result = run_test_cases.filter_shards(range(range_length), index, shards)
      self.assertEquals(
          expected, result, (result, expected, range_length, index, shards))


class RunTestCases(unittest.TestCase):
  def test_call(self):
    cmd = [sys.executable, SLEEP, '0.001']
    # 0 means no timeout, like None.
    output, code = run_test_cases.call_with_timeout(cmd, 0)
    self.assertEquals(to_native_eol('Sleeping.\nSlept.\n'), output)
    self.assertEquals(0, code)

  def test_call_eol(self):
    cmd = [sys.executable, SLEEP, '0.001']
    # 0 means no timeout, like None.
    output, code = run_test_cases.call_with_timeout(
        cmd, 0, universal_newlines=True)
    self.assertEquals('Sleeping.\nSlept.\n', output)
    self.assertEquals(0, code)

  def test_call_timed_out_kill(self):
    cmd = [sys.executable, SLEEP, '100']
    # On a loaded system, this can be tight.
    output, code = run_test_cases.call_with_timeout(cmd, timeout=1)
    self.assertEquals(to_native_eol('Sleeping.\n'), output)
    if sys.platform == 'win32':
      self.assertEquals(1, code)
    else:
      self.assertEquals(-9, code)

  def test_call_timed_out_kill_eol(self):
    cmd = [sys.executable, SLEEP, '100']
    # On a loaded system, this can be tight.
    output, code = run_test_cases.call_with_timeout(
        cmd, timeout=1, universal_newlines=True)
    self.assertEquals('Sleeping.\n', output)
    if sys.platform == 'win32':
      self.assertEquals(1, code)
    else:
      self.assertEquals(-9, code)

  def test_call_timeout_no_kill(self):
    cmd = [sys.executable, SLEEP, '0.001']
    output, code = run_test_cases.call_with_timeout(cmd, timeout=100)
    self.assertEquals(to_native_eol('Sleeping.\nSlept.\n'), output)
    self.assertEquals(0, code)

  def test_call_timeout_no_kill_eol(self):
    cmd = [sys.executable, SLEEP, '0.001']
    output, code = run_test_cases.call_with_timeout(
        cmd, timeout=100, universal_newlines=True)
    self.assertEquals('Sleeping.\nSlept.\n', output)
    self.assertEquals(0, code)

  def test_gtest_filter(self):
    old = run_test_cases.run_test_cases
    exe = os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py')
    def expect(executable, test_cases, jobs, timeout, run_all, result_file):
      self.assertEquals(exe, executable)
      self.assertEquals(['Foo.Bar1', 'Foo.Bar3'], test_cases)
      self.assertEquals(run_test_cases.num_processors(), jobs)
      self.assertEquals(120, timeout)
      self.assertEquals(False, run_all)
      self.assertEquals(exe + '.run_test_cases', result_file)
      return 89
    try:
      run_test_cases.run_test_cases = expect
      result = run_test_cases.main([exe, '--gtest_filter=Foo.Bar*-*.Bar2'])
      self.assertEquals(89, result)
    finally:
      run_test_cases.run_test_cases = old

  def testRunSome(self):
    tests = [
        # Try with named arguments. Accepts 3*1 failures.
        (
          run_test_cases.RunSome(
              expected_count=10,
              retries=3,
              min_failures=1,
              max_failure_ratio=0.001),
          [False] * 4),
        # Same without named arguments.
        (run_test_cases.RunSome(  10, 3, 1, 0.001), [False] * 4),

        (run_test_cases.RunSome(  10, 1, 1, 0.001), [False] * 2),
        (run_test_cases.RunSome(  10, 1, 1, 0.010), [False] * 2),

        # For low expected_count value, retries * min_failures is the minimum
        # bound of accepted failures.
        (run_test_cases.RunSome(  10, 3, 1, 0.010), [False] * 4),
        (run_test_cases.RunSome(  10, 3, 1, 0.020), [False] * 4),
        (run_test_cases.RunSome(  10, 3, 1, 0.050), [False] * 4),
        (run_test_cases.RunSome(  10, 3, 1, 0.100), [False] * 4),
        (run_test_cases.RunSome(  10, 3, 1, 0.110), [False] * 4),

        # Allows expected_count + retries failures at maximum.
        (run_test_cases.RunSome(  10, 3, 1, 0.200), [False] * 6),
        (run_test_cases.RunSome(  10, 3, 1, 0.999), [False] * 30),

        # The asympthote is nearing max_failure_ratio for large expected_count
        # values.
        (run_test_cases.RunSome(1000, 3, 1, 0.050), [False] * 150),
    ]
    for index, (decider, rounds) in enumerate(tests):
      for index2, r in enumerate(rounds):
        self.assertFalse(decider.should_stop(), (index, index2, str(decider)))
        decider.got_result(r)
      self.assertTrue(decider.should_stop(), (index, str(decider)))

  def testStatsInfinite(self):
    decider = run_test_cases.RunAll()
    for _ in xrange(200):
      self.assertFalse(decider.should_stop())
      decider.got_result(False)


class WorkerPoolTest(unittest.TestCase):
  def test_normal(self):
    mapper = lambda value: -value
    with run_test_cases.ThreadPool(8) as pool:
      for i in range(32):
        pool.add_task(mapper, i)
      results = pool.join()
    self.assertEquals(range(-31, 1), sorted(results))

  def test_exception(self):
    class FearsomeException(Exception):
      pass
    def mapper(value):
      raise FearsomeException(value)
    task_added = False
    try:
      with run_test_cases.ThreadPool(8) as pool:
        pool.add_task(mapper, 0)
        task_added = True
        pool.join()
        self.fail()
    except FearsomeException:
      self.assertEquals(True, task_added)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
