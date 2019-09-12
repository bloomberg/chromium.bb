#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for presubmit_support.py and presubmit_canned_checks.py."""

# pylint: disable=no-member,E1103

import StringIO
import functools
import itertools
import logging
import multiprocessing
import os
import random
import re
import sys
import tempfile
import time
import unittest
import urllib2

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _ROOT)

from testing_support.test_case_utils import TestCaseUtils
from third_party import mock

import auth
import gclient_utils
import git_cl
import git_common as git
import json
import owners
import owners_finder
import presubmit_support as presubmit
import scm
import subprocess2 as subprocess

# Shortcut.
presubmit_canned_checks = presubmit.presubmit_canned_checks


# Access to a protected member XXX of a client class
# pylint: disable=protected-access


class MockTemporaryFile(object):
  """Simple mock for files returned by tempfile.NamedTemporaryFile()."""
  def __init__(self, name):
    self.name = name

  def __enter__(self):
    return self

  def __exit__(self, *args):
    pass


class PresubmitTestsBase(TestCaseUtils, unittest.TestCase):
  """Sets up and tears down the mocks but doesn't test anything as-is."""
  presubmit_text = """
def CheckChangeOnUpload(input_api, output_api):
  if input_api.change.tags.get('ERROR'):
    return [output_api.PresubmitError("!!")]
  if input_api.change.tags.get('PROMPT_WARNING'):
    return [output_api.PresubmitPromptWarning("??")]
  else:
    return ()
"""

  presubmit_trymaster = """
def GetPreferredTryMasters(project, change):
  return %s
"""

  presubmit_diffs = """
diff --git %(filename)s %(filename)s
index fe3de7b..54ae6e1 100755
--- %(filename)s       2011-02-09 10:38:16.517224845 -0800
+++ %(filename)s       2011-02-09 10:38:53.177226516 -0800
@@ -1,6 +1,5 @@
 this is line number 0
 this is line number 1
-this is line number 2 to be deleted
 this is line number 3
 this is line number 4
 this is line number 5
@@ -8,7 +7,7 @@
 this is line number 7
 this is line number 8
 this is line number 9
-this is line number 10 to be modified
+this is line number 10
 this is line number 11
 this is line number 12
 this is line number 13
@@ -21,9 +20,8 @@
 this is line number 20
 this is line number 21
 this is line number 22
-this is line number 23
-this is line number 24
-this is line number 25
+this is line number 23.1
+this is line number 25.1
 this is line number 26
 this is line number 27
 this is line number 28
@@ -31,6 +29,7 @@
 this is line number 30
 this is line number 31
 this is line number 32
+this is line number 32.1
 this is line number 33
 this is line number 34
 this is line number 35
@@ -38,14 +37,14 @@
 this is line number 37
 this is line number 38
 this is line number 39
-
 this is line number 40
-this is line number 41
+this is line number 41.1
 this is line number 42
 this is line number 43
 this is line number 44
 this is line number 45
+
 this is line number 46
 this is line number 47
-this is line number 48
+this is line number 48.1
 this is line number 49
"""

  def setUp(self):
    super(PresubmitTestsBase, self).setUp()
    class FakeChange(object):
      def __init__(self, obj):
        self._root = obj.fake_root_dir
      def RepositoryRoot(self):
        return self._root

    presubmit._ASKED_FOR_FEEDBACK = False
    self.fake_root_dir = self.RootDir()
    self.fake_change = FakeChange(self)

    mock.patch('gclient_utils.FileRead').start()
    mock.patch('gclient_utils.FileWrite').start()
    mock.patch('json.load').start()
    mock.patch('os.path.abspath', lambda f: f).start()
    mock.patch('os.getcwd', self.RootDir)
    mock.patch('os.chdir').start()
    mock.patch('os.listdir').start()
    mock.patch('os.path.isfile').start()
    mock.patch('os.remove').start()
    mock.patch('random.randint').start()
    mock.patch('presubmit_support.warn').start()
    mock.patch('presubmit_support.sigint_handler').start()
    mock.patch('scm.determine_scm').start()
    mock.patch('scm.GIT.GenerateDiff').start()
    mock.patch('subprocess2.Popen').start()
    mock.patch('sys.stderr', StringIO.StringIO()).start()
    mock.patch('sys.stdout', StringIO.StringIO()).start()
    mock.patch('tempfile.NamedTemporaryFile').start()
    mock.patch('multiprocessing.cpu_count', lambda: 2)
    mock.patch('urllib2.urlopen').start()
    self.addCleanup(mock.patch.stopall)

  def checkstdout(self, value):
    self.assertEqual(sys.stdout.getvalue(), value)


