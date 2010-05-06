#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for coverage_posix.py."""

import coverage_posix as coverage
import sys
import unittest

class CoveragePosixTest(unittest.TestCase):

  def setUp(self):
    self.parseArgs()

  def parseArgs(self):
    """Setup and process arg parsing."""
    self.parser = coverage.CoverageOptionParser()
    (self.options, self.args) = self.parser.parse_args()

  def testSanity(self):
    """Sanity check we're able to actually run the tests.

    Simply creating a Coverage instance checks a few things (e.g. on
    Windows that the coverage tools can be found)."""
    c = coverage.Coverage('.', self.options, self.args)

  def testRunBasicProcess(self):
    """Test a simple run of a subprocess."""
    c = coverage.Coverage('.', self.options, self.args)
    for code in range(2):
      retcode = c.Run([sys.executable, '-u', '-c',
                       'import sys; sys.exit(%d)' % code],
                      ignore_error=True)
      self.assertEqual(code, retcode)

  def testRunSlowProcess(self):
    """Test program which prints slowly but doesn't hit our timeout.

    Overall runtime is longer than the timeout but output lines
    trickle in keeping things alive.
    """
    self.options.timeout = 2.5
    c = coverage.Coverage('.', self.options, self.args)
    slowscript = ('import sys, time\n'
                  'for x in range(10):\n'
                  '  time.sleep(0.5)\n'
                  '  print "hi mom"\n'
                  'sys.exit(0)\n')
    retcode = c.Run([sys.executable, '-u', '-c', slowscript])
    self.assertEqual(0, retcode)

  def testRunExcessivelySlowProcess(self):
    """Test program which DOES hit our timeout.

    Initial lines should print but quickly it takes too long and
    should be killed.
    """
    self.options.timeout = 2.5
    c = coverage.Coverage('.', self.options, self.args)
    slowscript = ('import time\n'
                  'for x in range(1,10):\n'
                  '  print "sleeping for %d" % x\n'
                  '  time.sleep(x)\n')
    self.assertRaises(Exception,
                      c.Run,
                      [sys.executable, '-u', '-c', slowscript])


if __name__ == '__main__':
  unittest.main()
