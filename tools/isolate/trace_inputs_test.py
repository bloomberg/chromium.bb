#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

VERBOSE = False


class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output


class TraceInputs(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp()
    self.log = os.path.join(self.tempdir, 'log')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _execute(self, args):
    cmd = [
        sys.executable, os.path.join(ROOT_DIR, 'trace_inputs.py'),
        '--log', self.log,
        '--gyp', os.path.join('data', 'trace_inputs'),
        '--product', '.',  # Not tested.
        '--root-dir', ROOT_DIR,
    ] + args
    p = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=ROOT_DIR)
    out = p.communicate()[0]
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out)
    return out

  def test_trace(self):
    if sys.platform == 'linux2':
      return self._test_trace_linux()
    if sys.platform == 'darwin':
      return self._test_trace_mac()
    print 'Unsupported: %s' % sys.platform

  def _test_trace_linux(self):
    # TODO(maruel): BUG: Note that child.py is missing.
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'isolate_files': [\n"
        "      '<(DEPTH)/trace_inputs.py',\n"
        "      '<(DEPTH)/trace_inputs_test.py',\n"
        "    ],\n"
        "    'isolate_dirs': [\n"
        "    ],\n"
        "  },\n"
        "},\n")
    gyp = self._execute(['trace_inputs_test.py', '--child1'])
    self.assertEquals(expected, gyp)

  def _test_trace_mac(self):
    # It is annoying in the case of dtrace because it requires root access.
    # TODO(maruel): BUG: Note that child.py is missing.
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'isolate_files': [\n"
        "      '<(DEPTH)/trace_inputs.py',\n"
        "      '<(DEPTH)/trace_inputs_test.py',\n"
        "    ],\n"
        "    'isolate_dirs': [\n"
        "    ],\n"
        "  },\n"
        "},\n")
    gyp = self._execute(['trace_inputs_test.py', '--child1'])
    self.assertEquals(expected, gyp)


def child1():
  print 'child1'
  # Implicitly force file opening.
  import trace_inputs  # pylint: disable=W0612
  # Do not wait for the child to exit.
  # Use relative directory.
  subprocess.Popen(
      ['python', 'child2.py'], cwd=os.path.join('data', 'trace_inputs'))
  return 0


def main():
  global VERBOSE
  VERBOSE = '-v' in sys.argv
  level = logging.DEBUG if VERBOSE else logging.ERROR
  logging.basicConfig(level=level)
  if len(sys.argv) == 1:
    unittest.main()

  if sys.argv[1] == '--child1':
    return child1()

  unittest.main()


if __name__ == '__main__':
  sys.exit(main())
