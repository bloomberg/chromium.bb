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

FULLNAME = os.path.abspath(__file__)
ROOT_DIR = os.path.dirname(FULLNAME)
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


class TraceInputsBase(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp(prefix='trace_smoke_test')
    self.log = os.path.join(self.tempdir, 'log')
    os.chdir(ROOT_DIR)

  def tearDown(self):
    if VERBOSE:
      print 'Leaking: %s' % self.tempdir
    else:
      shutil.rmtree(self.tempdir)

  @staticmethod
  def command(from_data):
    cmd = [sys.executable]
    if from_data:
      # When the gyp argument is specified, the command is started from --cwd
      # directory. In this case, 'data'.
      cmd.extend([os.path.join('trace_inputs', 'child1.py'), '--child-gyp'])
    else:
      # When the gyp argument is not specified, the command is started from
      # --root-dir directory.
      cmd.extend([os.path.join('data', 'trace_inputs', 'child1.py'), '--child'])
    return cmd


class TraceInputs(TraceInputsBase):
  def _execute(self, command):
    cmd = [
      sys.executable, os.path.join('..', '..', 'trace_inputs.py'),
      '--log', self.log,
      '--root-dir', ROOT_DIR,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    cmd.extend(command)
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
    actual = self._execute(self.command(False)).splitlines()
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

    cmd = [
      '--cwd', 'data',
      '--product', '.',  # Not tested.
    ] + self.command(True)
    actual = self._execute(cmd)
    self.assertEquals(expected_buffer.getvalue(), actual)


class TraceInputsImport(TraceInputsBase):
  def setUp(self):
    super(TraceInputsImport, self).setUp()
    self.cwd = os.path.join(ROOT_DIR, u'data')
    self.initial_cwd = self.cwd
    if sys.platform == 'win32':
      # Still not supported on Windows.
      self.initial_cwd = None

  # Similar to TraceInputs test fixture except that it calls the function
  # directly, so the Results instance can be inspected.
  # Roughly, make sure the API is stable.
  def _execute(self, command):
    # Similar to what trace_test_cases.py does.
    import trace_inputs
    api = trace_inputs.get_api()
    _, _ = trace_inputs.trace(
          self.log, command, self.cwd, api, True)
    # TODO(maruel): Check
    #self.assertEquals(0, returncode)
    #self.assertEquals('', output)
    return trace_inputs.load_trace(self.log, ROOT_DIR, api)

  def test_trace_wrong_path(self):
    # Deliberately start the trace from the wrong path. Starts it from the
    # directory 'data' so 'data/data/trace_inputs/child1.py' is not accessible,
    # so child2.py process is not started.
    results, simplified = self._execute(self.command(False))
    expected = {
      'root': {
        'children': [],
        'command': None,
        'executable': None,
        'files': [],
        'initial_cwd': self.initial_cwd,
      },
    }
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEquals(expected, actual)
    self.assertEquals([], simplified)

  def test_trace(self):
    size_t_i_s = os.stat(FULLNAME).st_size
    size_t_i = os.stat(os.path.join(ROOT_DIR, 'trace_inputs.py')).st_size
    expected = {
      'root': {
        'children': [
          {
            'children': [],
            'command': None,
            'executable': None,
            'files': [
              {
                'path': os.path.join(u'data', 'trace_inputs', 'child2.py'),
                'size': 776,
              },
              {
                'path': os.path.join(u'data', 'trace_inputs', 'test_file.txt'),
                'size': 4,
              },
            ],
            'initial_cwd': self.initial_cwd,
          },
        ],
        'command': None,
        'executable': None,
        'files': [
          {
            'path': os.path.join(u'data', 'trace_inputs', 'child1.py'),
            'size': 1364,
          },
          {
            'path': u'trace_inputs.py',
            'size': size_t_i,
          },
          {
            'path': u'trace_inputs_smoke_test.py',
            'size': size_t_i_s,
          },
        ],
        'initial_cwd': self.initial_cwd,
      },
    }
    results, simplified = self._execute(self.command(True))
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertTrue(actual['root']['children'][0].pop('pid'))
    self.assertEquals(expected, actual)
    files = [
      os.path.join(u'data', 'trace_inputs') + os.path.sep,
      u'trace_inputs.py',
      u'trace_inputs_smoke_test.py',
    ]
    self.assertEquals(files, [f.path for f in simplified])



if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
