#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import subprocess
import sys
import tempfile
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

import trace_inputs

FILE_PATH = os.path.realpath(unicode(os.path.abspath(__file__)))
TARGET_UTIL_PATH = os.path.join(BASE_DIR, 'gtest_fake', 'gtest_fake_base.py')
TARGET_PATH = os.path.join(BASE_DIR, 'gtest_fake', 'gtest_fake_fail.py')


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

    self.real_executable = trace_inputs.get_native_path_case(self.executable)
    # Make sure there's no environment variable that could do side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')

  def tearDown(self):
    if self.temp_file:
      os.remove(self.temp_file)

  def _gen_results(self, test_case):
    return {
      u'processes': 1,
      u'returncode': 0,
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
              u'path': os.path.join(u'tests', 'gtest_fake',
                                    'gtest_fake_base.py'),
              u'size': os.stat(TARGET_UTIL_PATH).st_size,
            },
            {
              u'path': os.path.join(u'tests', 'gtest_fake',
                                    'gtest_fake_fail.py'),
              u'size': os.stat(TARGET_PATH).st_size,
            },
          ],
          u'initial_cwd': self.initial_cwd,
        },
      },
      u'valid': True,
      u'variables': {
        u'isolate_dependency_tracked': [
          u'<(PRODUCT_DIR)/gtest_fake/gtest_fake_base.py',
          u'<(PRODUCT_DIR)/gtest_fake/gtest_fake_fail.py',
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
        # Forces 4 parallel jobs.
        '--jobs', '4',
        '--timeout', '0',
        '--out', self.temp_file,
        '--root-dir', ROOT_DIR,
        '--variable', 'PRODUCT_DIR', 'tests',
        TARGET_PATH,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    self.assertEquals(0, proc.returncode, (out, err))
    lines = out.splitlines()
    expected_out_re = [
      r'\[1/4\]   \d\.\d\ds .+',
      r'\[2/4\]   \d\.\d\ds .+',
      r'\[3/4\]   \d\.\d\ds .+',
      r'\[4/4\]   \d\.\d\ds .+',
      r'\d+\.\ds Done post-processing logs\. Parsing logs\.',
      r'\d+\.\ds Done parsing logs\.',
      r'\d+\.\ds Done stripping root\.',
      r'\d+\.\ds Done flattening\.',
    ]
    for index in range(len(expected_out_re)):
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], lines[index]),
          '%d: %s\n%r\n%s' % (
            index, expected_out_re[index], lines[index], out))
    # Junk is printed on win32.
    if sys.platform != 'win32' and not VERBOSE:
      self.assertEquals('', err)

    expected_json = {}
    test_cases = (
      'Baz.Fail',
      'Foo.Bar1',
      'Foo.Bar2',
      'Foo.Bar3',
    )
    for test_case in test_cases:
      expected_json[unicode(test_case)] = self._gen_results(test_case)
    expected_json['Baz.Fail']['returncode'] = 1
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
