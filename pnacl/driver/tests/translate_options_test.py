#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests of the pnacl driver.

This tests that pnacl-translate options pass through to LLC correctly,
and are overridden correctly.
"""

from driver_env import env
import driver_log
import driver_test_utils
import driver_tools

import cStringIO
import os
import re
import sys
import unittest

class TestLLCOptions(driver_test_utils.DriverTesterCommon):
  def setUp(self):
    super(TestLLCOptions, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)
    self.platform = driver_test_utils.GetPlatformToTest()

  def getFakePexe(self):
    # Even --dry-run requires a file to exist, so make a fake pexe.
    # It even cares that the file is really bitcode.
    with self.getTemp(suffix='.ll', close=False) as t:
      with self.getTemp(suffix='.pexe') as p:
        t.write('''
define i32 @main() {
  ret i32 0
}
''')
        t.close()
        driver_tools.RunDriver('as', [t.name, '-o', p.name])
        driver_tools.RunDriver('finalize', [p.name])
        return p


  def checkLLCTranslateFlags(self, pexe, arch, flags,
                             expected_flags):
    ''' Given a |pexe| the |arch| for translation and additional pnacl-translate
    |flags|, check that the commandline for LLC really contains the
    |expected_flags|.  This ensures that the pnacl-translate script
    does not drop certain flags accidentally. '''
    temp_output = self.getTemp()
    # Major hack to prevent DriverExit() from aborting the test.
    # The test will surely DriverLog.Fatal() because dry-run currently
    # does not handle anything that involves invoking a subprocess and
    # grepping the stdout/stderr since it never actually invokes
    # the subprocess. Unfortunately, pnacl-translate does grep the output of
    # the sandboxed LLC run, so we can only go that far with --dry-run.
    capture_out = cStringIO.StringIO()
    driver_log.Log.CaptureToStream(capture_out)
    backup_exit = sys.exit
    sys.exit = driver_test_utils.FakeExit
    self.assertRaises(driver_test_utils.DriverExitException,
                      driver_tools.RunDriver,
                      'translate',
                      ['--pnacl-driver-verbose',
                       '--dry-run',
                       '-arch', arch,
                       pexe.name,
                       '-o', temp_output.name] + flags)
    driver_log.Log.ResetStreams()
    out = capture_out.getvalue()
    sys.exit = backup_exit
    for f in expected_flags:
      self.assertTrue(re.search(f, out),
                      msg='Searching for regex %s in %s' % (f, out))
    return


  #### Individual tests.

  def test_no_overrides(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      if self.platform == 'arm':
        expected_triple_cpu = ['-mtriple=arm.*', '-mcpu=cortex.*']
      elif self.platform == 'x86-32':
        expected_triple_cpu = ['-mtriple=i686.*', '-mcpu=pentium.*']
      elif self.platform == 'x86-64':
        expected_triple_cpu = ['-mtriple=x86_64.*', '-mcpu=core.*']
      else:
        raise Exception('Unknown platform')
      # Test that certain defaults are set, when no flags are given.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          [],
          expected_triple_cpu)
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['--pnacl-sb'],
          ['StreamInit h'])

  def test_overrideO0(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      # Test that you get O0 when you ask for O0.
      # You also get no frame pointer elimination.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-O0'],
          ['-O0', '-disable-fp-elim '])
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-O0', '--pnacl-sb'],
          ['StreamInitWithOverrides.*-O0.*-disable-fp-elim.*-mcpu=.*'])

  def test_overrideTranslateFast(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      # Test that you get O0 when you ask for -translate-fast.
      # In this case... you don't get no frame pointer elimination.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-translate-fast'],
          ['-O0'])
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-translate-fast', '--pnacl-sb'],
          ['StreamInitWithOverrides.*-O0.*-mcpu=.*'])

  def test_overrideTLSUseCall(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      # Test that you -mtls-use-call, etc. when you ask for it.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-mtls-use-call', '-fdata-sections', '-ffunction-sections'],
          ['-mtls-use-call', '-fdata-sections', '-ffunction-sections'])
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          ['-mtls-use-call', '-fdata-sections', '-ffunction-sections',
           '--pnacl-sb'],
          ['StreamInitWithOverrides.*-mtls-use-call' +
           '.*-fdata-sections.*-ffunction-sections'])

  def test_overrideMCPU(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      if self.platform == 'arm':
        mcpu_pattern = '-mcpu=cortex-a15'
      elif self.platform == 'x86-32':
        mcpu_pattern = '-mcpu=atom'
      elif self.platform == 'x86-64':
        mcpu_pattern = '-mcpu=corei7'
      else:
        raise Exception('Unknown platform')
      # Test that you get the -mcpu that you ask for.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          [mcpu_pattern],
          [mcpu_pattern])
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          [mcpu_pattern, '--pnacl-sb'],
          ['StreamInitWithOverrides.*' + mcpu_pattern])

  def test_overrideMAttr(self):
    if driver_test_utils.CanRunHost():
      pexe = self.getFakePexe()
      if self.platform == 'arm':
        mattr_flags, mattr_pat = '-mattr=+hwdiv', r'-mattr=\+hwdiv'
      elif self.platform == 'x86-32' or self.platform == 'x86-64':
        mattr_flags, mattr_pat = '-mattr=+avx2,+sse41', r'-mattr=\+avx2,\+sse41'
      else:
        raise Exception('Unknown platform')
      # Test that you get the -mattr=.* that you ask for.
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          [mattr_flags],
          [mattr_pat])
      self.checkLLCTranslateFlags(
          pexe,
          self.platform,
          [mattr_flags, '--pnacl-sb'],
          ['StreamInitWithOverrides.*' + mattr_pat + '.*-mcpu=.*'])


if __name__ == '__main__':
  unittest.main()
