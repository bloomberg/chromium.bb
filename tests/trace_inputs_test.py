#!/usr/bin/env python
# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import StringIO
import logging
import os
import tempfile
import unittest
import shutil
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
    value = trace_inputs.Results.File(None, u'/foo/bar', False, False)
    actual = value.replace_variables({'$FOO': '/foo'})
    self.assertEquals('$FOO/bar', actual.path)
    self.assertEquals('$FOO/bar', actual.full_path)
    self.assertEquals(True, actual.tainted)

  def test_variable_rel(self):
    value = trace_inputs.Results.File(u'/usr', u'foo/bar', False, False)
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

  def test_strace_filename(self):
    filename = u'foo, bar,  ~p#o,,ué^t%t.txt'
    data = 'foo, bar,  ~p#o,,u\\303\\251^t%t.txt'
    self.assertEqual(filename, trace_inputs.Strace.load_filename(data))

  def test_CsvReader(self):
    test_cases = {
      u'   Next is empty, ,  {00000000-0000}':
        [u'Next is empty', u'', u'{00000000-0000}'],

      u'   Foo, , "\\\\NT AUTHORITY\\SYSTEM", "Idle", ""':
        [u'Foo', u'', u'\\\\NT AUTHORITY\\SYSTEM', u'Idle', u''],

      u'   Foo,  ""Who the hell thought delimiters are great as escape too""':
        [u'Foo', u'"Who the hell thought delimiters are great as escape too"'],

      (
        u'  "remoting.exe", ""C:\\Program Files\\remoting.exe" '
        u'--host="C:\\ProgramData\\host.json""'
      ):
        [
          u'remoting.exe',
          u'"C:\\Program Files\\remoting.exe" '
          u'--host="C:\\ProgramData\\host.json"'
        ],

      u'"MONSTRE", "", 0x0': [u'MONSTRE', u'', u'0x0'],

      # To whoever wrote this code at Microsoft: You did it wrong.
      u'"cmd.exe", ""C:\\\\Winz\\\\cmd.exe" /k ""C:\\\\MSVS\\\\vc.bat"" x86"':
        [u'cmd.exe', u'"C:\\\\Winz\\\\cmd.exe" /k "C:\\\\MSVS\\\\vc.bat" x86'],
    }
    for data, expected in test_cases.iteritems():
      csv = trace_inputs.LogmanTrace.Tracer.CsvReader(StringIO.StringIO(data))
      actual = [i for i in csv]
      self.assertEqual(1, len(actual))
      self.assertEqual(expected, actual[0])

  if sys.platform in ('darwin', 'win32'):
    def test_native_case_not_sensitive(self):
      # The home directory is almost guaranteed to have mixed upper/lower case
      # letters on both Windows and OSX.
      # This test also ensures that the output is independent on the input
      # string case.
      path = os.path.expanduser(u'~')
      self.assertTrue(os.path.isdir(path))
      path = path.replace('/', os.path.sep)
      if sys.platform == 'win32':
        # Make sure the drive letter is upper case for consistency.
        path = path[0].upper() + path[1:]
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
      path = os.path.expanduser(os.path.join(u'~', non_existing))
      path = path.replace('/', os.path.sep)
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

  if sys.platform == 'win32':
    def test_native_case_alternate_datastream(self):
      # Create the file manually, since tempfile doesn't support ADS.
      tempdir = unicode(tempfile.mkdtemp(prefix='trace_inputs'))
      try:
        tempdir = trace_inputs.get_native_path_case(tempdir)
        basename = 'foo.txt'
        filename = basename + ':Zone.Identifier'
        filepath = os.path.join(tempdir, filename)
        open(filepath, 'w').close()
        self.assertEqual(filepath, trace_inputs.get_native_path_case(filepath))
        data_suffix = ':$DATA'
        self.assertEqual(
            filepath + data_suffix,
            trace_inputs.get_native_path_case(filepath + data_suffix))

        open(filepath + '$DATA', 'w').close()
        self.assertEqual(
            filepath + data_suffix,
            trace_inputs.get_native_path_case(filepath + data_suffix))
        # Ensure the ADS weren't created as separate file. You love NTFS, don't
        # you?
        self.assertEqual([basename], os.listdir(tempdir))
      finally:
        shutil.rmtree(tempdir)


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
      self._test_lines(lines, u'/home/foo_bar_user/src', files, command)

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
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_close(self):
      lines = [
        (self._ROOT_PID, 'close(7)                          = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

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
          'open("%s", O_RDONLY)       = 76' % os.path.basename(str(FILE_PATH))),
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

    def test_faccess(self):
      lines = [
        (self._ROOT_PID,
         'faccessat(AT_FDCWD, "/home_foo_bar_user/file", W_OK) = 0'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [{'path': '/home_foo_bar_user/file', 'size': 0}],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, ROOT_DIR))

    def test_futex_died(self):
      # That's a pretty bad fork, copy-pasted from a real log.
      lines = [
        (self._ROOT_PID, 'close(9)                                = 0'),
        (self._ROOT_PID, 'futex( <unfinished ... exit status 0>'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, ROOT_DIR))

    def test_futex_missing_in_action(self):
      # That's how futex() calls roll.
      lines = [
        (self._ROOT_PID,
          'clone(child_stack=0x7fae9f4bed70, flags=CLONE_VM|CLONE_FS|'
          'CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|'
          'CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID, '
          'parent_tidptr=0x7fae9f4bf9d0, tls=0x7fae9f4bf700, '
          'child_tidptr=0x7fae9f4bf9d0) = 3862'),
        (self._ROOT_PID,
          'futex(0x1407670, FUTEX_WAIT_PRIVATE, 2, {0, 0}) = -1 EAGAIN '
          '(Resource temporarily unavailable)'),
        (self._ROOT_PID, 'futex(0x1407670, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'close(9)                                = 0'),
        (self._ROOT_PID, 'futex('),
      ]
      expected = {
        'root': {
          'children': [
            {
              'children': [],
              'command': None,
              'executable': None,
              'files': [],
              'initial_cwd': unicode(ROOT_DIR),
              'pid': 3862,
            },
          ],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, ROOT_DIR))

    def test_futex_missing_in_partial_action(self):
      # That's how futex() calls roll even more.
      lines = [
        (self._ROOT_PID,
          'futex(0x7fff25718b14, FUTEX_CMP_REQUEUE_PRIVATE, 1, 2147483647, '
          '0x7fff25718ae8, 2) = 1'),
        (self._ROOT_PID, 'futex(0x7fff25718ae8, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 1, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 3, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 5, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x697263c, FUTEX_WAIT_PRIVATE, 7, NULL) = 0'),
        (self._ROOT_PID, 'futex(0x6972610, FUTEX_WAKE_PRIVATE, 1) = 0'),
        (self._ROOT_PID, 'futex(0x7f0c17780634, '
          'FUTEX_WAIT_BITSET_PRIVATE|FUTEX_CLOCK_REALTIME, 1, '
          '{1351180745, 913067000}, ffffffff'),
      ]
      expected = {
        'root': {
          'children': [],
          'command': None,
          'executable': None,
          'files': [],
          'initial_cwd': unicode(ROOT_DIR),
          'pid': self._ROOT_PID,
        },
      }
      self.assertEquals(expected, self._load_context(lines, ROOT_DIR))

    def test_futex_missing_in_partial_action_with_no_process(self):
      # That's how futex() calls roll even more (again).
      lines = [
          (self._ROOT_PID, 'futex(0x7134840, FUTEX_WAIT_PRIVATE, 2, '
           'NULL <ptrace(SYSCALL):No such process>'),
      ]
      expected = {
         'root': {
           'children': [],
           'command': None,
           'executable': None,
           'files': [],
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
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

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
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_openat(self):
      lines = [
        (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
        (self._ROOT_PID,
         'openat(AT_FDCWD, "/home/foo_bar_user/file", O_RDONLY) = 0'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/file',
          'size': -1,
        },
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
      ]

      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_rmdir(self):
      lines = [
          (self._ROOT_PID, 'rmdir("directory/to/delete") = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_setxattr(self):
      lines = [
          (self._ROOT_PID,
           'setxattr("file.exe", "attribute", "value", 0, 0) = 0'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

    def test_sig_unexpected(self):
      lines = [
        (self._ROOT_PID, 'exit_group(0)                     = ?'),
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', [])

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
      self._test_lines(lines, u'/home/foo_bar_user/src', files)

    def test_truncate(self):
      lines = [
          (self._ROOT_PID,
          'execve("../out/unittests", '
            '["../out/unittests"...], [/* 44 vars */])         = 0'),
          (self._ROOT_PID,
           'truncate("file.exe", 0) = 0'),
      ]
      files = [
        {
          'path': u'/home/foo_bar_user/out/unittests',
          'size': -1,
        },
        {
          'path': u'/home/foo_bar_user/src/file.exe',
          'size': -1,
        },
      ]
      self._test_lines(lines, u'/home/foo_bar_user/src', files)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
