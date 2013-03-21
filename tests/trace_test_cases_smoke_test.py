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
sys.path.insert(0, os.path.join(BASE_DIR, 'gtest_fake'))

import trace_inputs
import gtest_fake_base

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

  def assertGreater(self, a, b, msg=None):
    """Just like self.assertTrue(a > b), but with a nicer default message.

    Added to support python 2.6.
    """
    if not a > b:
      standardMsg = '%r not greater than %r' % (a, b)
      self.fail(msg or standardMsg)

  def test_simple(self):
    file_handle, self.temp_file = tempfile.mkstemp(
        prefix='trace_test_cases_test')
    os.close(file_handle)

    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'trace_test_cases.py'),
        # Forces 4 parallel jobs.
        '--jobs', '4',
        '--out', self.temp_file,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    cmd.append(TARGET_PATH)
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        universal_newlines=True,
        cwd=ROOT_DIR)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    self.assertEqual(0, proc.returncode, (out, err))
    lines = out.splitlines()
    expected_out_re = [
      r'Tracing\.\.\.',
      r'\[1/4\] +\d+\.\d\ds .+',
      r'\[2/4\] +\d+\.\d\ds .+',
      r'\[3/4\] +\d+\.\d\ds .+',
      r'\[4/4\] +\d+\.\d\ds .+',
      r'Reading trace logs\.\.\.',
    ]
    self.assertEqual(len(expected_out_re), len(lines), lines)
    for index in range(len(expected_out_re)):
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], lines[index]),
          '%d: %s\n%r\n%s' % (
            index, expected_out_re[index], lines[index], out))
    # Junk is printed on win32.
    if sys.platform != 'win32' and not VERBOSE:
      self.assertEqual('', err)

    with open(self.temp_file, 'r') as f:
      content = f.read()
      try:
        result = json.loads(content)
      except:
        print repr(content)
        raise

    test_cases = {
      'Baz.Fail': 1,
      'Foo.Bar1': 0,
      'Foo.Bar2': 0,
      'Foo.Bar3': 0,
    }
    self.assertEqual(dict, result.__class__)
    self.assertEqual(sorted(test_cases), sorted(result))
    for index, test_case in enumerate(sorted(result)):
      actual = result[test_case]
      self.assertEqual(
          [u'duration', u'output', u'returncode', u'trace'], sorted(actual))
      self.assertGreater(actual['duration'], 0.0000001)
      self.assertEqual(test_cases[test_case], actual['returncode'])
      expected_output = (
          'Note: Google Test filter = %s\n' % test_case +
          '\n' +
          gtest_fake_base.get_test_output(test_case, 'Fail' in test_case) +
          '\n' +
          gtest_fake_base.get_footer(1, 1) +
          '\n')
      # On Windows, actual['output'] is unprocessed so it will contain CRLF.
      output = actual['output']
      if sys.platform == 'win32':
        output = output.replace('\r\n', '\n')
      self.assertEqual(expected_output, output, repr(output))

      expected_trace = {
        u'root': {
          u'children': [],
          u'command': [
            self.executable, TARGET_PATH, '--gtest_filter=' + test_case,
          ],
          u'executable': trace_inputs.get_native_path_case(
            unicode(self.executable)),
          u'initial_cwd': ROOT_DIR,
        },
      }
      if sys.platform == 'win32':
        expected_trace['root']['initial_cwd'] = None
      self.assertGreater(actual['trace']['root'].pop('pid'), 1)
      self.assertGreater(len(actual['trace']['root'].pop('files')), 10)
      self.assertEqual(expected_trace, actual['trace'])


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  # Necessary for the dtrace logger to work around execve() hook. See
  # trace_inputs.py for more details.
  os.environ['TRACE_INPUTS_DTRACE_ENABLE_EXECVE'] = '1'
  unittest.main()