class PresubmitUnittest(PresubmitTestsBase):
  """General presubmit_support.py tests (excluding InputApi and OutputApi)."""

  _INHERIT_SETTINGS = 'inherit-review-settings-ok'
  fake_root_dir = '/foo/bar'

  def testCannedCheckFilter(self):
    canned = presubmit.presubmit_canned_checks
    orig = canned.CheckOwners
    with presubmit.canned_check_filter(['CheckOwners']):
      self.assertNotEqual(canned.CheckOwners, orig)
      self.assertEqual(canned.CheckOwners(None, None), [])
    self.assertEqual(canned.CheckOwners, orig)

  def testListRelevantPresubmitFiles(self):
    files = [
        'blat.cc',
        os.path.join('foo', 'haspresubmit', 'yodle', 'smart.h'),
        os.path.join('moo', 'mat', 'gat', 'yo.h'),
        os.path.join('foo', 'luck.h'),
    ]

    known_files = [
        os.path.join(self.fake_root_dir, 'PRESUBMIT.py'),
        os.path.join(self.fake_root_dir, 'foo', 'haspresubmit', 'PRESUBMIT.py'),
        os.path.join(
            self.fake_root_dir, 'foo', 'haspresubmit', 'yodle', 'PRESUBMIT.py'),
    ]
    os.path.isfile.side_effect = lambda f: f in known_files

    dirs_with_presubmit = [
        self.fake_root_dir,
        os.path.join(self.fake_root_dir, 'foo', 'haspresubmit'),
        os.path.join(self.fake_root_dir, 'foo', 'haspresubmit', 'yodle'),
    ]
    os.listdir.side_effect = (
        lambda d: ['PRESUBMIT.py'] if d in dirs_with_presubmit else [])

    presubmit_files = presubmit.ListRelevantPresubmitFiles(
        files, self.fake_root_dir)
    self.assertEqual(presubmit_files, known_files)

  def testListUserPresubmitFiles(self):
    files = ['blat.cc',]

    os.path.isfile.side_effect = lambda f: 'PRESUBMIT' in f
    os.listdir.return_value = [
        'PRESUBMIT.py', 'PRESUBMIT_test.py', 'PRESUBMIT-user.py']

    presubmit_files = presubmit.ListRelevantPresubmitFiles(
        files, self.fake_root_dir)
    self.assertEqual(presubmit_files, [
        os.path.join(self.fake_root_dir, 'PRESUBMIT.py'),
        os.path.join(self.fake_root_dir, 'PRESUBMIT-user.py'),
    ])

  def testListRelevantPresubmitFilesInheritSettings(self):
    sys_root_dir = self._OS_SEP
    root_dir = os.path.join(sys_root_dir, 'foo', 'bar')
    inherit_path = os.path.join(root_dir, self._INHERIT_SETTINGS)
    files = [
        'test.cc',
        os.path.join('moo', 'test2.cc'),
        os.path.join('zoo', 'test3.cc')
    ]

    known_files = [
        inherit_path,
        os.path.join(sys_root_dir, 'foo', 'PRESUBMIT.py'),
        os.path.join(sys_root_dir, 'foo', 'bar', 'moo', 'PRESUBMIT.py'),
    ]
    os.path.isfile.side_effect = lambda f: f in known_files

    dirs_with_presubmit = [
        os.path.join(sys_root_dir, 'foo'),
        os.path.join(sys_root_dir, 'foo', 'bar','moo'),
    ]
    os.listdir.side_effect = (
        lambda d: ['PRESUBMIT.py'] if d in dirs_with_presubmit else [])

    presubmit_files = presubmit.ListRelevantPresubmitFiles(files, root_dir)
    self.assertEqual(presubmit_files, [
        os.path.join(sys_root_dir, 'foo', 'PRESUBMIT.py'),
        os.path.join(sys_root_dir, 'foo', 'bar', 'moo', 'PRESUBMIT.py')
    ])

  def testTagLineRe(self):
    m = presubmit.Change.TAG_LINE_RE.match(' BUG =1223, 1445  \t')
    self.assertIsNotNone(m)
    self.assertEqual(m.group('key'), 'BUG')
    self.assertEqual(m.group('value'), '1223, 1445')

  def testGitChange(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123',
                         'and some more regular text  \t')
    unified_diff = [
        'diff --git binary_a.png binary_a.png',
        'new file mode 100644',
        'index 0000000..6fbdd6d',
        'Binary files /dev/null and binary_a.png differ',
        'diff --git binary_d.png binary_d.png',
        'deleted file mode 100644',
        'index 6fbdd6d..0000000',
        'Binary files binary_d.png and /dev/null differ',
        'diff --git binary_md.png binary_md.png',
        'index 6fbdd6..be3d5d8 100644',
        'GIT binary patch',
        'delta 109',
        'zcmeyihjs5>)(Opwi4&WXB~yyi6N|G`(i5|?i<2_a@)OH5N{Um`D-<SM@g!_^W9;SR',
        'zO9b*W5{pxTM0slZ=F42indK9U^MTyVQlJ2s%1BMmEKMv1Q^gtS&9nHn&*Ede;|~CU',
        'CMJxLN',
        '',
        'delta 34',
        'scmV+-0Nww+y#@BX1(1W0gkzIp3}CZh0gVZ>`wGVcgW(Rh;SK@ZPa9GXlK=n!',
        '',
        'diff --git binary_m.png binary_m.png',
        'index 6fbdd6d..be3d5d8 100644',
        'Binary files binary_m.png and binary_m.png differ',
        'diff --git boo/blat.cc boo/blat.cc',
        'new file mode 100644',
        'index 0000000..37d18ad',
        '--- boo/blat.cc',
        '+++ boo/blat.cc',
        '@@ -0,0 +1,5 @@',
        '+This is some text',
        '+which lacks a copyright warning',
        '+but it is nonetheless interesting',
        '+and worthy of your attention.',
        '+Its freshness factor is through the roof.',
        'diff --git floo/delburt.cc floo/delburt.cc',
        'deleted file mode 100644',
        'index e06377a..0000000',
        '--- floo/delburt.cc',
        '+++ /dev/null',
        '@@ -1,14 +0,0 @@',
        '-This text used to be here',
        '-but someone, probably you,',
        '-having consumed the text',
        '-  (absorbed its meaning)',
        '-decided that it should be made to not exist',
        '-that others would not read it.',
        '-  (What happened here?',
        '-was the author incompetent?',
        '-or is the world today so different from the world',
        '-   the author foresaw',
        '-and past imaginination',
        '-   amounts to rubble, insignificant,',
        '-something to be tripped over',
        '-and frustrated by)',
        'diff --git foo/TestExpectations foo/TestExpectations',
        'index c6e12ab..d1c5f23 100644',
        '--- foo/TestExpectations',
        '+++ foo/TestExpectations',
        '@@ -1,12 +1,24 @@',
        '-Stranger, behold:',
        '+Strange to behold:',
        '   This is a text',
        ' Its contents existed before.',
        '',
        '-It is written:',
        '+Weasel words suggest:',
        '   its contents shall exist after',
        '   and its contents',
        ' with the progress of time',
        ' will evolve,',
        '-   snaillike,',
        '+   erratically,',
        ' into still different texts',
        '-from this.',
        '\ No newline at end of file',
        '+from this.',
        '+',
        '+For the most part,',
        '+I really think unified diffs',
        '+are elegant: the way you can type',
        '+diff --git inside/a/text inside/a/text',
        '+or something silly like',
        '+@@ -278,6 +278,10 @@',
        '+and have this not be interpreted',
        '+as the start of a new file',
        '+or anything messed up like that,',
        '+because you parsed the header',
        '+correctly.',
        '\ No newline at end of file',
            '']
    files = [('A      ', 'binary_a.png'),
             ('D      ', 'binary_d.png'),
             ('M      ', 'binary_m.png'),
             ('M      ', 'binary_md.png'),  # Binary w/ diff
             ('A      ', 'boo/blat.cc'),
             ('D      ', 'floo/delburt.cc'),
             ('M      ', 'foo/TestExpectations')]

    known_files = [
        os.path.join(self.fake_root_dir, *path.split('/'))
        for op, path in files if not op.startswith('D')]
    os.path.isfile.side_effect = lambda f: f in known_files

    scm.GIT.GenerateDiff.return_value = '\n'.join(unified_diff)

    change = presubmit.GitChange(
        'mychange',
        '\n'.join(description_lines),
        self.fake_root_dir,
        files,
        0,
        0,
        None,
        upstream=None)
    self.assertIsNotNone(change.Name() == 'mychange')
    self.assertIsNotNone(change.DescriptionText() ==
                    'Hello there\nthis is a change\nand some more regular text')
    self.assertIsNotNone(change.FullDescriptionText() ==
                    '\n'.join(description_lines))

    self.assertIsNotNone(change.BugsFromDescription() == ['123'])

    self.assertIsNotNone(len(change.AffectedFiles()) == 7)
    self.assertIsNotNone(len(change.AffectedFiles()) == 7)
    self.assertIsNotNone(len(change.AffectedFiles(include_deletes=False)) == 5)
    self.assertIsNotNone(len(change.AffectedFiles(include_deletes=False)) == 5)

    # Note that on git, there's no distinction between binary files and text
    # files; everything that's not a delete is a text file.
    affected_text_files = change.AffectedTestableFiles()
    self.assertIsNotNone(len(affected_text_files) == 5)

    local_paths = change.LocalPaths()
    expected_paths = [os.path.normpath(f) for op, f in files]
    self.assertEqual(local_paths, expected_paths)

    actual_rhs_lines = []
    for f, linenum, line in change.RightHandSideLines():
      actual_rhs_lines.append((f.LocalPath(), linenum, line))

    f_blat = os.path.normpath('boo/blat.cc')
    f_test_expectations = os.path.normpath('foo/TestExpectations')
    expected_rhs_lines = [
        (f_blat, 1, 'This is some text'),
        (f_blat, 2, 'which lacks a copyright warning'),
        (f_blat, 3, 'but it is nonetheless interesting'),
        (f_blat, 4, 'and worthy of your attention.'),
        (f_blat, 5, 'Its freshness factor is through the roof.'),
        (f_test_expectations, 1, 'Strange to behold:'),
        (f_test_expectations, 5, 'Weasel words suggest:'),
        (f_test_expectations, 10, '   erratically,'),
        (f_test_expectations, 13, 'from this.'),
        (f_test_expectations, 14, ''),
        (f_test_expectations, 15, 'For the most part,'),
        (f_test_expectations, 16, 'I really think unified diffs'),
        (f_test_expectations, 17, 'are elegant: the way you can type'),
        (f_test_expectations, 18, 'diff --git inside/a/text inside/a/text'),
        (f_test_expectations, 19, 'or something silly like'),
        (f_test_expectations, 20, '@@ -278,6 +278,10 @@'),
        (f_test_expectations, 21, 'and have this not be interpreted'),
        (f_test_expectations, 22, 'as the start of a new file'),
        (f_test_expectations, 23, 'or anything messed up like that,'),
        (f_test_expectations, 24, 'because you parsed the header'),
        (f_test_expectations, 25, 'correctly.')]

    self.assertEqual(expected_rhs_lines, actual_rhs_lines)

  def testInvalidChange(self):
    with self.assertRaises(AssertionError):
      presubmit.GitChange(
          'mychange',
          'description',
          self.fake_root_dir,
          ['foo/blat.cc', 'bar'],
          0,
          0,
          None)

  def testExecPresubmitScript(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123')
    files = [
      ['A', 'foo\\blat.cc'],
    ]
    fake_presubmit = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')

    change = presubmit.Change(
        'mychange',
        '\n'.join(description_lines),
        self.fake_root_dir,
        files,
        0,
        0,
        None)
    executer = presubmit.PresubmitExecuter(change, False, None, False)
    self.assertFalse(executer.ExecPresubmitScript('', fake_presubmit))
    # No error if no on-upload entry point
    self.assertFalse(executer.ExecPresubmitScript(
      ('def CheckChangeOnCommit(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      fake_presubmit
    ))

    executer = presubmit.PresubmitExecuter(change, True, None, False)
    # No error if no on-commit entry point
    self.assertFalse(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      fake_presubmit
    ))

    self.assertFalse(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  if not input_api.change.BugsFromDescription():\n'
       '    return (output_api.PresubmitError("!!"))\n'
       '  else:\n'
       '    return ()'),
      fake_presubmit
    ))

    self.assertRaises(presubmit.PresubmitFailure,
      executer.ExecPresubmitScript,
      'def CheckChangeOnCommit(input_api, output_api):\n'
      '  return "foo"',
      fake_presubmit)

    self.assertRaises(presubmit.PresubmitFailure,
      executer.ExecPresubmitScript,
      'def CheckChangeOnCommit(input_api, output_api):\n'
      '  return ["foo"]',
      fake_presubmit)

  def testExecPresubmitScriptTemporaryFilesRemoval(self):
    tempfile.NamedTemporaryFile.side_effect = [
        MockTemporaryFile('baz'),
        MockTemporaryFile('quux'),
    ]

    fake_presubmit = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    executer = presubmit.PresubmitExecuter(
        self.fake_change, False, None, False)

    self.assertEqual((), executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  if len(input_api._named_temporary_files):\n'
       '    return (output_api.PresubmitError("!!"),)\n'
       '  return ()\n'),
      fake_presubmit
    ))

    result = executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  with input_api.CreateTemporaryFile():\n'
       '    pass\n'
       '  with input_api.CreateTemporaryFile():\n'
       '    pass\n'
       '  return [output_api.PresubmitResult(None, f)\n'
       '          for f in input_api._named_temporary_files]\n'),
      fake_presubmit
    )
    self.assertEqual(['baz', 'quux'], [r._items for r in result])

    self.assertEqual(
        os.remove.mock_calls, [mock.call('baz'), mock.call('quux')])

  def testDoPresubmitChecksNoWarningsOrErrors(self):
    haspresubmit_path = os.path.join(
        self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')
    root_path = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')

    os.path.isfile.side_effect = lambda f: f in [root_path, haspresubmit_path]
    os.listdir.return_value = ['PRESUBMIT.py']

    gclient_utils.FileRead.return_value = self.presubmit_text

    # Make a change which will have no warnings.
    change = self.ExampleChange(extra_lines=['STORY=http://tracker/123'])

    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=None,
        default_presubmit=None, may_prompt=False,
        gerrit_obj=None, json_output=None)
    self.assertIsNotNone(output.should_continue())
    self.assertEqual(output.getvalue().count('!!'), 0)
    self.assertEqual(output.getvalue().count('??'), 0)
    self.assertEqual(output.getvalue().count(
        'Running presubmit upload checks ...\n'), 1)

  def testDoPresubmitChecksJsonOutput(self):
    fake_error = 'Missing LGTM'
    fake_error_items = '["!", "!!", "!!!"]'
    fake_error_long_text = "Error long text..."
    fake_error2 = 'This failed was found in file fake.py'
    fake_error2_items = '["!!!", "!!", "!"]'
    fake_error2_long_text = " Error long text" * 3
    fake_warning = 'Line 88 is more than 80 characters.'
    fake_warning_items = '["W", "w"]'
    fake_warning_long_text = 'Warning long text...'
    fake_notify = 'This is a dry run'
    fake_notify_items = '["N"]'
    fake_notify_long_text = 'Notification long text...'
    always_fail_presubmit_script = """
def CheckChangeOnUpload(input_api, output_api):
  return [
    output_api.PresubmitError("%s",%s, "%s"),
    output_api.PresubmitError("%s",%s, "%s"),
    output_api.PresubmitPromptWarning("%s",%s, "%s"),
    output_api.PresubmitNotifyResult("%s",%s, "%s")
  ]
def CheckChangeOnCommit(input_api, output_api):
  raise Exception("Test error")
""" % (fake_error, fake_error_items, fake_error_long_text,
       fake_error2, fake_error2_items, fake_error2_long_text,
       fake_warning, fake_warning_items, fake_warning_long_text,
       fake_notify, fake_notify_items, fake_notify_long_text
       )

    os.path.isfile.return_value = False
    os.listdir.side_effect = [[], ['PRESUBMIT.py']]
    random.randint.return_value = 0

    change = self.ExampleChange(extra_lines=['ERROR=yes'])

    temp_path = 'temp.json'

    fake_result = {
        'notifications': [
          {
            'message': fake_notify,
            'items': json.loads(fake_notify_items),
            'fatal': False,
            'long_text': fake_notify_long_text
          }
        ],
        'errors': [
          {
            'message': fake_error,
            'items': json.loads(fake_error_items),
            'fatal': True,
            'long_text': fake_error_long_text
          },
          {
            'message': fake_error2,
            'items': json.loads(fake_error2_items),
            'fatal': True,
            'long_text': fake_error2_long_text
          }
        ],
        'warnings': [
          {
            'message': fake_warning,
            'items': json.loads(fake_warning_items),
            'fatal': False,
            'long_text': fake_warning_long_text
          }
        ]
    }

    fake_result_json = json.dumps(fake_result)

    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=None,
        default_presubmit=always_fail_presubmit_script,
        may_prompt=False, gerrit_obj=None, json_output=temp_path)

    self.assertFalse(output.should_continue())
    gclient_utils.FileWrite.assert_called_with(temp_path, fake_result_json)

  def testDoPresubmitChecksPromptsAfterWarnings(self):
    presubmit_path = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    haspresubmit_path = os.path.join(
        self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')

    os.path.isfile.side_effect = (
        lambda f: f in [presubmit_path, haspresubmit_path])
    os.listdir.return_value = ['PRESUBMIT.py']
    random.randint.return_value = 1

    gclient_utils.FileRead.return_value = self.presubmit_text

    # Make a change with a single warning.
    change = self.ExampleChange(extra_lines=['PROMPT_WARNING=yes'])

    input_buf = StringIO.StringIO('n\n')  # say no to the warning
    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=input_buf,
        default_presubmit=None, may_prompt=True,
        gerrit_obj=None, json_output=None)
    self.assertFalse(output.should_continue())
    self.assertEqual(output.getvalue().count('??'), 2)

    input_buf = StringIO.StringIO('y\n')  # say yes to the warning
    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=input_buf,
        default_presubmit=None, may_prompt=True,
        gerrit_obj=None, json_output=None)
    self.assertIsNotNone(output.should_continue())
    self.assertEqual(output.getvalue().count('??'), 2)
    self.assertEqual(output.getvalue().count(
        'Running presubmit upload checks ...\n'), 1)

  def testDoPresubmitChecksWithWarningsAndNoPrompt(self):
    presubmit_path = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    haspresubmit_path = os.path.join(
        self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')

    os.path.isfile.side_effect = (
        lambda f: f in [presubmit_path, haspresubmit_path])
    os.listdir.return_value = ['PRESUBMIT.py']
    gclient_utils.FileRead.return_value = self.presubmit_text
    random.randint.return_value = 1

    change = self.ExampleChange(extra_lines=['PROMPT_WARNING=yes'])

    # There is no input buffer and may_prompt is set to False.
    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=None,
        default_presubmit=None, may_prompt=False,
        gerrit_obj=None, json_output=None)
    # A warning is printed, and should_continue is True.
    self.assertIsNotNone(output.should_continue())
    self.assertEqual(output.getvalue().count('??'), 2)
    self.assertEqual(output.getvalue().count('(y/N)'), 0)
    self.assertEqual(output.getvalue().count(
        'Running presubmit upload checks ...\n'), 1)

  def testDoPresubmitChecksNoWarningPromptIfErrors(self):
    presubmit_path = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    haspresubmit_path = os.path.join(
        self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')

    os.path.isfile.side_effect = (
        lambda f: f in [presubmit_path, haspresubmit_path])
    os.listdir.return_value = ['PRESUBMIT.py']
    gclient_utils.FileRead.return_value = self.presubmit_text
    random.randint.return_value = 1

    change = self.ExampleChange(extra_lines=['ERROR=yes'])
    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=None,
        default_presubmit=None, may_prompt=True,
        gerrit_obj=None, json_output=None)
    self.assertFalse(output.should_continue())
    self.assertEqual(output.getvalue().count('??'), 0)
    self.assertEqual(output.getvalue().count('!!'), 2)
    self.assertEqual(output.getvalue().count('(y/N)'), 0)
    self.assertEqual(output.getvalue().count(
        'Running presubmit upload checks ...\n'), 1)

  def testDoDefaultPresubmitChecksAndFeedback(self):
    always_fail_presubmit_script = """
def CheckChangeOnUpload(input_api, output_api):
  return [output_api.PresubmitError("!!")]
def CheckChangeOnCommit(input_api, output_api):
  raise Exception("Test error")
"""

    os.path.isfile.return_value = False
    os.listdir.side_effect = (
        lambda d: [] if d == self.fake_root_dir else ['PRESUBMIT.py'])
    random.randint.return_value = 0

    input_buf = StringIO.StringIO('y\n')

    change = self.ExampleChange(extra_lines=['STORY=http://tracker/123'])
    output = presubmit.DoPresubmitChecks(
        change=change, committing=False, verbose=True,
        output_stream=None, input_stream=input_buf,
        default_presubmit=always_fail_presubmit_script,
        may_prompt=False, gerrit_obj=None, json_output=None)
    self.assertFalse(output.should_continue())
    text = (
        'Running presubmit upload checks ...\n'
        'Warning, no PRESUBMIT.py found.\n'
        'Running default presubmit script.\n'
        '\n'
        '** Presubmit ERRORS **\n!!\n\n'
        'Was the presubmit check useful? If not, run "git cl presubmit -v"\n'
        'to figure out which PRESUBMIT.py was run, then run git blame\n'
        'on the file to figure out who to ask for help.\n')
    self.assertEqual(output.getvalue(), text)

  def testGetTryMastersExecuter(self):
    change = self.ExampleChange(
        extra_lines=['STORY=http://tracker.com/42', 'BUG=boo\n'])
    executer = presubmit.GetTryMastersExecuter()
    self.assertEqual({}, executer.ExecPresubmitScript('', '', '', change))
    self.assertEqual({},
        executer.ExecPresubmitScript('def foo():\n  return\n', '', '', change))

    expected_result = {'m1': {'s1': set(['t1', 't2'])},
                       'm2': {'s1': set(['defaulttests']),
                              's2': set(['defaulttests'])}}
    empty_result1 = {}
    empty_result2 = {'m': {}}
    space_in_name_result = {'m r': {'s\tv': set(['t1'])}}
    for result in (
        expected_result, empty_result1, empty_result2, space_in_name_result):
      self.assertEqual(
          result,
          executer.ExecPresubmitScript(
              self.presubmit_trymaster % result, '', '', change))

  def ExampleChange(self, extra_lines=None):
    """Returns an example Change instance for tests."""
    description_lines = [
      'Hello there',
      'This is a change',
    ] + (extra_lines or [])
    files = [
      ['A', os.path.join('haspresubmit', 'blat.cc')],
    ]
    return presubmit.Change(
        name='mychange',
        description='\n'.join(description_lines),
        local_root=self.fake_root_dir,
        files=files,
        issue=0,
        patchset=0,
        author=None)

  def testMergeMasters(self):
    merge = presubmit._MergeMasters
    self.assertEqual({}, merge({}, {}))
    self.assertEqual({'m1': {}}, merge({}, {'m1': {}}))
    self.assertEqual({'m1': {}}, merge({'m1': {}}, {}))
    parts = [
      {'try1.cr': {'win': set(['defaulttests'])}},
      {'try1.cr': {'linux1': set(['test1'])},
       'try2.cr': {'linux2': set(['defaulttests'])}},
      {'try1.cr': {'mac1': set(['defaulttests']),
                   'mac2': set(['test1', 'test2']),
                   'linux1': set(['defaulttests'])}},
    ]
    expected = {
      'try1.cr': {'win': set(['defaulttests']),
                  'linux1': set(['defaulttests', 'test1']),
                  'mac1': set(['defaulttests']),
                  'mac2': set(['test1', 'test2'])},
      'try2.cr': {'linux2': set(['defaulttests'])},
    }
    for permutation in itertools.permutations(parts):
      self.assertEqual(expected, reduce(merge, permutation, {}))

  def testDoGetTryMasters(self):
    root_text = (self.presubmit_trymaster
        % '{"t1.cr": {"win": set(["defaulttests"])}}')
    linux_text = (self.presubmit_trymaster
        % ('{"t1.cr": {"linux1": set(["t1"])},'
           ' "t2.cr": {"linux2": set(["defaulttests"])}}'))

    filename = 'foo.cc'
    filename_linux = os.path.join('linux_only', 'penguin.cc')
    root_presubmit = os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    linux_presubmit = os.path.join(
        self.fake_root_dir, 'linux_only', 'PRESUBMIT.py')

    os.path.isfile.side_effect = (
        lambda f: f in [root_presubmit, linux_presubmit])
    os.listdir.return_value = ['PRESUBMIT.py']
    gclient_utils.FileRead.side_effect = (
        lambda f, _: root_text if f == root_presubmit else linux_text)

    change = presubmit.Change(
        'mychange', '', self.fake_root_dir, [], 0, 0, None)

    output = StringIO.StringIO()
    self.assertEqual({'t1.cr': {'win': ['defaulttests']}},
                     presubmit.DoGetTryMasters(change, [filename],
                                               self.fake_root_dir,
                                               None, None, False, output))
    output = StringIO.StringIO()
    expected = {
      't1.cr': {'win': ['defaulttests'], 'linux1': ['t1']},
      't2.cr': {'linux2': ['defaulttests']},
    }
    self.assertEqual(expected,
                     presubmit.DoGetTryMasters(change,
                                               [filename, filename_linux],
                                               self.fake_root_dir, None, None,
                                               False, output))

  @mock.patch('presubmit_support.DoPresubmitChecks')
  @mock.patch('presubmit_support.ParseFiles')
  def testMainUnversioned(self, mockParseFiles, mockDoPresubmitChecks):
    scm.determine_scm.return_value = None
    mockParseFiles.return_value = [('M', 'random_file.txt')]
    mockDoPresubmitChecks().should_continue.return_value = False

    self.assertEqual(
        True,
        presubmit.main(['--root', self.fake_root_dir, 'random_file.txt']))

  def testMainUnversionedFail(self):
    scm.determine_scm.return_value = None

    with self.assertRaises(SystemExit) as e:
      presubmit.main(['--root', self.fake_root_dir])
    self.assertEqual(2, e.exception.code)

    self.assertEqual(
        sys.stderr.getvalue(),
        'Usage: presubmit_unittest.py [options] <files...>\n'
        '\n'
        'presubmit_unittest.py: error: For unversioned directory, <files> is '
        'not optional.\n')


class InputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.InputApi."""
  def testInputApiConstruction(self):
    api = presubmit.InputApi(
        self.fake_change,
        presubmit_path='foo/path/PRESUBMIT.py',
        is_committing=False, gerrit_obj=None, verbose=False)
    self.assertEqual(api.PresubmitLocalPath(), 'foo/path')
    self.assertEqual(api.change, self.fake_change)

  def testInputApiPresubmitScriptFiltering(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123',
                         ' STORY =http://foo/  \t',
                         'and some more regular text')
    files = [
      ['A', os.path.join('foo', 'blat.cc'), True],
      ['M', os.path.join('foo', 'blat', 'READ_ME2'), True],
      ['M', os.path.join('foo', 'blat', 'binary.dll'), True],
      ['M', os.path.join('foo', 'blat', 'weird.xyz'), True],
      ['M', os.path.join('foo', 'blat', 'another.h'), True],
      ['M', os.path.join('foo', 'third_party', 'third.cc'), True],
      ['D', os.path.join('foo', 'mat', 'beingdeleted.txt'), False],
      ['M', os.path.join('flop', 'notfound.txt'), False],
      ['A', os.path.join('boo', 'flap.h'), True],
    ]
    diffs = []
    known_files = []
    for _, f, exists in files:
      full_file = os.path.join(self.fake_root_dir, f)
      if exists and f.startswith('foo/'):
        known_files.append(full_file)
      diffs.append(self.presubmit_diffs % {'filename': f})

    os.path.isfile.side_effect = lambda f: f in known_files
    presubmit.scm.GIT.GenerateDiff.return_value = '\n'.join(diffs)

    change = presubmit.GitChange(
        'mychange',
        '\n'.join(description_lines),
        self.fake_root_dir,
        [[f[0], f[1]] for f in files],
        0,
        0,
        None)
    input_api = presubmit.InputApi(
        change,
        os.path.join(self.fake_root_dir, 'foo', 'PRESUBMIT.py'),
        False, None, False)
    # Doesn't filter much
    got_files = input_api.AffectedFiles()
    self.assertEqual(len(got_files), 7)
    self.assertEqual(got_files[0].LocalPath(), presubmit.normpath(files[0][1]))
    self.assertEqual(got_files[1].LocalPath(), presubmit.normpath(files[1][1]))
    self.assertEqual(got_files[2].LocalPath(), presubmit.normpath(files[2][1]))
    self.assertEqual(got_files[3].LocalPath(), presubmit.normpath(files[3][1]))
    self.assertEqual(got_files[4].LocalPath(), presubmit.normpath(files[4][1]))
    self.assertEqual(got_files[5].LocalPath(), presubmit.normpath(files[5][1]))
    self.assertEqual(got_files[6].LocalPath(), presubmit.normpath(files[6][1]))
    # Ignores weird because of whitelist, third_party because of blacklist,
    # binary isn't a text file and beingdeleted doesn't exist. The rest is
    # outside foo/.
    rhs_lines = [x for x in input_api.RightHandSideLines(None)]
    self.assertEqual(len(rhs_lines), 14)
    self.assertEqual(rhs_lines[0][0].LocalPath(),
                     presubmit.normpath(files[0][1]))
    self.assertEqual(rhs_lines[3][0].LocalPath(),
                     presubmit.normpath(files[0][1]))
    self.assertEqual(rhs_lines[7][0].LocalPath(),
                     presubmit.normpath(files[4][1]))
    self.assertEqual(rhs_lines[13][0].LocalPath(),
                     presubmit.normpath(files[4][1]))

  def testInputApiFilterSourceFile(self):
    files = [
      ['A', os.path.join('foo', 'blat.cc')],
      ['M', os.path.join('foo', 'blat', 'READ_ME2')],
      ['M', os.path.join('foo', 'blat', 'binary.dll')],
      ['M', os.path.join('foo', 'blat', 'weird.xyz')],
      ['M', os.path.join('foo', 'blat', 'another.h')],
      ['M', os.path.join(
        'foo', 'third_party', 'WebKit', 'WebKit.cpp')],
      ['M', os.path.join(
        'foo', 'third_party', 'WebKit2', 'WebKit2.cpp')],
      ['M', os.path.join('foo', 'third_party', 'blink', 'blink.cc')],
      ['M', os.path.join(
        'foo', 'third_party', 'blink1', 'blink1.cc')],
      ['M', os.path.join('foo', 'third_party', 'third', 'third.cc')],
    ]
    known_files = [
        os.path.join(self.fake_root_dir, f)
        for _, f in files]
    os.path.isfile.side_effect = lambda f: f in known_files

    change = presubmit.GitChange(
        'mychange',
        'description\nlines\n',
        self.fake_root_dir,
        [[f[0], f[1]] for f in files],
        0,
        0,
        None)
    input_api = presubmit.InputApi(
        change,
        os.path.join(self.fake_root_dir, 'foo', 'PRESUBMIT.py'),
        False, None, False)
    # We'd like to test FilterSourceFile, which is used by
    # AffectedSourceFiles(None).
    got_files = input_api.AffectedSourceFiles(None)
    self.assertEqual(len(got_files), 4)
    # blat.cc, another.h, WebKit.cpp, and blink.cc remain.
    self.assertEqual(got_files[0].LocalPath(), presubmit.normpath(files[0][1]))
    self.assertEqual(got_files[1].LocalPath(), presubmit.normpath(files[4][1]))
    self.assertEqual(got_files[2].LocalPath(), presubmit.normpath(files[5][1]))
    self.assertEqual(got_files[3].LocalPath(), presubmit.normpath(files[7][1]))

  def testDefaultWhiteListBlackListFilters(self):
    def f(x):
      return presubmit.AffectedFile(x, 'M', self.fake_root_dir, None)
    files = [
      (
        [
          # To be tested.
          f('testing_support/google_appengine/b'),
          f('testing_support/not_google_appengine/foo.cc'),
        ],
        [
          # Expected.
          'testing_support/not_google_appengine/foo.cc',
        ],
      ),
      (
        [
          # To be tested.
          f('a/experimental/b'),
          f('experimental/b'),
          f('a/experimental'),
          f('a/experimental.cc'),
          f('a/experimental.S'),
        ],
        [
          # Expected.
          'a/experimental.cc',
          'a/experimental.S',
        ],
      ),
      (
        [
          # To be tested.
          f('a/third_party/b'),
          f('third_party/b'),
          f('a/third_party'),
          f('a/third_party.cc'),
        ],
        [
          # Expected.
          'a/third_party.cc',
        ],
      ),
      (
        [
          # To be tested.
          f('a/LOL_FILE/b'),
          f('b.c/LOL_FILE'),
          f('a/PRESUBMIT.py'),
          f('a/FOO.json'),
          f('a/FOO.java'),
          f('a/FOO.mojom'),
        ],
        [
          # Expected.
          'a/PRESUBMIT.py',
          'a/FOO.java',
          'a/FOO.mojom',
        ],
      ),
      (
        [
          # To be tested.
          f('a/.git'),
          f('b.c/.git'),
          f('a/.git/bleh.py'),
          f('.git/bleh.py'),
          f('bleh.diff'),
          f('foo/bleh.patch'),
        ],
        [
          # Expected.
        ],
      ),
    ]
    input_api = presubmit.InputApi(
        self.fake_change, './PRESUBMIT.py', False, None, False)

    self.assertEqual(len(input_api.DEFAULT_WHITE_LIST), 24)
    self.assertEqual(len(input_api.DEFAULT_BLACK_LIST), 12)
    for item in files:
      results = filter(input_api.FilterSourceFile, item[0])
      for i in range(len(results)):
        self.assertEqual(results[i].LocalPath(),
                          presubmit.normpath(item[1][i]))
      # Same number of expected results.
      self.assertEqual(sorted([f.LocalPath().replace(os.sep, '/')
                                for f in results]),
                        sorted(item[1]))

  def testCustomFilter(self):
    def FilterSourceFile(affected_file):
      return 'a' in affected_file.LocalPath()
    files = [('A', 'eeaee'), ('M', 'eeabee'), ('M', 'eebcee')]
    known_files = [
      os.path.join(self.fake_root_dir, item)
      for _, item in files]
    os.path.isfile.side_effect = lambda f: f in known_files

    change = presubmit.GitChange(
        'mychange', '', self.fake_root_dir, files, 0, 0, None)
    input_api = presubmit.InputApi(
        change,
        os.path.join(self.fake_root_dir, 'PRESUBMIT.py'),
        False, None, False)
    got_files = input_api.AffectedSourceFiles(FilterSourceFile)
    self.assertEqual(len(got_files), 2)
    self.assertEqual(got_files[0].LocalPath(), 'eeaee')
    self.assertEqual(got_files[1].LocalPath(), 'eeabee')

  def testLambdaFilter(self):
    white_list = presubmit.InputApi.DEFAULT_BLACK_LIST + (r".*?a.*?",)
    black_list = [r".*?b.*?"]
    files = [('A', 'eeaee'), ('M', 'eeabee'), ('M', 'eebcee'), ('M', 'eecaee')]
    known_files = [
      os.path.join(self.fake_root_dir, item)
      for _, item in files]
    os.path.isfile.side_effect = lambda f: f in known_files

    change = presubmit.GitChange(
        'mychange', '', self.fake_root_dir, files, 0, 0, None)
    input_api = presubmit.InputApi(
        change, './PRESUBMIT.py', False, None, False)
    # Sample usage of overiding the default white and black lists.
    got_files = input_api.AffectedSourceFiles(
        lambda x: input_api.FilterSourceFile(x, white_list, black_list))
    self.assertEqual(len(got_files), 2)
    self.assertEqual(got_files[0].LocalPath(), 'eeaee')
    self.assertEqual(got_files[1].LocalPath(), 'eecaee')

  def testGetAbsoluteLocalPath(self):
    normpath = presubmit.normpath
    # Regression test for bug of presubmit stuff that relies on invoking
    # SVN (e.g. to get mime type of file) not working unless gcl invoked
    # from the client root (e.g. if you were at 'src' and did 'cd base' before
    # invoking 'gcl upload' it would fail because svn wouldn't find the files
    # the presubmit script was asking about).
    files = [
      ['A', 'isdir'],
      ['A', os.path.join('isdir', 'blat.cc')],
      ['M', os.path.join('elsewhere', 'ouf.cc')],
    ]

    change = presubmit.Change(
        'mychange', '', self.fake_root_dir, files, 0, 0, None)
    affected_files = change.AffectedFiles()
    # Local paths should remain the same
    self.assertEqual(affected_files[0].LocalPath(), normpath('isdir'))
    self.assertEqual(affected_files[1].LocalPath(), normpath('isdir/blat.cc'))
    # Absolute paths should be prefixed
    self.assertEqual(
        affected_files[0].AbsoluteLocalPath(),
        presubmit.normpath(os.path.join(self.fake_root_dir, 'isdir')))
    self.assertEqual(
        affected_files[1].AbsoluteLocalPath(),
        presubmit.normpath(os.path.join(
            self.fake_root_dir, 'isdir/blat.cc')))

    # New helper functions need to work
    paths_from_change = change.AbsoluteLocalPaths()
    self.assertEqual(len(paths_from_change), 3)
    presubmit_path = os.path.join(
        self.fake_root_dir, 'isdir', 'PRESUBMIT.py')
    api = presubmit.InputApi(
        change=change, presubmit_path=presubmit_path,
        is_committing=True, gerrit_obj=None, verbose=False)
    paths_from_api = api.AbsoluteLocalPaths()
    self.assertEqual(len(paths_from_api), 2)
    for absolute_paths in [paths_from_change, paths_from_api]:
      self.assertEqual(
          absolute_paths[0],
          presubmit.normpath(os.path.join(
              self.fake_root_dir, 'isdir')))
      self.assertEqual(
          absolute_paths[1],
          presubmit.normpath(os.path.join(
              self.fake_root_dir, 'isdir', 'blat.cc')))

  def testDeprecated(self):
    change = presubmit.Change(
        'mychange', '', self.fake_root_dir, [], 0, 0, None)
    api = presubmit.InputApi(
        change,
        os.path.join(self.fake_root_dir, 'foo', 'PRESUBMIT.py'), True,
        None, False)
    api.AffectedTestableFiles(include_deletes=False)

  def testReadFileStringDenied(self):

    change = presubmit.Change(
        'foo', 'foo', self.fake_root_dir, [('M', 'AA')], 0, 0, None)
    input_api = presubmit.InputApi(
        change, os.path.join(self.fake_root_dir, '/p'), False,
        None, False)
    self.assertRaises(IOError, input_api.ReadFile, 'boo', 'x')

  def testReadFileStringAccepted(self):
    path = os.path.join(self.fake_root_dir, 'AA/boo')
    presubmit.gclient_utils.FileRead.return_code = None

    change = presubmit.Change(
        'foo', 'foo', self.fake_root_dir, [('M', 'AA')], 0, 0, None)
    input_api = presubmit.InputApi(
        change, os.path.join(self.fake_root_dir, '/p'), False,
        None, False)
    input_api.ReadFile(path, 'x')

  def testReadFileAffectedFileDenied(self):
    fileobj = presubmit.AffectedFile('boo', 'M', 'Unrelated',
                                     diff_cache=mock.Mock())

    change = presubmit.Change(
        'foo', 'foo', self.fake_root_dir, [('M', 'AA')], 0, 0, None)
    input_api = presubmit.InputApi(
        change, os.path.join(self.fake_root_dir, '/p'), False,
        None, False)
    self.assertRaises(IOError, input_api.ReadFile, fileobj, 'x')

  def testReadFileAffectedFileAccepted(self):
    fileobj = presubmit.AffectedFile('AA/boo', 'M', self.fake_root_dir,
                                     diff_cache=mock.Mock())
    presubmit.gclient_utils.FileRead.return_code = None

    change = presubmit.Change(
        'foo', 'foo', self.fake_root_dir, [('M', 'AA')], 0, 0, None)
    input_api = presubmit.InputApi(
        change, os.path.join(self.fake_root_dir, '/p'), False,
        None, False)
    input_api.ReadFile(fileobj, 'x')

  def testCreateTemporaryFile(self):
    input_api = presubmit.InputApi(
        self.fake_change,
        presubmit_path='foo/path/PRESUBMIT.py',
        is_committing=False, gerrit_obj=None, verbose=False)
    tempfile.NamedTemporaryFile.side_effect = [
        MockTemporaryFile('foo'), MockTemporaryFile('bar')]

    self.assertEqual(0, len(input_api._named_temporary_files))
    with input_api.CreateTemporaryFile():
      self.assertEqual(1, len(input_api._named_temporary_files))
    self.assertEqual(['foo'], input_api._named_temporary_files)
    with input_api.CreateTemporaryFile():
      self.assertEqual(2, len(input_api._named_temporary_files))
    self.assertEqual(2, len(input_api._named_temporary_files))
    self.assertEqual(['foo', 'bar'], input_api._named_temporary_files)

    self.assertRaises(TypeError, input_api.CreateTemporaryFile, delete=True)
    self.assertRaises(TypeError, input_api.CreateTemporaryFile, delete=False)
    self.assertEqual(['foo', 'bar'], input_api._named_temporary_files)


class OutputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.OutputApi."""
  def testOutputApiBasics(self):
    self.assertIsNotNone(presubmit.OutputApi.PresubmitError('').fatal)
    self.assertFalse(presubmit.OutputApi.PresubmitError('').should_prompt)

    self.assertFalse(presubmit.OutputApi.PresubmitPromptWarning('').fatal)
    self.assertIsNotNone(
        presubmit.OutputApi.PresubmitPromptWarning('').should_prompt)

    self.assertFalse(presubmit.OutputApi.PresubmitNotifyResult('').fatal)
    self.assertFalse(
        presubmit.OutputApi.PresubmitNotifyResult('').should_prompt)

    # TODO(joi) Test MailTextResult once implemented.

  def testAppendCC(self):
    output_api = presubmit.OutputApi(False)
    output_api.AppendCC('chromium-reviews@chromium.org')
    self.assertEqual(['chromium-reviews@chromium.org'], output_api.more_cc)


  def testOutputApiHandling(self):

    output = presubmit.PresubmitOutput()
    presubmit.OutputApi.PresubmitError('!!!').handle(output)
    self.assertFalse(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('!!!'))

    output = presubmit.PresubmitOutput()
    presubmit.OutputApi.PresubmitNotifyResult('?see?').handle(output)
    self.assertIsNotNone(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('?see?'))

    output = presubmit.PresubmitOutput(input_stream=StringIO.StringIO('y'))
    presubmit.OutputApi.PresubmitPromptWarning('???').handle(output)
    output.prompt_yes_no('prompt: ')
    self.assertIsNotNone(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('???'))

    output = presubmit.PresubmitOutput(input_stream=StringIO.StringIO('\n'))
    presubmit.OutputApi.PresubmitPromptWarning('???').handle(output)
    output.prompt_yes_no('prompt: ')
    self.assertFalse(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('???'))

    output_api = presubmit.OutputApi(True)
    output = presubmit.PresubmitOutput(input_stream=StringIO.StringIO('y'))
    output_api.PresubmitPromptOrNotify('???').handle(output)
    output.prompt_yes_no('prompt: ')
    self.assertIsNotNone(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('???'))

    output_api = presubmit.OutputApi(False)
    output = presubmit.PresubmitOutput(input_stream=StringIO.StringIO('y'))
    output_api.PresubmitPromptOrNotify('???').handle(output)
    self.assertIsNotNone(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('???'))

    output_api = presubmit.OutputApi(True)
    output = presubmit.PresubmitOutput(input_stream=StringIO.StringIO('\n'))
    output_api.PresubmitPromptOrNotify('???').handle(output)
    output.prompt_yes_no('prompt: ')
    self.assertFalse(output.should_continue())
    self.assertIsNotNone(output.getvalue().count('???'))


