#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess
import sys
import tempfile
import unittest

import trace_inputs

FILE_PATH = os.path.realpath(unicode(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(FILE_PATH)
TARGET_PATH = os.path.join(ROOT_DIR, 'data', 'gtest_fake', 'gtest_fake.py')


class TraceTestCases(unittest.TestCase):
  def setUp(self):
    self.temp_file = None

    self.initial_cwd = ROOT_DIR
    if sys.platform == 'win32':
      # Windows has no kernel mode concept of current working directory.
      self.initial_cwd = None

    # There's 2 kinds of references to python, self.executable,
    # self.real_executable. It depends how python was started and on which OS.
    self.executable = unicode(sys.executable)
    if sys.platform == 'darwin':
      # /usr/bin/python is a thunk executable that decides which version of
      # python gets executed.
      suffix = '.'.join(map(str, sys.version_info[0:2]))
      if os.access(self.executable + suffix, os.X_OK):
        # So it'll look like /usr/bin/python2.7
        self.executable += suffix

    self.real_executable = trace_inputs.get_native_path_case(
        self.executable)

    if sys.platform == 'darwin':
      # Interestingly, only OSX does resolve the symlink manually before
      # starting the executable.
      if os.path.islink(self.real_executable):
        self.real_executable = os.path.normpath(
            os.path.join(
                os.path.dirname(self.real_executable),
                os.readlink(self.real_executable)))

  def tearDown(self):
    if self.temp_file:
      os.remove(self.temp_file)

  def _gen_results(self, test_case):
    return {
      u'processes': 0,
      u'result': 0,
      u'results': {
        u'root': {
          u'children': [],
          u'command': [
            self.executable,
            TARGET_PATH,
            u'--gtest_filter=%s' % test_case,
          ],
          u'executable': self.real_executable,
          u'files': [
            {
              u'path': os.path.join(u'data', 'gtest_fake', 'gtest_fake.py'),
              u'size': os.stat(TARGET_PATH).st_size,
            },
          ],
          u'initial_cwd': self.initial_cwd,
        },
      },
      u'variables': {
        u'isolate_dependency_tracked': [
          u'<(PRODUCT_DIR)/gtest_fake/gtest_fake.py',
        ],
      },
    }

  def _strip_result(self, result):
    """Strips mutable information from a flattened test case Results."""
    self.assertTrue(result.pop('duration') > 0.)
    self.assertTrue(len(result.pop('output')) > 10)
    def strip_pid(proc):
      self.assertTrue(proc.pop('pid') > 100)
      for child in proc['children']:
        strip_pid(child)
    strip_pid(result['results']['root'])

  def test_simple(self):
    file_handle, self.temp_file = tempfile.mkstemp(
        prefix='trace_test_cases_test')
    os.close(file_handle)

    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'trace_test_cases.py'),
        '--jobs', '1',
        '--timeout', '0',
        '--out', self.temp_file,
        '--root-dir', ROOT_DIR,
        '--cwd', ROOT_DIR,
        '--product-dir', 'data',
        TARGET_PATH,
    ]
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    self.assertEquals(0, proc.returncode)
    if sys.platform == 'win32':
      # TODO(maruel): Figure out why replace('\r\n', '\n') doesn't work.
      out = out.replace('\r', '')
    expected_out = (
      'Found 4 test cases in gtest_fake.py\n'
      '\n'
      'Failed while running: %(path)s --gtest_filter=Baz.Fail\n'
      '\n'
      'Failed while running: %(path)s --gtest_filter=Baz.Fail\n'
      '\n'
      'Failed while running: %(path)s --gtest_filter=Baz.Fail\n'
      '\n'
      'Failed while running: %(path)s --gtest_filter=Baz.Fail\n'
      '\n'
      'Failed while running: %(path)s --gtest_filter=Baz.Fail\n') % {
        'path': TARGET_PATH,
      }
    self.assertEquals(expected_out, out)
    self.assertTrue(err.startswith('\r'), err)

    expected_json = {}
    test_cases = (
      'Baz.Fail',
      'Foo.Bar1',
      'Foo.Bar2',
      'Foo.Bar3',
    )
    for test_case in test_cases:
      expected_json[unicode(test_case)] = self._gen_results(test_case)
    expected_json['Baz.Fail']['result'] = 1
    with open(self.temp_file, 'r') as f:
      result = json.load(f)

    # Trim off 'duration' and 'output', they don't have a constant value.
    for value in result.itervalues():
      self._strip_result(value)
    self.assertEquals(expected_json, result)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
