#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_test_cases

FILENAME = os.path.basename(__file__)
REL_DATA = os.path.join(u'tests', 'trace_inputs')
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
    self.trace_inputs_path = os.path.join(ROOT_DIR, 'trace_inputs.py')

    # Wraps up all the differences between OSes here.
    # - Windows doesn't track initial_cwd.
    # - OSX replaces /usr/bin/python with /usr/bin/python2.7.
    self.cwd = os.path.join(ROOT_DIR, u'tests')
    self.initial_cwd = unicode(self.cwd)
    self.expected_cwd = unicode(ROOT_DIR)
    if sys.platform == 'win32':
      # Not supported on Windows.
      self.initial_cwd = None
      self.expected_cwd = None

    # There's 3 kinds of references to python, self.executable,
    # self.real_executable and self.naked_executable. It depends how python was
    # started.
    self.executable = sys.executable
    if sys.platform == 'darwin':
      # /usr/bin/python is a thunk executable that decides which version of
      # python gets executed.
      suffix = '.'.join(map(str, sys.version_info[0:2]))
      if os.access(self.executable + suffix, os.X_OK):
        # So it'll look like /usr/bin/python2.7
        self.executable += suffix

    import trace_inputs
    self.real_executable = trace_inputs.get_native_path_case(
        unicode(self.executable))
    trace_inputs = None

    # self.naked_executable will only be naked on Windows.
    self.naked_executable = unicode(sys.executable)
    if sys.platform == 'win32':
      self.naked_executable = os.path.basename(sys.executable)

  def tearDown(self):
    if VERBOSE:
      print 'Leaking: %s' % self.tempdir
    else:
      shutil.rmtree(self.tempdir)

  @staticmethod
  def get_child_command(from_data):
    """Returns command to run the child1.py."""
    cmd = [sys.executable]
    if from_data:
      # When the gyp argument is specified, the command is started from --cwd
      # directory. In this case, 'tests'.
      cmd.extend([os.path.join('trace_inputs', 'child1.py'), '--child-gyp'])
    else:
      # When the gyp argument is not specified, the command is started from
      # --root-dir directory.
      cmd.extend([os.path.join(REL_DATA, 'child1.py'), '--child'])
    return cmd

  @staticmethod
  def _size(*args):
    return os.stat(os.path.join(ROOT_DIR, *args)).st_size


