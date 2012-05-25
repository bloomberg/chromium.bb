#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
FILENAME = os.path.basename(__file__)
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
    self.tempdir = tempfile.mkdtemp(prefix='trace_smoke_test')
    self.log = os.path.join(self.tempdir, 'log')
    os.chdir(ROOT_DIR)

  def tearDown(self):
    if VERBOSE:
      print 'Leaking: %s' % self.tempdir
    else:
      shutil.rmtree(self.tempdir)

  def _execute(self, is_gyp):
    cmd = [
      sys.executable, os.path.join('..', '..', 'trace_inputs.py'),
      '--log', self.log,
      '--root-dir', ROOT_DIR,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    if is_gyp:
      cmd.extend(
          [
            '--cwd', 'data',
            '--product', '.',  # Not tested.
          ])
    cmd.append(sys.executable)
    if is_gyp:
      # When the gyp argument is specified, the command is started from --cwd
      # directory. In this case, 'data'.
      cmd.extend([os.path.join('trace_inputs', 'child1.py'), '--child-gyp'])
    else:
      # When the gyp argument is not specified, the command is started from
      # --root-dir directory.
      cmd.extend([os.path.join('data', 'trace_inputs', 'child1.py'), '--child'])

    # The current directory doesn't matter, the traced process will be called
    # from the correct cwd.
    cwd = os.path.join('data', 'trace_inputs')
    # Ignore stderr.
    logging.info('Command: %s' % ' '.join(cmd))
    p = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd,
        universal_newlines=True)
    out, err = p.communicate()
    if VERBOSE:
      print err
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out + err, cwd)
    return out or ''

  def test_trace(self):
    expected_end = [
      'Interesting: 5 reduced to 3',
      '  data/trace_inputs/'.replace('/', os.path.sep),
      '  trace_inputs.py',
      '  %s' % FILENAME,
    ]
    actual = self._execute(False).splitlines()
    self.assertTrue(actual[0].startswith('Tracing... ['), actual)
    self.assertTrue(actual[1].startswith('Loading traces... '), actual)
    self.assertTrue(actual[2].startswith('Total: '), actual)
    if sys.platform == 'win32':
      # On windows, python searches the current path for python stdlib like
      # subprocess.py and others, I'm not sure why.
      self.assertTrue(actual[3].startswith('Non existent: '), actual[3])
    else:
      self.assertEquals('Non existent: 0', actual[3])
    # Ignore any Unexpected part.
    # TODO(maruel): Make sure there is no Unexpected part, even in the case of
    # virtualenv usage.
    self.assertEquals(expected_end, actual[-len(expected_end):])

  def test_trace_gyp(self):
    import trace_inputs
    expected_value = {
      'conditions': [
        ['OS=="%s"' % trace_inputs.get_flavor(), {
          'variables': {
            'isolate_dependency_tracked': [
              # It is run from data/trace_inputs.
              '../trace_inputs.py',
              '../%s' % FILENAME,
            ],
            'isolate_dependency_untracked': [
              'trace_inputs/',
            ],
          },
        }],
      ],
    }
    expected_buffer = cStringIO.StringIO()
    trace_inputs.pretty_print(expected_value, expected_buffer)

    actual = self._execute(True)
    self.assertEquals(expected_buffer.getvalue(), actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
