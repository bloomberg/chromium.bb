#!/usr/bin/env python
# Copyright 2019 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

# pylint: disable=missing-docstring
# pylint: disable=import-error
# pylint: disable=invalid-name

import os
import sys
import tempfile
import unittest

# Mutates sys.path.
import test_env

# third_party/
from depot_tools import auto_stub

from utils import file_path
from utils import tools

_ACTUALLY_WINDOWS = sys.platform == "win32"


class FindExecutableTest(auto_stub.TestCase):
  """This tests the tools.find_executable function.

  All test cases and helper functions are written so that they can operate (with
  few exceptions) on all platforms that run the test. Tests which test the
  Windows behavior of find_executable must include the string "WIN" in their
  test function name.
  """

  def setUp(self):
    super(FindExecutableTest, self).setUp()
    self.PATH = []
    self.playground = os.path.realpath(tempfile.mkdtemp('tools_test-'))

    self._orig_cwd = os.getcwd()
    os.chdir(self.playground)

    if 'WIN' in self.id():
      self.mock(sys, "platform", "win32")
    else:
      self.mock(sys, "platform", "linux2")

    # initial
    self._touch_exe('SYSTEM', 'python')
    self._touch_exe('SYSTEM', 'python.exe')
    self._add_PATH_abs('SYSTEM')

  def tearDown(self):
    super(FindExecutableTest, self).tearDown()

    # restore cwd
    os.chdir(self._orig_cwd)

    file_path.rmtree(self.playground)

  def _add_PATH_abs(self, *path_toks):
    self.PATH.append(self._abs(*path_toks))

  def _abs(self, *path_toks):
    return os.path.abspath(os.path.join(self.playground, *path_toks))

  def _touch(self, *path_toks):
    full = self._abs(*path_toks)
    d = os.path.dirname(full)
    if not os.path.isdir(d):
      os.makedirs(d)

    with open(full, 'w') as f:
      f.write('hi')

    return full

  def _touch_exe(self, *path_toks):
    full = self._touch(*path_toks)
    if not _ACTUALLY_WINDOWS:
      os.chmod(full, 0777)
    return full

  def _fe(self, cmd):
    return tools.find_executable(cmd, env={'PATH': os.pathsep.join(self.PATH)})

  def test_add_python(self):
    cmd = ['not_a_real_path.py']
    self.assertEqual([self._abs('SYSTEM', 'python')] + cmd, self._fe(cmd))

  def test_WIN_add_python(self):
    cmd = ['not_a_real_path.py']
    self.assertEqual([self._abs('SYSTEM', 'python.exe')] + cmd, self._fe(cmd))

  def test_missing_passthrough(self):
    cmd = ['not_a_real_path']
    self.assertEqual(cmd, self._fe(cmd))

  def test_missing_abs_passthrough(self):
    cmd = [self._abs('not', 'real')]
    self.assertEqual(cmd, self._fe(cmd))

  @unittest.skipIf(sys.platform == 'win32', 'posix only')
  def test_bad_permissions(self):
    cmd = ['config_file']
    os.chmod(self._touch('something', 'dir', 'config_file'), 0)
    os.chmod(self._abs('something', 'dir'), 0)
    self._add_PATH_abs('something', 'dir')
    self.assertEqual(cmd, self._fe(cmd))

  def test_WIN_implicit_extension(self):
    self.assertEqual([self._touch_exe('SYSTEM', 'hello.bat')],
                     self._fe(['hello']))

  def test_implicit_cwd(self):
    self.assertEqual([self._touch_exe('python')], self._fe(['python']))

  def test_WIN_implicit_cwd(self):
    self.assertEqual([self._touch_exe('python.bat')], self._fe(['python']))

  def test_relative_PATH_entry(self):
    self.PATH.insert(0, 'LOCAL')
    self.assertEqual([self._touch_exe('LOCAL', 'thingy')], self._fe(['thingy']))

  def test_skip_empty(self):
    self.PATH.insert(0, '')
    self.assertEqual([self._abs('SYSTEM', 'python')], self._fe(['python']))

  def test_explicit_relative(self):
    self.assertEqual([self._touch_exe('python')], self._fe(['./python']))

  def test_WIN_explicit_relative(self):
    self.assertEqual([self._touch_exe('python.exe')], self._fe(['./python']))

  def test_explicit_relative_missing(self):
    # slash gets flipped on windows because this is not resolved to an absolute
    # path.
    self.assertEqual([os.path.join('.', 'not_real')], self._fe(['./not_real']))


if __name__ == '__main__':
  test_env.main()
