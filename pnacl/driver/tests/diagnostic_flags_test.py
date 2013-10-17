#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests of the pnacl driver.

This tests that diagnostic flags work fine, even with --pnacl-* flags.
"""
import cStringIO
import os
import unittest

from driver_env import env
import driver_log
import driver_test_utils
import driver_tools


def exists(substring, list_of_strings):
  for string in list_of_strings:
    if substring in string:
      return True
  return False

class TestDiagnosticFlags(unittest.TestCase):

  def setUp(self):
    super(TestDiagnosticFlags, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)

  def check_flags(self, flags, pnacl_flag):
    """ Check that pnacl_flag doesn't get passed to clang.

    It should only be in the 'Driver invocation:' part that we print out.
    """
    capture_out = cStringIO.StringIO()
    driver_log.Log.CaptureToStream(capture_out)
    driver_tools.RunDriver('clang', flags + [pnacl_flag])
    driver_log.Log.ResetStreams()
    out = capture_out.getvalue()
    lines = out.splitlines()
    self.assertTrue('Driver invocation:' in lines[0])
    self.assertTrue(pnacl_flag in lines[0])
    for line in lines[1:]:
      self.assertTrue(pnacl_flag not in line)
    for flag in flags:
      self.assertTrue(exists(flag, lines[1:]))

  def test_with_pnacl_flag(self):
    """ Test that we do not pass the made-up --pnacl-* flags along to clang.

    Previously, when we ran --version and other "diagnostic" flags, there
    was a special invocation of clang, where we did not filter out the
    --pnacl-* driver-only flags.
    """
    if not driver_test_utils.CanRunHost():
      return
    # Pick a particular --pnacl-* flag to test. Don't use
    # --pnacl-driver-verbose to test this, because --pnacl-driver-verbose
    # has a side-effect that isn't cleared by the env.pop() of RunDriver().
    pnacl_flag = '--pnacl-disable-abi-check'
    self.check_flags(['--version'], pnacl_flag)
    self.check_flags(['-v'], pnacl_flag)
    self.check_flags(['-v', '-E', '-xc', os.devnull], pnacl_flag)
    self.check_flags(['-print-file-name=libc'], pnacl_flag)
    self.check_flags(['-print-libgcc-file-name'], pnacl_flag)
    self.check_flags(['-print-multi-directory'], pnacl_flag)
    self.check_flags(['-print-multi-lib'], pnacl_flag)
    self.check_flags(['-print-multi-os-directory'], pnacl_flag)

if __name__ == '__main__':
  unittest.main()
