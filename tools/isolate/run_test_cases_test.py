#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

import run_test_cases

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
SLEEP = os.path.join(ROOT_DIR, 'data', 'run_test_cases', 'sleep.py')


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
    exe = os.path.join(ROOT_DIR, 'data', 'gtest_fake', 'gtest_fake_pass.py')
    def expect(executable, test_cases, jobs, timeout, result_file):
      self.assertEquals(exe, executable)
      self.assertEquals(['Foo.Bar1', 'Foo.Bar3'], test_cases)
      self.assertEquals(run_test_cases.num_processors(), jobs)
      self.assertEquals(120, timeout)
      self.assertEquals(exe + '.run_test_cases', result_file)
      return 89
    try:
      run_test_cases.run_test_cases = expect
      result = run_test_cases.main([exe, '--gtest_filter=Foo.Bar*-*.Bar2'])
      self.assertEquals(89, result)
    finally:
      run_test_cases.run_test_cases = old


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