class AffectedFileUnittest(PresubmitTestsBase):
  def testAffectedFile(self):
    gclient_utils.FileRead.return_value = 'whatever\ncookie'
    af = presubmit.GitAffectedFile('foo/blat.cc', 'M', self.fake_root_dir, None)
    self.assertEqual(presubmit.normpath('foo/blat.cc'), af.LocalPath())
    self.assertEqual('M', af.Action())
    self.assertEqual(['whatever', 'cookie'], af.NewContents())

  def testAffectedFileNotExists(self):
    notfound = 'notfound.cc'
    gclient_utils.FileRead.side_effect = IOError
    af = presubmit.AffectedFile(notfound, 'A', self.fake_root_dir, None)
    self.assertEqual([], af.NewContents())

  def testIsTestableFile(self):
    files = [
        presubmit.GitAffectedFile('foo/blat.txt', 'M', self.fake_root_dir,
                                  None),
        presubmit.GitAffectedFile('foo/binary.blob', 'M', self.fake_root_dir,
                                  None),
        presubmit.GitAffectedFile('blat/flop.txt', 'D', self.fake_root_dir,
                                  None)
    ]
    blat = os.path.join('foo', 'blat.txt')
    blob = os.path.join('foo', 'binary.blob')
    f_blat = os.path.join(self.fake_root_dir, blat)
    f_blob = os.path.join(self.fake_root_dir, blob)
    os.path.isfile.side_effect = lambda f: f in [f_blat, f_blob]

    output = filter(lambda x: x.IsTestableFile(), files)
    self.assertEqual(2, len(output))
    self.assertEqual(files[:2], output[:2])


