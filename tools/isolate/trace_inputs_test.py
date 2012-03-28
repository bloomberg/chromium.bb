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
  def __init__(self, returncode, cmd, output, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s') % (self.cwd, self.output)


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
      '--root-dir', ROOT_DIR,
    ] + args
    p = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=ROOT_DIR)
    out = p.communicate()[0]
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, ROOT_DIR)
    return out

  @staticmethod
  def _gyp():
    return [
      '--gyp', os.path.join('data', 'trace_inputs'),
      '--product', '.',  # Not tested.
    ]

  def test_trace(self):
    if sys.platform not in ('linux2', 'darwin'):
      print 'WARNING: unsupported: %s' % sys.platform
      return
    expected_end = [
      "Interesting: 4 reduced to 3",
      "  data/trace_inputs/",
      "  trace_inputs.py",
      "  trace_inputs_test.py",
    ]
    actual = self._execute(['trace_inputs_test.py', '--child1']).splitlines()
    self.assertTrue(actual[0].startswith('Tracing... ['))
    self.assertTrue(actual[1].startswith('Loading traces... '))
    self.assertTrue(actual[2].startswith('Total: '))
    self.assertEquals("Non existent: 0", actual[3])
    # Ignore any Unexpected part.
    # TODO(maruel): Make sure there is no Unexpected part, even in the case of
    # virtualenv usage.
    self.assertEquals(expected_end, actual[-len(expected_end):])

  def test_trace_gyp(self):
    if sys.platform not in ('linux2', 'darwin'):
      print 'WARNING: unsupported: %s' % sys.platform
      return
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'isolate_files': [\n"
        "      '<(DEPTH)/trace_inputs.py',\n"
        "      '<(DEPTH)/trace_inputs_test.py',\n"
        "    ],\n"
        "    'isolate_dirs': [\n"
        "      './',\n"
        "    ],\n"
        "  },\n"
        "},\n")
    actual = self._execute(self._gyp() + ['trace_inputs_test.py', '--child1'])
    self.assertEquals(expected, actual)


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