class TraceInputs(TraceInputsBase):
  def _execute(self, mode, command, cwd):
    cmd = [
      sys.executable,
      self.trace_inputs_path,
      mode,
      '--log', self.log,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    cmd.extend(command)
    logging.info('Command: %s' % ' '.join(cmd))
    p = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=cwd,
        universal_newlines=True)
    out, err = p.communicate()
    if VERBOSE:
      print err
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out + err, cwd)
    return out or ''

  def _trace(self, from_data):
    if from_data:
      cwd = os.path.join(ROOT_DIR, 'tests')
    else:
      cwd = ROOT_DIR
    return self._execute('trace', self.get_child_command(from_data), cwd=cwd)

  def test_trace(self):
    expected = '\n'.join((
      'Total: 7',
      'Non existent: 0',
      'Interesting: 7 reduced to 6',
      '  tests/trace_inputs/child1.py'.replace('/', os.path.sep),
      '  tests/trace_inputs/child2.py'.replace('/', os.path.sep),
      '  tests/trace_inputs/files1/'.replace('/', os.path.sep),
      '  tests/trace_inputs/test_file.txt'.replace('/', os.path.sep),
      ('  tests/%s' % FILENAME).replace('/', os.path.sep),
      '  trace_inputs.py',
    )) + '\n'
    trace_expected = '\n'.join((
      'child from %s' % ROOT_DIR,
      'child2',
    )) + '\n'
    trace_actual = self._trace(False)
    actual = self._execute(
        'read',
        [
          '--root-dir', ROOT_DIR,
          '--blacklist', '.+\\.pyc',
          '--blacklist', '.*\\.svn',
          '--blacklist', '.*do_not_care\\.txt',
        ],
        cwd=ROOT_DIR)
    self.assertEquals(expected, actual)
    self.assertEquals(trace_expected, trace_actual)

  def test_trace_json(self):
    expected = {
      u'root': {
        u'children': [
          {
            u'children': [],
            u'command': [u'python', u'child2.py'],
            u'executable': self.naked_executable,
            u'files': [
              {
                u'path': os.path.join(REL_DATA, 'child2.py'),
                u'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                u'path': os.path.join(REL_DATA, 'files1', 'bar'),
                u'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                u'path': os.path.join(REL_DATA, 'files1', 'foo'),
                u'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                u'path': os.path.join(REL_DATA, 'test_file.txt'),
                u'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            u'initial_cwd': self.initial_cwd,
            #u'pid': 123,
          },
        ],
        u'command': [
          unicode(self.executable),
          os.path.join(u'trace_inputs', 'child1.py'),
          u'--child-gyp',
        ],
        u'executable': self.real_executable,
        u'files': [
          {
            u'path': os.path.join(REL_DATA, 'child1.py'),
            u'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            u'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            u'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            u'path': u'trace_inputs.py',
            u'size': self._size('trace_inputs.py'),
          },
        ],
        u'initial_cwd': self.initial_cwd,
        #u'pid': 123,
      },
    }
    trace_expected = '\n'.join((
      'child_gyp from %s' % os.path.join(ROOT_DIR, 'tests'),
      'child2',
    )) + '\n'
    trace_actual = self._trace(True)
    actual_text = self._execute(
        'read',
        [
          '--root-dir', ROOT_DIR,
          '--blacklist', '.+\\.pyc',
          '--blacklist', '.*\\.svn',
          '--blacklist', '.*do_not_care\\.txt',
          '--json',
        ],
        cwd=ROOT_DIR)
    actual_json = json.loads(actual_text)
    # Removes the pids.
    self.assertTrue(actual_json['root'].pop('pid'))
    self.assertTrue(actual_json['root']['children'][0].pop('pid'))
    self.assertEquals(expected, actual_json)
    self.assertEquals(trace_expected, trace_actual)


class TraceInputsImport(TraceInputsBase):
  def setUp(self):
    super(TraceInputsImport, self).setUp()
    import trace_inputs
    self.trace_inputs = trace_inputs

  def tearDown(self):
    del self.trace_inputs
    super(TraceInputsImport, self).tearDown()

  # Similar to TraceInputs test fixture except that it calls the function
  # directly, so the Results instance can be inspected.
  # Roughly, make sure the API is stable.
  def _execute_trace(self, command):
    # Similar to what trace_test_cases.py does.
    api = self.trace_inputs.get_api()
    _, _ = self.trace_inputs.trace(
          self.log, command, self.cwd, api, True)
    # TODO(maruel): Check
    #self.assertEquals(0, returncode)
    #self.assertEquals('', output)
    def blacklist(f):
      return f.endswith(('.pyc', '.svn', 'do_not_care.txt'))
    return self.trace_inputs.load_trace(self.log, ROOT_DIR, api, blacklist)

  def _gen_dict_wrong_path(self):
    """Returns the expected flattened Results when child1.py is called with the
    wrong relative path.
    """
    return {
      'root': {
        'children': [],
        'command': [
          self.executable,
          os.path.join(REL_DATA, 'child1.py'),
          '--child',
        ],
        'executable': self.real_executable,
        'files': [],
        'initial_cwd': self.initial_cwd,
      },
    }

  def _gen_dict_full(self):
    """Returns the expected flattened Results when child1.py is called with
    --child.
    """
    return {
      'root': {
        'children': [
          {
            'children': [],
            'command': ['python', 'child2.py'],
            'executable': self.naked_executable,
            'files': [
              {
                'path': os.path.join(REL_DATA, 'child2.py'),
                'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                'path': os.path.join(REL_DATA, 'files1', 'bar'),
                'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                'path': os.path.join(REL_DATA, 'files1', 'foo'),
                'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                'path': os.path.join(REL_DATA, 'test_file.txt'),
                'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            'initial_cwd': self.expected_cwd,
          },
        ],
        'command': [
          self.executable,
          os.path.join(REL_DATA, 'child1.py'),
          '--child',
        ],
        'executable': self.real_executable,
        'files': [
          {
            'path': os.path.join(REL_DATA, 'child1.py'),
            'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            u'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            'path': u'trace_inputs.py',
            'size': self._size('trace_inputs.py'),
          },
        ],
        'initial_cwd': self.expected_cwd,
      },
    }

  def _gen_dict_full_gyp(self):
    """Returns the expected flattened results when child1.py is called with
    --child-gyp.
    """
    return {
      'root': {
        'children': [
          {
            'children': [],
            'command': ['python', 'child2.py'],
            'executable': self.naked_executable,
            'files': [
              {
                'path': os.path.join(REL_DATA, 'child2.py'),
                'size': self._size(REL_DATA, 'child2.py'),
              },
              {
                'path': os.path.join(REL_DATA, 'files1', 'bar'),
                'size': self._size(REL_DATA, 'files1', 'bar'),
              },
              {
                'path': os.path.join(REL_DATA, 'files1', 'foo'),
                'size': self._size(REL_DATA, 'files1', 'foo'),
              },
              {
                'path': os.path.join(REL_DATA, 'test_file.txt'),
                'size': self._size(REL_DATA, 'test_file.txt'),
              },
            ],
            'initial_cwd': self.initial_cwd,
          },
        ],
        'command': [
          self.executable,
          os.path.join('trace_inputs', 'child1.py'),
          '--child-gyp',
        ],
        'executable': self.real_executable,
        'files': [
          {
            'path': os.path.join(REL_DATA, 'child1.py'),
            'size': self._size(REL_DATA, 'child1.py'),
          },
          {
            'path': os.path.join(u'tests', u'trace_inputs_smoke_test.py'),
            'size': self._size('tests', 'trace_inputs_smoke_test.py'),
          },
          {
            'path': u'trace_inputs.py',
            'size': self._size('trace_inputs.py'),
          },
        ],
        'initial_cwd': self.initial_cwd,
      },
    }

  def test_trace_wrong_path(self):
    # Deliberately start the trace from the wrong path. Starts it from the
    # directory 'tests' so 'tests/tests/trace_inputs/child1.py' is not
    # accessible, so child2.py process is not started.
    results = self._execute_trace(self.get_child_command(False))
    expected = self._gen_dict_wrong_path()
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEquals(expected, actual)

  def test_trace(self):
    expected = self._gen_dict_full_gyp()
    results = self._execute_trace(self.get_child_command(True))
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertTrue(actual['root']['children'][0].pop('pid'))
    self.assertEquals(expected, actual)
    files = [
      u'tests/trace_inputs/child1.py'.replace('/', os.path.sep),
      u'tests/trace_inputs/child2.py'.replace('/', os.path.sep),
      u'tests/trace_inputs/files1/'.replace('/', os.path.sep),
      u'tests/trace_inputs/test_file.txt'.replace('/', os.path.sep),
      u'tests/trace_inputs_smoke_test.py'.replace('/', os.path.sep),
      u'trace_inputs.py',
    ]
    def blacklist(f):
      return f.endswith(('.pyc', 'do_not_care.txt', '.git', '.svn'))
    simplified = self.trace_inputs.extract_directories(
        ROOT_DIR, results.files, blacklist)
    self.assertEquals(files, [f.path for f in simplified])

  def test_trace_multiple(self):
    # Starts parallel threads and trace parallel child processes simultaneously.
    # Some are started from 'tests' directory, others from this script's
    # directory. One trace fails. Verify everything still goes one.
    parallel = 8

    def trace(tracer, cmd, cwd, tracename):
      resultcode, output = tracer.trace(
          cmd, cwd, tracename, True)
      return (tracename, resultcode, output)

    with run_test_cases.ThreadPool(parallel) as pool:
      api = self.trace_inputs.get_api()
      with api.get_tracer(self.log) as tracer:
        pool.add_task(
            trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace1')
        pool.add_task(
            trace, tracer, self.get_child_command(True), self.cwd, 'trace2')
        pool.add_task(
            trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace3')
        pool.add_task(
            trace, tracer, self.get_child_command(True), self.cwd, 'trace4')
        # Have this one fail since it's started from the wrong directory.
        pool.add_task(
            trace, tracer, self.get_child_command(False), self.cwd, 'trace5')
        pool.add_task(
            trace, tracer, self.get_child_command(True), self.cwd, 'trace6')
        pool.add_task(
            trace, tracer, self.get_child_command(False), ROOT_DIR, 'trace7')
        pool.add_task(
            trace, tracer, self.get_child_command(True), self.cwd, 'trace8')
        trace_results = pool.join()
    def blacklist(f):
      return f.endswith(('.pyc', 'do_not_care.txt', '.git', '.svn'))
    actual_results = api.parse_log(self.log, blacklist)
    self.assertEquals(8, len(trace_results))
    self.assertEquals(8, len(actual_results))

    # Convert to dict keyed on the trace name, simpler to verify.
    trace_results = dict((i[0], i[1:]) for i in trace_results)
    actual_results = dict((x.pop('trace'), x) for x in actual_results)
    self.assertEquals(sorted(trace_results), sorted(actual_results))

    # It'd be nice to start different kinds of processes.
    expected_results = [
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
      self._gen_dict_wrong_path(),
      self._gen_dict_full_gyp(),
      self._gen_dict_full(),
      self._gen_dict_full_gyp(),
    ]
    self.assertEquals(len(expected_results), len(trace_results))

    # See the comment above about the trace that fails because it's started from
    # the wrong directory.
    busted = 4
    for index, key in enumerate(sorted(actual_results)):
      self.assertEquals('trace%d' % (index + 1), key)
      self.assertEquals(2, len(trace_results[key]))
      # returncode
      self.assertEquals(0 if index != busted else 2, trace_results[key][0])
      # output
      self.assertEquals(actual_results[key]['output'], trace_results[key][1])

      self.assertEquals(['output', 'results'], sorted(actual_results[key]))
      results = actual_results[key]['results']
      results = results.strip_root(ROOT_DIR)
      actual = results.flatten()
      self.assertTrue(actual['root'].pop('pid'))
      if index != busted:
        self.assertTrue(actual['root']['children'][0].pop('pid'))
      self.assertEquals(expected_results[index], actual)

  if sys.platform != 'win32':
    def test_trace_symlink(self):
      expected = {
        'root': {
          'children': [],
          'command': [
            self.executable,
            os.path.join('trace_inputs', 'symlink.py'),
          ],
          'executable': self.real_executable,
          'files': [
            {
              'path': os.path.join(REL_DATA, 'files2', 'bar'),
              'size': self._size(REL_DATA, 'files2', 'bar'),
            },
            {
              'path': os.path.join(REL_DATA, 'files2', 'foo'),
              'size': self._size(REL_DATA, 'files2', 'foo'),
            },
            {
              'path': os.path.join(REL_DATA, 'symlink.py'),
              'size': self._size(REL_DATA, 'symlink.py'),
            },
          ],
          'initial_cwd': self.initial_cwd,
        },
      }
      cmd = [sys.executable, os.path.join('trace_inputs', 'symlink.py')]
      results = self._execute_trace(cmd)
      actual = results.flatten()
      self.assertTrue(actual['root'].pop('pid'))
      self.assertEquals(expected, actual)
      files = [
        # In particular, the symlink is *not* resolved.
        u'tests/trace_inputs/files2/'.replace('/', os.path.sep),
        u'tests/trace_inputs/symlink.py'.replace('/', os.path.sep),
      ]
      def blacklist(f):
        return f.endswith(('.pyc', '.svn', 'do_not_care.txt'))
      simplified = self.trace_inputs.extract_directories(
          ROOT_DIR, results.files, blacklist)
      self.assertEquals(files, [f.path for f in simplified])

  def test_trace_quoted(self):
    results = self._execute_trace([sys.executable, '-c', 'print("hi")'])
    expected = {
      'root': {
        'children': [],
        'command': [
          self.executable,
          '-c',
          'print("hi")',
        ],
        'executable': self.real_executable,
        'files': [],
        'initial_cwd': self.initial_cwd,
      },
    }
    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEquals(expected, actual)

  def _touch_expected(self, command):
    # Looks for file that were touched but not opened, using different apis.
    results = self._execute_trace(
      [sys.executable, os.path.join('trace_inputs', 'touch_only.py'), command])
    expected = {
      'root': {
        'children': [],
        'command': [
          self.executable,
          os.path.join('trace_inputs', 'touch_only.py'),
          command,
        ],
        'executable': self.real_executable,
        'files': [
          {
            'path': os.path.join(REL_DATA, 'test_file.txt'),
            'size': 0,
          },
          {
            'path': os.path.join(REL_DATA, 'touch_only.py'),
            'size': self._size(REL_DATA, 'touch_only.py'),
          },
        ],
        'initial_cwd': self.initial_cwd,
      },
    }
    if sys.platform != 'linux2':
      # TODO(maruel): Remove once properly implemented.
      expected['root']['files'].pop(0)

    actual = results.flatten()
    self.assertTrue(actual['root'].pop('pid'))
    self.assertEquals(expected, actual)

  def test_trace_touch_only_access(self):
    self._touch_expected('access')

  def test_trace_touch_only_isfile(self):
    self._touch_expected('isfile')

  def test_trace_touch_only_stat(self):
    self._touch_expected('stat')


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
