#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import unittest
import sys

BASE_DIR = unicode(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

FILE_PATH = unicode(os.path.abspath(__file__))

import trace_inputs


def join_norm(*args):
  """Joins and normalizes path in a single step."""
  return unicode(os.path.normpath(os.path.join(*args)))


class TraceInputs(unittest.TestCase):
  def test_process_quoted_arguments(self):
    test_cases = (
      ('"foo"', ['foo']),
      ('"foo", "bar"', ['foo', 'bar']),
      ('"foo"..., "bar"', ['foo', 'bar']),
      ('"foo", "bar"...', ['foo', 'bar']),
      (
        '"/browser_tests", "--type=use,comma"',
        ['/browser_tests', '--type=use,comma']
      ),
      (
        '"/browser_tests", "--ignored=\\" --type=renderer \\""',
        ['/browser_tests', '--ignored=" --type=renderer "']
      ),
    )
    for actual, expected in test_cases:
      self.assertEquals(
          expected, trace_inputs.strace_process_quoted_arguments(actual))

  def test_process_escaped_arguments(self):
    test_cases = (
      ('foo\\0', ['foo']),
      ('foo\\001bar\\0', ['foo', 'bar']),
      ('\\"foo\\"\\0', ['"foo"']),
    )
    for actual, expected in test_cases:
      self.assertEquals(
          expected,
          trace_inputs.Dtrace.Context.process_escaped_arguments(actual))

  def test_variable_abs(self):
    value = trace_inputs.Results.File(None, '/foo/bar', False, False)
    actual = value.replace_variables({'$FOO': '/foo'})
    self.assertEquals('$FOO/bar', actual.path)
    self.assertEquals('$FOO/bar', actual.full_path)
    self.assertEquals(True, actual.tainted)

  def test_variable_rel(self):
    value = trace_inputs.Results.File('/usr', 'foo/bar', False, False)
    actual = value.replace_variables({'$FOO': 'foo'})
    self.assertEquals('$FOO/bar', actual.path)
    self.assertEquals(os.path.join('/usr', '$FOO/bar'), actual.full_path)
    self.assertEquals(True, actual.tainted)

  def test_native_case_end_with_os_path_sep(self):
    # Make sure the trailing os.path.sep is kept.
    path = trace_inputs.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEquals(trace_inputs.get_native_path_case(path), path)

  def test_native_case_non_existing(self):
    # Make sure it doesn't throw on non-existing files.
    non_existing = 'trace_input_test_this_file_should_not_exist'
    path = os.path.expanduser('~/' + non_existing)
    self.assertFalse(os.path.exists(path))
    path = trace_inputs.get_native_path_case(ROOT_DIR) + os.path.sep
    self.assertEquals(trace_inputs.get_native_path_case(path), path)

  if sys.platform in ('darwin', 'win32'):
    def test_native_case_not_sensitive(self):
      # The home directory is almost guaranteed to have mixed upper/lower case
      # letters on both Windows and OSX.
      # This test also ensures that the output is independent on the input
      # string case.
      path = os.path.expanduser('~')
      self.assertTrue(os.path.isdir(path))
      # This test assumes the variable is in the native path case on disk, this
      # should be the case. Verify this assumption:
      self.assertEquals(path, trace_inputs.get_native_path_case(path))
      self.assertEquals(
          trace_inputs.get_native_path_case(path.lower()),
          trace_inputs.get_native_path_case(path.upper()))

    def test_native_case_not_sensitive_non_existent(self):
      # This test also ensures that the output is independent on the input
      # string case.
      non_existing = os.path.join(
          'trace_input_test_this_dir_should_not_exist', 'really not', '')
      path = os.path.expanduser(os.path.join('~', non_existing))
      self.assertFalse(os.path.exists(path))
      lower = trace_inputs.get_native_path_case(path.lower())
      upper = trace_inputs.get_native_path_case(path.upper())
      # Make sure non-existing element is not modified:
      self.assertTrue(lower.endswith(non_existing.lower()))
      self.assertTrue(upper.endswith(non_existing.upper()))
      self.assertEquals(lower[:-len(non_existing)], upper[:-len(non_existing)])

  if sys.platform != 'win32':
    def test_symlink(self):
      # This test will fail if the checkout is in a symlink.
      actual = trace_inputs.split_at_symlink(None, ROOT_DIR)
      expected = (ROOT_DIR, None, None)
      self.assertEquals(expected, actual)

      actual = trace_inputs.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), None, None)
      self.assertEquals(expected, actual)

      actual = trace_inputs.split_at_symlink(
          None, os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      expected = (
          os.path.join(BASE_DIR, 'trace_inputs'), 'files2', '')
      self.assertEquals(expected, actual)

      actual = trace_inputs.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '')
      self.assertEquals(expected, actual)
      actual = trace_inputs.split_at_symlink(
          ROOT_DIR, os.path.join('tests', 'trace_inputs', 'files2', 'bar'))
      expected = (
          os.path.join('tests', 'trace_inputs'), 'files2', '/bar')
      self.assertEquals(expected, actual)

    def test_native_case_symlink_right_case(self):
      actual = trace_inputs.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs'))
      self.assertEquals('trace_inputs', os.path.basename(actual))

      # Make sure the symlink is not resolved.
      actual = trace_inputs.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs', 'files2'))
      self.assertEquals('files2', os.path.basename(actual))

  if sys.platform == 'darwin':
    def test_native_case_symlink_wrong_case(self):
      actual = trace_inputs.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs'))
      self.assertEquals('trace_inputs', os.path.basename(actual))

      # Make sure the symlink is not resolved.
      actual = trace_inputs.get_native_path_case(
          os.path.join(BASE_DIR, 'trace_inputs', 'Files2'))
      self.assertEquals('files2', os.path.basename(actual))