class ChangeUnittest(PresubmitTestsBase):
  def testAffectedFiles(self):
    change = presubmit.Change(
        '', '', self.fake_root_dir, [('Y', 'AA')], 3, 5, '')
    self.assertEqual(1, len(change.AffectedFiles()))
    self.assertEqual('Y', change.AffectedFiles()[0].Action())

  def testSetDescriptionText(self):
    change = presubmit.Change(
        '', 'foo\nDRU=ro', self.fake_root_dir, [], 3, 5, '')
    self.assertEqual('foo', change.DescriptionText())
    self.assertEqual('foo\nDRU=ro', change.FullDescriptionText())
    self.assertEqual({'DRU': 'ro'}, change.tags)

    change.SetDescriptionText('WHIZ=bang\nbar\nFOO=baz')
    self.assertEqual('bar', change.DescriptionText())
    self.assertEqual('WHIZ=bang\nbar\nFOO=baz', change.FullDescriptionText())
    self.assertEqual({'WHIZ': 'bang', 'FOO': 'baz'}, change.tags)

  def testBugsFromDescription_MixedTagsAndFooters(self):
    change = presubmit.Change(
        '', 'foo\nBUG=2,1\n\nChange-Id: asdf\nBug: 3, 6',
        self.fake_root_dir, [], 0, 0, '')
    self.assertEqual(['1', '2', '3', '6'], change.BugsFromDescription())
    self.assertEqual('1,2,3,6', change.BUG)

  def testBugsFromDescription_MultipleFooters(self):
    change = presubmit.Change(
        '', 'foo\n\nChange-Id: asdf\nBug: 1\nBug:4,  6',
        self.fake_root_dir, [], 0, 0, '')
    self.assertEqual(['1', '4', '6'], change.BugsFromDescription())
    self.assertEqual('1,4,6', change.BUG)

  def testReviewersFromDescription(self):
    change = presubmit.Change(
        '', 'foo\nR=foo,bar\n\nChange-Id: asdf\nR: baz',
        self.fake_root_dir, [], 0, 0, '')
    self.assertEqual(['bar', 'foo'], change.ReviewersFromDescription())
    self.assertEqual('bar,foo', change.R)

  def testTBRsFromDescription(self):
    change = presubmit.Change(
        '', 'foo\nTBR=foo,bar\n\nChange-Id: asdf\nTBR: baz',
        self.fake_root_dir, [], 0, 0, '')
    self.assertEqual(['bar', 'baz', 'foo'], change.TBRsFromDescription())
    self.assertEqual('bar,baz,foo', change.TBR)


