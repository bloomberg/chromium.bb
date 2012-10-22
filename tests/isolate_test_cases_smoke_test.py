#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

import isolate


class IsolateTestCases(unittest.TestCase):
  def setUp(self):
    self.tempdir = None

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

    self.real_executable = isolate.trace_inputs.get_native_path_case(
        self.executable)
    # Make sure there's no environment variable that could cause side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def _copy(self, *relpath):
    relpath = os.path.join(*relpath)
    shutil.copy(
        os.path.join(ROOT_DIR, relpath),
        os.path.join(self.tempdir, relpath))

  def test_simple(self):
    # Create a directory and re-use tests/gtest_fake/gtest_fake_pass.isolate.
    # Warning: we need to copy the files around, since the original .isolate
    # file is modified.
    gtest_fake_base_py = os.path.join(
        'tests', 'gtest_fake', 'gtest_fake_base.py')
    gtest_fake_pass_py = os.path.join(
        'tests', 'gtest_fake', 'gtest_fake_pass.py')
    gtest_fake_pass_isolate = os.path.join(
        'tests', 'isolate_test_cases', 'gtest_fake_pass.isolate')

    self.tempdir = tempfile.mkdtemp(prefix='isolate_test_cases_test')
    os.mkdir(os.path.join(self.tempdir, 'isolated'))
    os.mkdir(os.path.join(self.tempdir, 'tests'))
    os.mkdir(os.path.join(self.tempdir, 'tests', 'gtest_fake'))
    os.mkdir(os.path.join(self.tempdir, 'tests', 'isolate_test_cases'))
    self._copy('isolate.py')
    self._copy(gtest_fake_base_py)
    self._copy(gtest_fake_pass_isolate)
    self._copy(gtest_fake_pass_py)

    basename = os.path.join(self.tempdir, 'isolated', 'gtest_fake_pass')
    isolated = basename + '.isolated'

    # Create a proper .isolated file.
    cmd = [
      sys.executable, 'isolate.py',
      'check',
      '-V', 'FLAG', 'run',
      '-i', os.path.join(self.tempdir, gtest_fake_pass_isolate),
      '-r', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    subprocess.check_call(cmd, cwd=ROOT_DIR)

    # Assert the content of the .isolated file.
    with open(isolated) as f:
      actual_isolated = json.load(f)
    root_dir_gtest_fake_pass_py = os.path.join(ROOT_DIR, gtest_fake_pass_py)
    rel_gtest_fake_pass_py = os.path.join(u'gtest_fake', 'gtest_fake_pass.py')
    expected_isolated = {
      u'command': [u'../gtest_fake/gtest_fake_pass.py'],
      u'files': {
        rel_gtest_fake_pass_py: {
          u'mode': 488,
          u'sha-1': unicode(hashlib.sha1(
              open(root_dir_gtest_fake_pass_py, 'rb').read()).hexdigest()),
          u'size': os.stat(root_dir_gtest_fake_pass_py).st_size,
        },
      },
      u'os': unicode(isolate.get_flavor()),
      u'relative_cwd': u'isolate_test_cases',
    }
    self.assertTrue(
        actual_isolated['files'][rel_gtest_fake_pass_py].pop('timestamp'))
    if sys.platform == 'win32':
      expected_isolated['files'][rel_gtest_fake_pass_py].pop('mode')
    self.assertEquals(expected_isolated, actual_isolated)

    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolate_test_cases.py'),
        # Forces 4 parallel jobs.
        '--jobs', '4',
        '--isolated', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    self.assertEqual(0, proc.returncode, (out, err))
    lines = out.splitlines()
    expected_out_re = [
      r'\[1/3\]   \d\.\d\ds .+',
      r'\[2/3\]   \d\.\d\ds .+',
      r'\[3/3\]   \d\.\d\ds .+',
    ]
    self.assertEqual(len(expected_out_re), len(lines), (out, err))
    for index in range(len(expected_out_re)):
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], lines[index]),
          '%d\n%r\n%r\n%r' % (
            index, expected_out_re[index], lines[index], out))
    logging.info(err)
    # Junk is printed on win32.
    if sys.platform != 'win32' and not VERBOSE:
      self.assertEqual('', err)

    test_cases = (
      'Foo.Bar1',
      'Foo.Bar2',
      'Foo.Bar3',
    )
    expected = {
      'conditions': [
        ['OS=="%s"' % isolate.get_flavor(), {
          'variables': {
            'isolate_dependency_untracked': [
              '../gtest_fake/',
            ],
          },
        }],
      ],
    }
    for test_case in test_cases:
      with open(basename + '.' + test_case + '.isolate', 'r') as f:
        result = eval(f.read(), {'__builtins__': None}, None)
        self.assertEqual(expected, result)

    # Now verify the .isolate file was updated! (That's the magical part where
    # you say wow!)
    with open(os.path.join(self.tempdir, gtest_fake_pass_isolate)) as f:
      actual = eval(f.read(), {'__builtins__': None}, None)
    expected = {
      'variables': {
        'command': ['../gtest_fake/gtest_fake_pass.py'],
        'isolate_dependency_tracked': [
          # TODO(maruel): In theory, this file should be listed in the else
          # clause of the condition below but that becomes tricky at that
          # point.
          '../gtest_fake/gtest_fake_pass.py',
        ],
      },
      'conditions': [
        ['OS=="%s"' % isolate.get_flavor(), {
          'variables': {
            'isolate_dependency_untracked': [
              '../gtest_fake/',
            ],
          },
        }],
      ],
    }
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