if sys.platform != 'win32':
  class StraceInputs(unittest.TestCase):
    # Represents the root process pid (an arbitrary number).
    _ROOT_PID = 27
    _CHILD_PID = 14
    _GRAND_CHILD_PID = 70

    @staticmethod
    def _load_context(lines, initial_cwd):
      context = trace_inputs.Strace.Context(lambda _: False, initial_cwd)
      for line in lines:
        context.on_line(*line)
      return context.to_results().flatten()

    def _test_lines(self, lines, initial_cwd, files, command=None):
      filepath = join_norm(initial_cwd, '../out/unittests')
      command = command or ['../out/unittests']
      expected = {
        'root': {
          'children': [],
          'command': command,
          'executable': filepath,
          'files': files,
          'initial_cwd': initial_cwd,
          'pid': self._ROOT_PID,
        }
      }
      if not files:
        expected['root']['command'] = None
        expected['root']['executable'] = None
      self.assertEquals(expected, self._load_context(lines, initial_cwd))

    def test_execve(self):
      lines = [
        (self._ROOT_PID,
          'execve("/home/foo_bar_user/out/unittests", '
            '["/home/foo_bar_user/out/unittests", '
            '"--gtest_filter=AtExitTest.Basic"], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 8'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      command = [
        '/home/foo_bar_user/out/unittests', '--gtest_filter=AtExitTest.Basic',
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', files, command)

    def test_empty(self):
      try:
        self._load_context([], None)
        self.fail()
      except trace_inputs.TracingFailure, e:
        expected = (
          'Found internal inconsitency in process lifetime detection '
          'while finding the root process',
          None,
          None,
          None,
          [])
        self.assertEquals(expected, e.args)

    def test_chmod(self):
      lines = [
          (self._ROOT_PID, 'chmod("temp/file", 0100644) = 0'),
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', [])

    def test_close(self):
      lines = [
        (self._ROOT_PID, 'close(7)                          = 0'),
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', [])

    def test_clone(self):
      # Grand-child with relative directory.
      lines = [
        (self._ROOT_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._CHILD_PID),
        (self._CHILD_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._GRAND_CHILD_PID),
        (self._GRAND_CHILD_PID,
          'open("%s", O_RDONLY)       = 76' % os.path.basename(FILE_PATH)),
      ]
      size = os.stat(FILE_PATH).st_size
      expected = {
        'root': {
          'children': [
            {
              'children': [
                {
                  'children': [],
                  'command': None,
                  'executable': None,
                  'files': [
                    {
                      'path': FILE_PATH,
                      'size': size,
                    },
                  ],
                  'initial_cwd': BASE_DIR,
                  'pid': self._GRAND_CHILD_PID,
                },
              ],
              'command': None,
              'executable': None,
              'files': [],
              'initial_cwd': BASE_DIR,
              'pid': self._CHILD_PID,
            },
          ],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': BASE_DIR,
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, BASE_DIR))

    def test_clone_chdir(self):
      # Grand-child with relative directory.
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._CHILD_PID),
        (self._CHILD_PID,
          'chdir("/home_foo_bar_user/path1") = 0'),
        (self._CHILD_PID,
          'clone(child_stack=0, flags=CLONE_CHILD_CLEARTID'
            '|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f5350f829d0) = %d' %
            self._GRAND_CHILD_PID),
        (self._GRAND_CHILD_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID, 'chdir("/home_foo_bar_user/path2") = 0'),
        (self._GRAND_CHILD_PID,
          'open("random.txt", O_RDONLY)       = 76'),
      ]
      expected = {
        'root': {
          'children': [
            {
              'children': [
                {
                  'children': [],
                  'command': ['../out/unittests'],
                  'executable': '/home_foo_bar_user/out/unittests',
                  'files': [
                    {
                      'path': u'/home_foo_bar_user/out/unittests',
                      'size': -1,
                    },
                    {
                      'path': u'/home_foo_bar_user/path1/random.txt',
                      'size': -1,
                    },
                  ],
                  'initial_cwd': u'/home_foo_bar_user/path1',
                  'pid': self._GRAND_CHILD_PID,
                },
              ],
              # clone does not carry over the command and executable so it is
              # clear if an execve() call was done or not.
              'command': None,
              'executable': None,
              # This is important, since no execve call was done, it didn't
              # touch the executable file.
              'files': [],
              'initial_cwd': unicode(ROOT_DIR),
              'pid': self._CHILD_PID,
            },
          ],
          'command': ['../out/unittests'],
          'executable': join_norm(ROOT_DIR, '../out/unittests'),
          'files': [
            {
              'path': join_norm(ROOT_DIR, '../out/unittests'),
              'size': -1,
            },
          ],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, ROOT_DIR))

    def test_open(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 8'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', files)

    def test_open_resumed(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          'open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND '
            '<unfinished ...>'),
        (self._ROOT_PID, '<... open resumed> )              = 3'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'path': u'/home/foo_bar_user/src/out/unittests.log',
          'size': -1,
        },
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', files)

    def test_rmdir(self):
      lines = [
          (self._ROOT_PID, 'rmdir("directory/to/delete") = 0'),
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', [])

    def test_setxattr(self):
      lines = [
          (self._ROOT_PID,
           'setxattr("file.exe", "attribute", "value", 0, 0) = 0'),
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', [])

    def test_sig_unexpected(self):
      lines = [
        (self._ROOT_PID, 'exit_group(0)                     = ?'),
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', [])

    def test_stray(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
          ')                                       = ? <unavailable>'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
      ]
      self._test_lines(lines, '/home/foo_bar_user/src', files)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