class CannedChecksUnittest(PresubmitTestsBase):
  """Tests presubmit_canned_checks.py."""
  def MockInputApi(self, change, committing):
    # pylint: disable=no-self-use
    input_api = mock.MagicMock(presubmit.InputApi)
    input_api.thread_pool = presubmit.ThreadPool()
    input_api.parallel = False
    input_api.cStringIO = presubmit.cStringIO
    input_api.json = presubmit.json
    input_api.logging = logging
    input_api.os_listdir = mock.Mock()
    input_api.os_walk = mock.Mock()
    input_api.os_path = os.path
    input_api.re = presubmit.re
    input_api.gerrit = mock.MagicMock(presubmit.GerritAccessor)
    input_api.traceback = presubmit.traceback
    input_api.urllib2 = mock.MagicMock(presubmit.urllib2)
    input_api.unittest = unittest
    input_api.subprocess = subprocess
    class fake_CalledProcessError(Exception):
      def __str__(self):
        return 'foo'
    input_api.subprocess.CalledProcessError = fake_CalledProcessError
    input_api.verbose = False
    input_api.is_windows = False

    input_api.change = change
    input_api.is_committing = committing
    input_api.tbr = False
    input_api.dry_run = None
    input_api.python_executable = 'pyyyyython'
    input_api.platform = sys.platform
    input_api.cpu_count = 2
    input_api.time = time
    input_api.canned_checks = presubmit_canned_checks
    input_api.Command = presubmit.CommandData
    input_api.RunTests = functools.partial(
        presubmit.InputApi.RunTests, input_api)
    return input_api

  def DescriptionTest(self, check, description1, description2, error_type,
                      committing):
    change1 = presubmit.Change(
        'foo1', description1, self.fake_root_dir, None, 0, 0, None)
    input_api1 = self.MockInputApi(change1, committing)
    change2 = presubmit.Change(
        'foo2', description2, self.fake_root_dir, None, 0, 0, None)
    input_api2 = self.MockInputApi(change2, committing)

    results1 = check(input_api1, presubmit.OutputApi)
    self.assertEqual(results1, [])
    results2 = check(input_api2, presubmit.OutputApi)
    self.assertEqual(len(results2), 1)
    self.assertTrue(isinstance(results2[0], error_type))

  def ContentTest(self, check, content1, content1_path, content2,
                  content2_path, error_type):
    """Runs a test of a content-checking rule.

      Args:
        check: the check to run.
        content1: content which is expected to pass the check.
        content1_path: file path for content1.
        content2: content which is expected to fail the check.
        content2_path: file path for content2.
        error_type: the type of the error expected for content2.
    """
    change1 = presubmit.Change(
        'foo1', 'foo1\n', self.fake_root_dir, None, 0, 0, None)
    input_api1 = self.MockInputApi(change1, False)
    affected_file1 = mock.MagicMock(presubmit.GitAffectedFile)
    input_api1.AffectedFiles.return_value = [affected_file1]
    affected_file1.LocalPath.return_value = content1_path
    affected_file1.NewContents.return_value = [
        'afoo',
        content1,
        'bfoo',
        'cfoo',
        'dfoo']

    change2 = presubmit.Change(
        'foo2', 'foo2\n', self.fake_root_dir, None, 0, 0, None)
    input_api2 = self.MockInputApi(change2, False)

    affected_file2 = mock.MagicMock(presubmit.GitAffectedFile)
    input_api2.AffectedFiles.return_value = [affected_file2]
    affected_file2.LocalPath.return_value = content2_path
    affected_file2.NewContents.return_value = [
        'dfoo',
        content2,
        'efoo',
        'ffoo',
        'gfoo']
    # It falls back to ChangedContents when there is a failure. This is an
    # optimization since NewContents() is much faster to execute than
    # ChangedContents().
    affected_file2.ChangedContents.return_value = [
        (42, content2),
        (43, 'hfoo'),
        (23, 'ifoo')]
    affected_file2.LocalPath.return_value = 'foo.cc'


    results1 = check(input_api1, presubmit.OutputApi, None)
    self.assertEqual(results1, [])
    results2 = check(input_api2, presubmit.OutputApi, None)
    self.assertEqual(len(results2), 1)
    self.assertEqual(results2[0].__class__, error_type)

  def PythonLongLineTest(self, maxlen, content, should_pass):
    """Runs a test of Python long-line checking rule.

    Because ContentTest() cannot be used here due to the different code path
    that the implementation of CheckLongLines() uses for Python files.

    Args:
      maxlen: Maximum line length for content.
      content: Python source which is expected to pass or fail the test.
      should_pass: True iff the test should pass, False otherwise.
    """
    change = presubmit.Change('foo1', 'foo1\n', self.fake_root_dir, None, 0, 0,
                              None)
    input_api = self.MockInputApi(change, False)
    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    input_api.AffectedFiles.return_value = [affected_file]
    affected_file.LocalPath.return_value = 'foo.py'
    affected_file.NewContents.return_value = content.splitlines()

    results = presubmit_canned_checks.CheckLongLines(
        input_api, presubmit.OutputApi, maxlen)
    if should_pass:
      self.assertEqual(results, [])
    else:
      self.assertEqual(len(results), 1)
      self.assertEqual(results[0].__class__,
                        presubmit.OutputApi.PresubmitPromptWarning)

  def ReadFileTest(self, check, content1, content2, error_type):
    change1 = presubmit.Change(
        'foo1', 'foo1\n', self.fake_root_dir, None, 0, 0, None)
    input_api1 = self.MockInputApi(change1, False)
    affected_file1 = mock.MagicMock(presubmit.GitAffectedFile)
    input_api1.AffectedSourceFiles.return_value = [affected_file1]
    input_api1.ReadFile.return_value = content1
    change2 = presubmit.Change(
        'foo2', 'foo2\n', self.fake_root_dir, None, 0, 0, None)
    input_api2 = self.MockInputApi(change2, False)
    affected_file2 = mock.MagicMock(presubmit.GitAffectedFile)
    input_api2.AffectedSourceFiles.return_value = [affected_file2]
    input_api2.ReadFile.return_value = content2
    affected_file2.LocalPath.return_value = 'bar.cc'

    results = check(input_api1, presubmit.OutputApi)
    self.assertEqual(results, [])
    results2 = check(input_api2, presubmit.OutputApi)
    self.assertEqual(len(results2), 1)
    self.assertEqual(results2[0].__class__, error_type)

  def testCannedCheckChangeHasBugField(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasBugField,
                         'Foo\nBUG=1234', 'Foo\n',
                         presubmit.OutputApi.PresubmitNotifyResult,
                         False)

  def testCheckChangeHasDescription(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasDescription,
                         'Bleh', '',
                         presubmit.OutputApi.PresubmitNotifyResult,
                         False)
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasDescription,
                         'Bleh', '',
                         presubmit.OutputApi.PresubmitError,
                         True)

  def testCannedCheckDoNotSubmitInDescription(self):
    self.DescriptionTest(presubmit_canned_checks.CheckDoNotSubmitInDescription,
                         'Foo\nDO NOTSUBMIT', 'Foo\nDO NOT ' + 'SUBMIT',
                         presubmit.OutputApi.PresubmitError,
                         False)

  def testCannedCheckDoNotSubmitInFiles(self):
    self.ContentTest(
        lambda x,y,z: presubmit_canned_checks.CheckDoNotSubmitInFiles(x, y),
        'DO NOTSUBMIT', None, 'DO NOT ' + 'SUBMIT', None,
        presubmit.OutputApi.PresubmitError)

  def testCheckChangeHasNoStrayWhitespace(self):
    self.ContentTest(
        lambda x,y,z:
            presubmit_canned_checks.CheckChangeHasNoStrayWhitespace(x, y),
        'Foo', None, 'Foo ', None,
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeHasOnlyOneEol(self):
    self.ReadFileTest(presubmit_canned_checks.CheckChangeHasOnlyOneEol,
                      "Hey!\nHo!\n", "Hey!\nHo!\n\n",
                      presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeHasNoCR(self):
    self.ReadFileTest(presubmit_canned_checks.CheckChangeHasNoCR,
                      "Hey!\nHo!\n", "Hey!\r\nHo!\r\n",
                      presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeHasNoCrAndHasOnlyOneEol(self):
    self.ReadFileTest(
        presubmit_canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol,
        "Hey!\nHo!\n", "Hey!\nHo!\n\n",
        presubmit.OutputApi.PresubmitPromptWarning)
    self.ReadFileTest(
        presubmit_canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol,
        "Hey!\nHo!\n", "Hey!\r\nHo!\r\n",
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeTodoHasOwner(self):
    self.ContentTest(presubmit_canned_checks.CheckChangeTodoHasOwner,
                     "TODO(foo): bar", None, "TODO: bar", None,
                     presubmit.OutputApi.PresubmitPromptWarning)

  @mock.patch('git_cl.Changelist')
  @mock.patch('auth.get_authenticator_for_host')
  def testCannedCheckChangedLUCIConfigs(self, mockGAFH, mockChangelist):
    affected_file1 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file1.LocalPath.return_value = 'foo.cfg'
    affected_file1.NewContents.return_value = ['test', 'foo']
    affected_file2 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file2.LocalPath.return_value = 'bar.cfg'
    affected_file2.NewContents.return_value = ['test', 'bar']

    mockGAFH().get_access_token().token = 123

    host = 'https://host.com'
    branch = 'branch'
    http_resp = {
      'messages': [{'severity': 'ERROR', 'text': 'deadbeef'}],
      'config_sets': [{'config_set': 'deadbeef',
                       'location': '%s/+/%s' % (host, branch)}]
    }
    urllib2.urlopen.return_value = http_resp
    json.load.return_value = http_resp

    mockChangelist().GetRemoteBranch.return_value = ('remote', branch)
    mockChangelist().GetRemoteUrl.return_value = host

    change1 = presubmit.Change(
      'foo', 'foo1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change1, False)
    affected_files = (affected_file1, affected_file2)

    input_api.AffectedFiles = lambda **_: affected_files

    results = presubmit_canned_checks.CheckChangedLUCIConfigs(
        input_api, presubmit.OutputApi)
    self.assertEqual(len(results), 1)

  def testCannedCheckChangeHasNoTabs(self):
    self.ContentTest(presubmit_canned_checks.CheckChangeHasNoTabs,
                     'blah blah', None, 'blah\tblah', None,
                     presubmit.OutputApi.PresubmitPromptWarning)

    # Make sure makefiles are ignored.
    change1 = presubmit.Change(
        'foo1', 'foo1\n', self.fake_root_dir, None, 0, 0, None)
    input_api1 = self.MockInputApi(change1, False)
    affected_file1 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file1.LocalPath.return_value = 'foo.cc'
    affected_file2 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file2.LocalPath.return_value = 'foo/Makefile'
    affected_file3 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file3.LocalPath.return_value = 'makefile'
    # Only this one will trigger.
    affected_file4 = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file1.LocalPath.return_value = 'foo.cc'
    affected_file1.NewContents.return_value = ['yo, ']
    affected_file4.LocalPath.return_value = 'makefile.foo'
    affected_file4.LocalPath.return_value = 'makefile.foo'
    affected_file4.NewContents.return_value = ['ye\t']
    affected_file4.ChangedContents.return_value = [(46, 'ye\t')]
    affected_file4.LocalPath.return_value = 'makefile.foo'
    affected_files = (affected_file1, affected_file2,
                      affected_file3, affected_file4)

    def test(include_deletes=True, file_filter=None):
      self.assertFalse(include_deletes)
      for x in affected_files:
        if file_filter(x):
          yield x
    # Override the mock of these functions.
    input_api1.FilterSourceFile = lambda x: x
    input_api1.AffectedFiles = test

    results1 = presubmit_canned_checks.CheckChangeHasNoTabs(input_api1,
        presubmit.OutputApi, None)
    self.assertEqual(len(results1), 1)
    self.assertEqual(results1[0].__class__,
        presubmit.OutputApi.PresubmitPromptWarning)
    self.assertEqual(results1[0]._long_text,
        'makefile.foo:46')

  def testCannedCheckLongLines(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(check, '0123456789', None, '01234567890', None,
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckJavaLongLines(self):
    check = lambda x, y, _: presubmit_canned_checks.CheckLongLines(x, y, 80)
    self.ContentTest(check, 'A ' * 50, 'foo.java', 'A ' * 50 + 'B', 'foo.java',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckSpecialJavaLongLines(self):
    check = lambda x, y, _: presubmit_canned_checks.CheckLongLines(x, y, 80)
    self.ContentTest(check, 'import ' + 'A ' * 150, 'foo.java',
                     'importSomething ' + 'A ' * 50, 'foo.java',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckPythonLongLines(self):
    # NOTE: Cannot use ContentTest() here because of the different code path
    #       used for Python checks in CheckLongLines().
    passing_cases = [
        r"""
01234568901234589012345689012345689
A short line
""",
        r"""
01234568901234589012345689012345689
This line is too long but should pass # pylint: disable=line-too-long
""",
        r"""
01234568901234589012345689012345689
# pylint: disable=line-too-long
This line is too long but should pass due to global disable
""",
        r"""
01234568901234589012345689012345689
   #pylint: disable=line-too-long
This line is too long but should pass due to global disable.
""",
        r"""
01234568901234589012345689012345689
                       #    pylint: disable=line-too-long
This line is too long but should pass due to global disable.
""",
        r"""
01234568901234589012345689012345689
# import is a valid exception
import some.really.long.package.name.that.should.pass
""",
        r"""
01234568901234589012345689012345689
# from is a valid exception
from some.really.long.package.name import passing.line
""",
        r"""
01234568901234589012345689012345689
                    import some.package
""",
        r"""
01234568901234589012345689012345689
                    from some.package import stuff
""",
    ]
    for content in passing_cases:
      self.PythonLongLineTest(40, content, should_pass=True)

    failing_cases = [
        r"""
01234568901234589012345689012345689
This line is definitely too long and should fail.
""",
        r"""
01234568901234589012345689012345689
# pylint: disable=line-too-long
This line is too long and should pass due to global disable
# pylint: enable=line-too-long
But this line is too long and should still fail now
""",
        r"""
01234568901234589012345689012345689
# pylint: disable=line-too-long
This line is too long and should pass due to global disable
But this line is too long # pylint: enable=line-too-long
""",
        r"""
01234568901234589012345689012345689
This should fail because the global
check is enabled on the next line.
              #         pylint: enable=line-too-long
""",
        r"""
01234568901234589012345689012345689
# pylint: disable=line-too-long
                  # pylint: enable-foo-bar should pass
The following line should fail
since global directives apply to
the current line as well!
                  # pylint: enable-line-too-long should fail
""",
    ]
    for content in failing_cases[0:0]:
      self.PythonLongLineTest(40, content, should_pass=False)

  def testCannedCheckJSLongLines(self):
    check = lambda x, y, _: presubmit_canned_checks.CheckLongLines(x, y, 10)
    self.ContentTest(check, 'GEN(\'#include "c/b/ui/webui/fixture.h"\');',
                     'foo.js', "// GEN('something');", 'foo.js',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckObjCExceptionLongLines(self):
    check = lambda x, y, _: presubmit_canned_checks.CheckLongLines(x, y, 80)
    self.ContentTest(check, '#import ' + 'A ' * 150, 'foo.mm',
                     'import' + 'A ' * 150, 'foo.mm',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckMakefileLongLines(self):
    check = lambda x, y, _: presubmit_canned_checks.CheckLongLines(x, y, 80)
    self.ContentTest(check, 'A ' * 100, 'foo.mk', 'A ' * 100 + 'B', 'foo.mk',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckLongLinesLF(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(check, '012345678\n', None, '0123456789\n', None,
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckCppExceptionLongLines(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(
        check,
        '#if 56 89 12 45 9191919191919',
        'foo.cc',
        '#nif 56 89 12 45 9191919191919',
        'foo.cc',
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckLongLinesHttp(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(
        check,
        ' http:// 0 23 56',
        None,
        ' foob:// 0 23 56',
        None,
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckLongLinesFile(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(
        check,
        ' file:// 0 23 56',
        None,
        ' foob:// 0 23 56',
        None,
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckLongLinesCssUrl(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(
        check,
        ' url(some.png)',
        'foo.css',
        ' url(some.png)',
        'foo.cc',
        presubmit.OutputApi.PresubmitPromptWarning)


  def testCannedCheckLongLinesLongSymbol(self):
    check = lambda x, y, z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(
        check,
        ' TUP5D_LoNG_SY ',
        None,
        ' TUP5D_LoNG_SY5 ',
        None,
        presubmit.OutputApi.PresubmitPromptWarning)

  def _LicenseCheck(self, text, license_text, committing, expected_result,
      **kwargs):
    change = mock.MagicMock(presubmit.GitChange)
    change.scm = 'svn'
    input_api = self.MockInputApi(change, committing)
    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    input_api.AffectedSourceFiles.return_value = [affected_file]
    input_api.ReadFile.return_value = text
    if expected_result:
      affected_file.LocalPath.return_value = 'bleh'

    result = presubmit_canned_checks.CheckLicense(
                 input_api, presubmit.OutputApi, license_text,
                 source_file_filter=42,
                 **kwargs)
    if expected_result:
      self.assertEqual(len(result), 1)
      self.assertEqual(result[0].__class__, expected_result)
    else:
      self.assertEqual(result, [])

  def testCheckLicenseSuccess(self):
    text = (
        "#!/bin/python\n"
        "# Copyright (c) 2037 Nobody.\n"
        "# All Rights Reserved.\n"
        "print 'foo'\n"
    )
    license_text = (
        r".*? Copyright \(c\) 2037 Nobody." "\n"
        r".*? All Rights Reserved\." "\n"
    )
    self._LicenseCheck(text, license_text, True, None)

  def testCheckLicenseFailCommit(self):
    text = (
        "#!/bin/python\n"
        "# Copyright (c) 2037 Nobody.\n"
        "# All Rights Reserved.\n"
        "print 'foo'\n"
    )
    license_text = (
        r".*? Copyright \(c\) 0007 Nobody." "\n"
        r".*? All Rights Reserved\." "\n"
    )
    self._LicenseCheck(text, license_text, True,
                       presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckLicenseFailUpload(self):
    text = (
        "#!/bin/python\n"
        "# Copyright (c) 2037 Nobody.\n"
        "# All Rights Reserved.\n"
        "print 'foo'\n"
    )
    license_text = (
        r".*? Copyright \(c\) 0007 Nobody." "\n"
        r".*? All Rights Reserved\." "\n"
    )
    self._LicenseCheck(text, license_text, False,
                       presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckLicenseEmptySuccess(self):
    text = ''
    license_text = (
        r".*? Copyright \(c\) 2037 Nobody." "\n"
        r".*? All Rights Reserved\." "\n"
    )
    self._LicenseCheck(text, license_text, True, None, accept_empty_files=True)

  def testCannedCheckTreeIsOpenOpen(self):
    input_api = self.MockInputApi(None, True)
    input_api.urllib2.urlopen().read.return_value = 'The tree is open'
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi, url='url_to_open', closed='.*closed.*')
    self.assertEqual(results, [])

  def testCannedCheckTreeIsOpenClosed(self):
    input_api = self.MockInputApi(None, True)
    input_api.urllib2.urlopen().read.return_value = (
        'Tree is closed for maintenance')
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi,
        url='url_to_closed', closed='.*closed.*')
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
                      presubmit.OutputApi.PresubmitError)

  def testCannedCheckJsonTreeIsOpenOpen(self):
    input_api = self.MockInputApi(None, True)
    status = {
        'can_commit_freely': True,
        'general_state': 'open',
        'message': 'The tree is open'
    }
    input_api.urllib2.urlopen().read.return_value = json.dumps(status)
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi, json_url='url_to_open')
    self.assertEqual(results, [])

  def testCannedCheckJsonTreeIsOpenClosed(self):
    input_api = self.MockInputApi(None, True)
    status = {
        'can_commit_freely': False,
        'general_state': 'closed',
        'message': 'The tree is close',
    }
    input_api.urllib2.urlopen().read.return_value = json.dumps(status)
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi, json_url='url_to_closed')
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
                      presubmit.OutputApi.PresubmitError)

  def testRunPythonUnitTestsNoTest(self):
    input_api = self.MockInputApi(None, False)
    presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, [])
    results = input_api.thread_pool.RunAsync()
    self.assertEqual(results, [])

  def testRunPythonUnitTestsNonExistentUpload(self):
    input_api = self.MockInputApi(None, False)
    subprocess.Popen().returncode = 1  # pylint: disable=no-value-for-parameter
    presubmit.sigint_handler.wait.return_value = ('foo', None)

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['_non_existent_module'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
                      presubmit.OutputApi.PresubmitNotifyResult)

  def testRunPythonUnitTestsNonExistentCommitting(self):
    input_api = self.MockInputApi(None, True)
    subprocess.Popen().returncode = 1  # pylint: disable=no-value-for-parameter
    presubmit.sigint_handler.wait.return_value = ('foo', None)

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['_non_existent_module'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__, presubmit.OutputApi.PresubmitError)

  def testRunPythonUnitTestsFailureUpload(self):
    input_api = self.MockInputApi(None, False)
    input_api.unittest = mock.MagicMock(unittest)
    input_api.cStringIO = mock.MagicMock(presubmit.cStringIO)
    subprocess.Popen().returncode = 1  # pylint: disable=no-value-for-parameter
    presubmit.sigint_handler.wait.return_value = ('foo', None)

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
                      presubmit.OutputApi.PresubmitNotifyResult)
    self.assertEqual('test_module (0.00s) failed\nfoo', results[0]._message)

  def testRunPythonUnitTestsFailureCommitting(self):
    input_api = self.MockInputApi(None, True)
    subprocess.Popen().returncode = 1  # pylint: disable=no-value-for-parameter
    presubmit.sigint_handler.wait.return_value = ('foo', None)

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__, presubmit.OutputApi.PresubmitError)
    self.assertEqual('test_module (0.00s) failed\nfoo', results[0]._message)

  def testRunPythonUnitTestsSuccess(self):
    input_api = self.MockInputApi(None, False)
    input_api.cStringIO = mock.MagicMock(presubmit.cStringIO)
    input_api.unittest = mock.MagicMock(unittest)
    subprocess.Popen().returncode = 0  # pylint: disable=no-value-for-parameter
    presubmit.sigint_handler.wait.return_value = ('', None)

    presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    results = input_api.thread_pool.RunAsync()
    self.assertEqual(results, [])

  def testCannedRunPylint(self):
    input_api = self.MockInputApi(None, True)
    input_api.environ = mock.MagicMock(os.environ)
    input_api.environ.copy.return_value = {}
    input_api.AffectedSourceFiles.return_value = True
    input_api.PresubmitLocalPath.return_value = '/foo'
    input_api.os_walk.return_value = [('/foo', [], ['file1.py'])]

    process = mock.Mock()
    process.returncode = 0
    subprocess.Popen.return_value = process
    presubmit.sigint_handler.wait.return_value = ('', None)

    pylint = os.path.join(_ROOT, 'pylint')
    pylintrc = os.path.join(_ROOT, 'pylintrc')
    env = {'PYTHONPATH': ''}

    results = presubmit_canned_checks.RunPylint(
        input_api, presubmit.OutputApi)

    self.assertEqual([], results)
    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            [pylint, '--args-on-stdin'], env=env,
            cwd='/foo', stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
            stdin=subprocess.PIPE),
        mock.call(
            [pylint, '--args-on-stdin'], env=env,
            cwd='/foo', stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
            stdin=subprocess.PIPE),
    ])
    self.assertEqual(presubmit.sigint_handler.wait.mock_calls, [
        mock.call(
            process,
            '--rcfile=%s\n--disable=all\n--enable=cyclic-import\nfile1.py'
                % pylintrc),
        mock.call(
            process,
            '--rcfile=%s\n--disable=cyclic-import\n--jobs=2\nfile1.py'
                % pylintrc),
    ])

    self.checkstdout('')

  def testCheckBuildbotPendingBuildsBad(self):
    input_api = self.MockInputApi(None, True)
    input_api.urllib2.urlopen().read.return_value = 'foo'

    results = presubmit_canned_checks.CheckBuildbotPendingBuilds(
        input_api, presubmit.OutputApi, 'uurl', 2, ('foo'))
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
        presubmit.OutputApi.PresubmitNotifyResult)

  def testCheckBuildbotPendingBuildsGood(self):
    input_api = self.MockInputApi(None, True)
    input_api.urllib2.urlopen().read.return_value = """
    {
      'b1': { 'pending_builds': [0, 1, 2, 3, 4, 5, 6, 7] },
      'foo': { 'pending_builds': [0, 1, 2, 3, 4, 5, 6, 7] },
      'b2': { 'pending_builds': [0] }
    }"""

    results = presubmit_canned_checks.CheckBuildbotPendingBuilds(
        input_api, presubmit.OutputApi, 'uurl', 2, ('foo'))
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].__class__,
        presubmit.OutputApi.PresubmitNotifyResult)

  def GetInputApiWithOWNERS(self, owners_content):
    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file.LocalPath = lambda: 'OWNERS'
    affected_file.Action = lambda: 'M'

    change = mock.MagicMock(presubmit.Change)
    change.AffectedFiles = lambda **_: [affected_file]

    input_api = self.MockInputApi(None, False)
    input_api.change = change

    os.path.exists = lambda _: True

    owners_file = presubmit.cStringIO.StringIO(owners_content)
    fopen = lambda *args: owners_file

    input_api.owners_db = owners.Database('', fopen, os.path)

    return input_api

  def testCheckOwnersFormatWorks(self):
    input_api = self.GetInputApiWithOWNERS('\n'.join([
        'set noparent',
        'per-file lalala = lemur@chromium.org',
    ]))
    self.assertEqual(
        [],
        presubmit_canned_checks.CheckOwnersFormat(
            input_api, presubmit.OutputApi)
    )

  def testCheckOwnersFormatFails(self):
    input_api = self.GetInputApiWithOWNERS('\n'.join([
        'set noparent',
        'invalid format',
    ]))
    results = presubmit_canned_checks.CheckOwnersFormat(
        input_api, presubmit.OutputApi)

    self.assertEqual(1, len(results))
    self.assertIsInstance(results[0], presubmit.OutputApi.PresubmitError)

  def AssertOwnersWorks(self, tbr=False, issue='1', approvers=None,
      reviewers=None, is_committing=True,
      response=None, uncovered_files=None, expected_output='',
      manually_specified_reviewers=None, dry_run=None,
      modified_file='foo/xyz.cc'):
    if approvers is None:
      # The set of people who lgtm'ed a change.
      approvers = set()
    if reviewers is None:
      # The set of people needed to lgtm a change. We default to
      # the same list as the people who approved it. We use 'reviewers'
      # to avoid a name collision w/ owners.py.
      reviewers = approvers
    if uncovered_files is None:
      uncovered_files = set()
    if manually_specified_reviewers is None:
      manually_specified_reviewers = []

    change = mock.MagicMock(presubmit.Change)
    change.issue = issue
    change.author_email = 'john@example.com'
    change.ReviewersFromDescription = lambda: manually_specified_reviewers
    change.TBRsFromDescription = lambda: []
    change.RepositoryRoot = lambda: None
    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    input_api = self.MockInputApi(change, False)
    input_api.gerrit = presubmit.GerritAccessor('host')

    fake_db = mock.MagicMock(owners.Database)
    fake_db.email_regexp = input_api.re.compile(owners.BASIC_EMAIL_REGEXP)
    input_api.owners_db = fake_db

    fake_finder = mock.MagicMock(owners_finder.OwnersFinder)
    fake_finder.unreviewed_files = uncovered_files
    fake_finder.print_indent = lambda: ''
    # pylint: disable=unnecessary-lambda
    fake_finder.print_comments = lambda owner: fake_finder.writeln(owner)
    input_api.owners_finder = lambda *args, **kwargs: fake_finder
    input_api.is_committing = is_committing
    input_api.tbr = tbr
    input_api.dry_run = dry_run

    affected_file.LocalPath.return_value = modified_file
    change.AffectedFiles.return_value = [affected_file]
    if not is_committing or issue or ('OWNERS' in modified_file):
      change.OriginalOwnersFiles.return_value = {}
      if issue and not response:
        response = {
          "owner": {"email": change.author_email},
          "labels": {"Code-Review": {
            u'all': [
              {
                u'email': a,
                u'value': +1
              } for a in approvers
            ],
            u'default_value': 0,
            u'values': {u' 0': u'No score',
                        u'+1': u'Looks good to me',
                        u'-1': u"I would prefer that you didn't submit this"}
          }},
          "reviewers": {"REVIEWER": [{u'email': a}] for a in approvers},
        }

      if is_committing:
        people = approvers
      else:
        people = reviewers

      if issue:
        input_api.gerrit._FetchChangeDetail = lambda _: response

      people.add(change.author_email)
      if not is_committing and uncovered_files:
        fake_db.reviewers_for.return_value = change.author_email

    output = presubmit.PresubmitOutput()
    results = presubmit_canned_checks.CheckOwners(input_api,
        presubmit.OutputApi)
    for result in results:
      result.handle(output)
    if isinstance(expected_output, re._pattern_type):
      self.assertRegexpMatches(output.getvalue(), expected_output)
    else:
      self.assertEqual(output.getvalue(), expected_output)

  def testCannedCheckOwners_DryRun(self):
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'all': [
          {
            u'email': u'ben@example.com',
            u'value': 0
          },
        ],
        u'approved': {u'email': u'ben@example.org'},
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me',
                    u'-1': u"I would prefer that you didn't submit this"}
      }},
      "reviewers": {"REVIEWER": [{u'email': u'ben@example.com'}]},
    }
    self.AssertOwnersWorks(approvers=set(),
        dry_run=True,
        response=response,
        reviewers=set(["ben@example.com"]),
        expected_output='This is a dry run, but these failures would be ' +
                        'reported on commit:\nMissing LGTM from someone ' +
                        'other than john@example.com\n')

    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
        is_committing=False,
        response=response,
        expected_output='')

  def testCannedCheckOwners_Approved(self):
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'all': [
          {
            u'email': u'john@example.com',  # self +1 :)
            u'value': 1
          },
          {
            u'email': u'ben@example.com',
            u'value': 2
          },
        ],
        u'approved': {u'email': u'ben@example.org'},
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me, but someone else must approve',
                    u'+2': u'Looks good to me, approved',
                    u'-1': u"I would prefer that you didn't submit this",
                    u'-2': u'Do not submit'}
      }},
      "reviewers": {"REVIEWER": [{u'email': u'ben@example.com'}]},
    }
    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
        response=response,
        is_committing=True,
        expected_output='')

    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
        is_committing=False,
        response=response,
        expected_output='')

    # Testing configuration with on -1..+1.
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'all': [
          {
            u'email': u'ben@example.com',
            u'value': 1
          },
        ],
        u'approved': {u'email': u'ben@example.org'},
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me',
                    u'-1': u"I would prefer that you didn't submit this"}
      }},
      "reviewers": {"REVIEWER": [{u'email': u'ben@example.com'}]},
    }
    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
        response=response,
        is_committing=True,
        expected_output='')

  def testCannedCheckOwners_NotApproved(self):
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'all': [
          {
            u'email': u'john@example.com',  # self +1 :)
            u'value': 1
          },
          {
            u'email': u'ben@example.com',
            u'value': 1
          },
        ],
        u'approved': {u'email': u'ben@example.org'},
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me, but someone else must approve',
                    u'+2': u'Looks good to me, approved',
                    u'-1': u"I would prefer that you didn't submit this",
                    u'-2': u'Do not submit'}
      }},
      "reviewers": {"REVIEWER": [{u'email': u'ben@example.com'}]},
    }
    self.AssertOwnersWorks(
        approvers=set(),
        reviewers=set(["ben@example.com"]),
        response=response,
        is_committing=True,
        expected_output=
            'Missing LGTM from someone other than john@example.com\n')

    self.AssertOwnersWorks(
        approvers=set(),
        reviewers=set(["ben@example.com"]),
        is_committing=False,
        response=response,
        expected_output='')

    # Testing configuration with on -1..+1.
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'all': [
          {
            u'email': u'ben@example.com',
            u'value': 0
          },
        ],
        u'approved': {u'email': u'ben@example.org'},
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me',
                    u'-1': u"I would prefer that you didn't submit this"}
      }},
      "reviewers": {"REVIEWER": [{u'email': u'ben@example.com'}]},
    }
    self.AssertOwnersWorks(
        approvers=set(),
        reviewers=set(["ben@example.com"]),
        response=response,
        is_committing=True,
        expected_output=
            'Missing LGTM from someone other than john@example.com\n')

  def testCannedCheckOwners_NoReviewers(self):
    response = {
      "owner": {"email": "john@example.com"},
      "labels": {"Code-Review": {
        u'default_value': 0,
        u'values': {u' 0': u'No score',
                    u'+1': u'Looks good to me',
                    u'-1': u"I would prefer that you didn't submit this"}
      }},
      "reviewers": {},
    }
    self.AssertOwnersWorks(
        approvers=set(),
        reviewers=set(),
        response=response,
        expected_output=
            'Missing LGTM from someone other than john@example.com\n')

    self.AssertOwnersWorks(
        approvers=set(),
        reviewers=set(),
        is_committing=False,
        response=response,
        expected_output='')

  def testCannedCheckOwners_NoIssueNoFiles(self):
    self.AssertOwnersWorks(issue=None,
        expected_output="OWNERS check failed: this CL has no Gerrit "
                        "change number, so we can't check it for approvals.\n")
    self.AssertOwnersWorks(issue=None, is_committing=False,
        expected_output="")

  def testCannedCheckOwners_NoIssue(self):
    self.AssertOwnersWorks(issue=None,
        uncovered_files=set(['foo']),
        expected_output="OWNERS check failed: this CL has no Gerrit "
                        "change number, so we can't check it for approvals.\n")
    self.AssertOwnersWorks(issue=None,
        is_committing=False,
        uncovered_files=set(['foo']),
        expected_output=re.compile(
            'Missing OWNER reviewers for these files:\n'
            '    foo\n', re.MULTILINE))

  def testCannedCheckOwners_NoIssueLocalReviewers(self):
    self.AssertOwnersWorks(issue=None,
        reviewers=set(['jane@example.com']),
        manually_specified_reviewers=['jane@example.com'],
        expected_output="OWNERS check failed: this CL has no Gerrit "
                        "change number, so we can't check it for approvals.\n")
    self.AssertOwnersWorks(issue=None,
        reviewers=set(['jane@example.com']),
        manually_specified_reviewers=['jane@example.com'],
        is_committing=False,
        expected_output='')

  def testCannedCheckOwners_NoIssueLocalReviewersDontInferEmailDomain(self):
    self.AssertOwnersWorks(issue=None,
        reviewers=set(['jane']),
        manually_specified_reviewers=['jane@example.com'],
        expected_output="OWNERS check failed: this CL has no Gerrit "
                        "change number, so we can't check it for approvals.\n")
    self.AssertOwnersWorks(issue=None,
        uncovered_files=set(['foo']),
        manually_specified_reviewers=['jane'],
        is_committing=False,
        expected_output=re.compile(
            'Missing OWNER reviewers for these files:\n'
            '    foo\n', re.MULTILINE))

  def testCannedCheckOwners_NoLGTM(self):
    self.AssertOwnersWorks(expected_output='Missing LGTM from someone '
                                           'other than john@example.com\n')
    self.AssertOwnersWorks(is_committing=False, expected_output='')

  def testCannedCheckOwners_OnlyOwnerLGTM(self):
    self.AssertOwnersWorks(approvers=set(['john@example.com']),
                           expected_output='Missing LGTM from someone '
                                           'other than john@example.com\n')
    self.AssertOwnersWorks(approvers=set(['john@example.com']),
                           is_committing=False,
                           expected_output='')

  def testCannedCheckOwners_TBR(self):
    self.AssertOwnersWorks(tbr=True,
        expected_output='--tbr was specified, skipping OWNERS check\n')
    self.AssertOwnersWorks(tbr=True, is_committing=False, expected_output='')

  def testCannedCheckOwners_TBROWNERSFile(self):
    self.AssertOwnersWorks(
        tbr=True,
        uncovered_files=set(['foo/OWNERS']),
        modified_file='foo/OWNERS',
        expected_output='Missing LGTM from an OWNER for these files:\n'
        '    foo/OWNERS\n'
        'TBR for OWNERS files are ignored.\n')

  def testCannedCheckOwners_TBRNonOWNERSFile(self):
    self.AssertOwnersWorks(
        tbr=True,
        uncovered_files=set(['foo/xyz.cc']),
        modified_file='foo/OWNERS',
        expected_output='--tbr was specified, skipping OWNERS check\n')

  def testCannedCheckOwners_WithoutOwnerLGTM(self):
    self.AssertOwnersWorks(uncovered_files=set(['foo']),
        expected_output='Missing LGTM from an OWNER for these files:\n'
                        '    foo\n')
    self.AssertOwnersWorks(uncovered_files=set(['foo']),
        is_committing=False,
        expected_output=re.compile(
            'Missing OWNER reviewers for these files:\n'
            '    foo\n', re.MULTILINE))

  def testCannedCheckOwners_WithLGTMs(self):
    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
                           uncovered_files=set())
    self.AssertOwnersWorks(approvers=set(['ben@example.com']),
                           is_committing=False,
                           uncovered_files=set())

  @mock.patch('__builtin__.open', mock.mock_open(read_data=''))
  def testCannedRunUnitTests(self):
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    input_api.verbose = True
    input_api.PresubmitLocalPath.return_value = self.fake_root_dir
    presubmit.sigint_handler.wait.return_value = ('', None)

    process1 = mock.Mock()
    process1.returncode = 1
    process2 = mock.Mock()
    process2.returncode = 0
    subprocess.Popen.side_effect = [process1, process2]

    unit_tests = ['allo', 'bar.py']
    results = presubmit_canned_checks.RunUnitTests(
        input_api,
        presubmit.OutputApi,
        unit_tests)
    self.assertEqual(2, len(results))
    self.assertEqual(
        presubmit.OutputApi.PresubmitNotifyResult, results[1].__class__)
    self.assertEqual(
        presubmit.OutputApi.PresubmitPromptWarning, results[0].__class__)

    cmd = ['bar.py', '--verbose']
    if input_api.platform == 'win32':
      cmd.insert(0, 'vpython.bat')
    else:
      cmd.insert(0, 'vpython')
    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            cmd, cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
        mock.call(
            ['allo', '--verbose'], cwd=self.fake_root_dir,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE),
    ])

    self.checkstdout('')

  @mock.patch('__builtin__.open', mock.mock_open())
  def testCannedRunUnitTestsPython3(self):
    open().readline.return_value = '#!/usr/bin/env python3'
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    input_api.verbose = True
    input_api.PresubmitLocalPath.return_value = self.fake_root_dir
    presubmit.sigint_handler.wait.return_value = ('', None)

    subprocesses = [
        mock.Mock(returncode=1),
        mock.Mock(returncode=0),
        mock.Mock(returncode=0),
    ]
    subprocess.Popen.side_effect = subprocesses

    unit_tests = ['allo', 'bar.py']
    results = presubmit_canned_checks.RunUnitTests(
        input_api,
        presubmit.OutputApi,
        unit_tests)
    self.assertEqual([result.__class__ for result in results], [
        presubmit.OutputApi.PresubmitPromptWarning,
        presubmit.OutputApi.PresubmitNotifyResult,
        presubmit.OutputApi.PresubmitNotifyResult,
    ])

    cmd = ['bar.py', '--verbose']
    vpython = 'vpython'
    vpython3 = 'vpython3'
    if input_api.platform == 'win32':
      vpython += '.bat'
      vpython3 += '.bat'

    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            [vpython] + cmd, cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
        mock.call(
            [vpython3] + cmd, cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
        mock.call(
            ['allo', '--verbose'], cwd=self.fake_root_dir,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE),
    ])

    self.assertEqual(presubmit.sigint_handler.wait.mock_calls, [
        mock.call(subprocesses[0], None),
        mock.call(subprocesses[1], None),
        mock.call(subprocesses[2], None),
    ])

    self.checkstdout('')

  @mock.patch('__builtin__.open', mock.mock_open())
  def testCannedRunUnitTestsDontRunOnPython2(self):
    open().readline.return_value = '#!/usr/bin/env python3'
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    input_api.verbose = True
    input_api.PresubmitLocalPath.return_value = self.fake_root_dir
    presubmit.sigint_handler.wait.return_value = ('', None)

    subprocess.Popen.side_effect = [
        mock.Mock(returncode=1),
        mock.Mock(returncode=0),
        mock.Mock(returncode=0),
    ]

    unit_tests = ['allo', 'bar.py']
    results = presubmit_canned_checks.RunUnitTests(
        input_api,
        presubmit.OutputApi,
        unit_tests,
        run_on_python2=False)
    self.assertEqual([result.__class__ for result in results], [
        presubmit.OutputApi.PresubmitPromptWarning,
        presubmit.OutputApi.PresubmitNotifyResult,
    ])

    cmd = ['bar.py', '--verbose']
    vpython3 = 'vpython3'
    if input_api.platform == 'win32':
      vpython3 += '.bat'

    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            [vpython3] + cmd, cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
        mock.call(
            ['allo', '--verbose'], cwd=self.fake_root_dir,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE),
    ])

    self.checkstdout('')

  @mock.patch('__builtin__.open', mock.mock_open())
  def testCannedRunUnitTestsDontRunOnPython3(self):
    open().readline.return_value = '#!/usr/bin/env python3'
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    input_api.verbose = True
    input_api.PresubmitLocalPath.return_value = self.fake_root_dir
    presubmit.sigint_handler.wait.return_value = ('', None)

    subprocess.Popen.side_effect = [
        mock.Mock(returncode=1),
        mock.Mock(returncode=0),
        mock.Mock(returncode=0),
    ]

    unit_tests = ['allo', 'bar.py']
    results = presubmit_canned_checks.RunUnitTests(
        input_api,
        presubmit.OutputApi,
        unit_tests,
        run_on_python3=False)
    self.assertEqual([result.__class__ for result in results], [
        presubmit.OutputApi.PresubmitPromptWarning,
        presubmit.OutputApi.PresubmitNotifyResult,
    ])

    cmd = ['bar.py', '--verbose']
    vpython = 'vpython'
    if input_api.platform == 'win32':
      vpython += '.bat'

    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            [vpython] + cmd, cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
        mock.call(
            ['allo', '--verbose'], cwd=self.fake_root_dir,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE),
    ])

    self.checkstdout('')

  def testCannedRunUnitTestsInDirectory(self):
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    input_api.verbose = True
    input_api.logging = mock.MagicMock(logging)
    input_api.PresubmitLocalPath.return_value = self.fake_root_dir
    input_api.os_listdir.return_value = ['.', '..', 'a', 'b', 'c']
    input_api.os_path.isfile = lambda x: not x.endswith('.')

    process = mock.Mock()
    process.returncode = 0
    subprocess.Popen.return_value = process
    presubmit.sigint_handler.wait.return_value = ('', None)

    results = presubmit_canned_checks.RunUnitTestsInDirectory(
        input_api,
        presubmit.OutputApi,
        'random_directory',
        whitelist=['^a$', '^b$'],
        blacklist=['a'])
    self.assertEqual(1, len(results))
    self.assertEqual(
        presubmit.OutputApi.PresubmitNotifyResult, results[0].__class__)
    self.assertEqual(subprocess.Popen.mock_calls, [
        mock.call(
            [os.path.join('random_directory', 'b'), '--verbose'],
            cwd=self.fake_root_dir, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, stdin=subprocess.PIPE),
    ])
    self.checkstdout('')

  def testPanProjectChecks(self):
    # Make sure it accepts both list and tuples.
    change = presubmit.Change(
        'foo1', 'description1', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)
    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    input_api.AffectedFiles.return_value = [affected_file]
    affected_file.NewContents.return_value = 'Hey!\nHo!\nHey!\nHo!\n\n'
    # CheckChangeHasNoTabs() calls _FindNewViolationsOfRule() which calls
    # ChangedContents().
    affected_file.ChangedContents.return_value = [
        (0, 'Hey!\n'),
        (1, 'Ho!\n'),
        (2, 'Hey!\n'),
        (3, 'Ho!\n'),
        (4, '\n')]

    affected_file.LocalPath.return_value = 'hello.py'

    # CheckingLicense() calls AffectedSourceFiles() instead of AffectedFiles().
    input_api.AffectedSourceFiles.return_value = [affected_file]
    input_api.ReadFile.return_value = 'Hey!\nHo!\nHey!\nHo!\n\n'

    results = presubmit_canned_checks.PanProjectChecks(
        input_api,
        presubmit.OutputApi,
        excluded_paths=None,
        text_files=None,
        license_header=None,
        project_name=None,
        owners_check=False)
    self.assertEqual(2, len(results))
    self.assertEqual(
        'Found line ending with white spaces in:', results[0]._message)
    self.checkstdout('')

  def testCheckCIPDManifest_file(self):
    input_api = self.MockInputApi(None, False)

    command = presubmit_canned_checks.CheckCIPDManifest(
        input_api, presubmit.OutputApi, path='/path/to/foo')
    self.assertEqual(command.cmd,
        ['cipd', 'ensure-file-verify', '-ensure-file', '/path/to/foo'])
    self.assertEqual(command.kwargs, {
        'stdin': subprocess.PIPE,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
    })

  def testCheckCIPDManifest_content(self):
    input_api = self.MockInputApi(None, False)
    input_api.verbose = True

    command = presubmit_canned_checks.CheckCIPDManifest(
        input_api, presubmit.OutputApi, content='manifest_content')
    self.assertEqual(command.cmd,
        ['cipd', 'ensure-file-verify', '-log-level', 'debug', '-ensure-file=-'])
    self.assertEqual(command.stdin, 'manifest_content')
    self.assertEqual(command.kwargs, {
        'stdin': subprocess.PIPE,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
    })

  def testCheckCIPDPackages(self):
    content = '\n'.join([
        '$VerifiedPlatform foo-bar',
        '$VerifiedPlatform baz-qux',
        'foo/bar/baz/${platform} version:ohaithere',
        'qux version:kthxbye',
    ])

    input_api = self.MockInputApi(None, False)

    command = presubmit_canned_checks.CheckCIPDPackages(
        input_api, presubmit.OutputApi,
        platforms=['foo-bar', 'baz-qux'],
        packages={
          'foo/bar/baz/${platform}': 'version:ohaithere',
          'qux': 'version:kthxbye',
        })
    self.assertEqual(command.cmd,
        ['cipd', 'ensure-file-verify', '-ensure-file=-'])
    self.assertEqual(command.stdin, content)
    self.assertEqual(command.kwargs, {
        'stdin': subprocess.PIPE,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
    })

  def testCheckCIPDClientDigests(self):
    input_api = self.MockInputApi(None, False)
    input_api.verbose = True

    command = presubmit_canned_checks.CheckCIPDClientDigests(
        input_api, presubmit.OutputApi, client_version_file='ver')
    self.assertEqual(command.cmd, [
      'cipd', 'selfupdate-roll', '-check', '-version-file', 'ver',
      '-log-level', 'debug',
    ])

  def testCannedCheckVPythonSpec(self):
    change = presubmit.Change('a', 'b', self.fake_root_dir, None, 0, 0, None)
    input_api = self.MockInputApi(change, False)

    affected_file = mock.MagicMock(presubmit.GitAffectedFile)
    affected_file.AbsoluteLocalPath.return_value = '/path1/to/.vpython'
    input_api.AffectedTestableFiles.return_value = [affected_file]

    commands = presubmit_canned_checks.CheckVPythonSpec(
        input_api, presubmit.OutputApi)
    self.assertEqual(len(commands), 1)
    self.assertEqual(commands[0].name, 'Verify /path1/to/.vpython')
    self.assertEqual(commands[0].cmd, [
      'vpython',
      '-vpython-spec', '/path1/to/.vpython',
      '-vpython-tool', 'verify'
    ])
    self.assertDictEqual(
        commands[0].kwargs,
        {
            'stderr': input_api.subprocess.STDOUT,
            'stdout': input_api.subprocess.PIPE,
            'stdin': input_api.subprocess.PIPE,
        })
    self.assertEqual(commands[0].message, presubmit.OutputApi.PresubmitError)
    self.assertIsNone(commands[0].info)


if __name__ == '__main__':
  import unittest
  unittest.main()
