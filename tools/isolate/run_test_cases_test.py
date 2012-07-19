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


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
