#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import logging
import os
import unittest
import sys

FILE_NAME = os.path.abspath(__file__)
ROOT_DIR = os.path.dirname(FILE_NAME)

import trace_inputs


class TraceInputs(unittest.TestCase):
  def _test(self, value, expected):
    actual = cStringIO.StringIO()
    trace_inputs.pretty_print(value, actual)
    self.assertEquals(expected, actual.getvalue())

  def test_pretty_print_empty(self):
    self._test({}, '{\n}\n')

  def test_pretty_print_mid_size(self):
    value = {
      'variables': {
        'bar': [
          'file1',
          'file2',
        ],
      },
      'conditions': [
        ['OS=\"foo\"', {
          'variables': {
            trace_inputs.KEY_UNTRACKED: [
              'dir1',
              'dir2',
            ],
            trace_inputs.KEY_TRACKED: [
              'file4',
              'file3',
            ],
            'command': ['python', '-c', 'print "H\\i\'"'],
            'read_only': True,
            'relative_cwd': 'isol\'at\\e',
          },
        }],
        ['OS=\"bar\"', {
          'variables': {},
        }, {
          'variables': {},
        }],
      ],
    }
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'bar': [\n"
        "      'file1',\n"
        "      'file2',\n"
        "    ],\n"
        "  },\n"
        "  'conditions': [\n"
        "    ['OS=\"foo\"', {\n"
        "      'variables': {\n"
        "        'command': [\n"
        "          'python',\n"
        "          '-c',\n"
        "          'print \"H\\i\'\"',\n"
        "        ],\n"
        "        'relative_cwd': 'isol\\'at\\\\e',\n"
        "        'read_only': True\n"
        "        'isolate_dependency_tracked': [\n"
        "          'file4',\n"
        "          'file3',\n"
        "        ],\n"
        "        'isolate_dependency_untracked': [\n"
        "          'dir1',\n"
        "          'dir2',\n"
        "        ],\n"
        "      },\n"
        "    }],\n"
        "    ['OS=\"bar\"', {\n"
        "      'variables': {\n"
        "      },\n"
        "    }, {\n"
        "      'variables': {\n"
        "      },\n"
        "    }],\n"
        "  ],\n"
        "}\n")
    self._test(value, expected)


class StraceInputs(unittest.TestCase):
  def _test_lines(self, lines, files, non_existent):
    context = trace_inputs.Strace.Context(lambda _: False)
    for line in lines:
      context.on_line(line)
    self.assertEquals(sorted(files), sorted(context.files))
    self.assertEquals(sorted(non_existent), sorted(context.non_existent))

  def test_empty(self):
    self._test_lines([], [], [])

  def test_close(self):
    lines = [
      '31426 close(7)                          = 0',
    ]
    self._test_lines(lines, [], [])

  def test_clone(self):
    # Grand-child with relative directory.
    lines = [
      '86 chdir("%s") = 0' % ROOT_DIR,
      '86 clone(child_stack=0, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID'
          '|SIGCHLD, child_tidptr=0x7f5350f829d0) = 14',
      ')                                       = ? <unavailable>',
      '14 clone(child_stack=0, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID'
          '|SIGCHLD, child_tidptr=0x7f5350f829d0) = 70',
      '14 close(75)                         = 0',
      '70 open("%s", O_RDONLY)       = 76' % os.path.basename(FILE_NAME),
    ]
    files = [
      FILE_NAME,
    ]
    self._test_lines(lines, files, [])

  def test_open(self):
    lines = [
      '42 chdir("/home/foo_bar_user/src") = 0',
      '42 execve("../out/unittests", '
          '["../out/unittests"...], [/* 44 vars */])         = 0',
      '42 open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 8',
    ]
    files = [
      '/home/foo_bar_user/src/../out/unittests',
      '/home/foo_bar_user/src/out/unittests.log',
    ]
    self._test_lines(lines, [], files)

  def test_open_resumed(self):
    lines = [
      '42 chdir("/home/foo_bar_user/src") = 0',
      '42 execve("../out/unittests", '
          '["../out/unittests"...], [/* 44 vars */])         = 0',
      '42 open("out/unittests.log", O_WRONLY|O_CREAT|O_APPEND <unfinished ...>',
      '42 <... open resumed> )              = 3',
    ]
    files = [
      '/home/foo_bar_user/src/../out/unittests',
      '/home/foo_bar_user/src/out/unittests.log',
    ]
    self._test_lines(lines, [], files)

  def test_sig_unexpected(self):
    lines = [
      '27 exit_group(0)                     = ?',
    ]
    try:
      self._test_lines(lines, [], [])
      self.fail()
    except KeyError, e:
      self.assertEqual(27, e.args[0])


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
