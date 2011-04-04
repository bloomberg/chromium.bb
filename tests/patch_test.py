#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for patch.py."""

import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

import patch


class PatchTest(unittest.TestCase):
  def testFilePatchDelete(self):
    c = patch.FilePatchDelete('foo', False)
    self.assertEquals(c.is_delete, True)
    self.assertEquals(c.is_binary, False)
    self.assertEquals(c.filename, 'foo')
    try:
      c.get()
      self.fail()
    except NotImplementedError:
      pass
    c = patch.FilePatchDelete('foo', True)
    self.assertEquals(c.is_delete, True)
    self.assertEquals(c.is_binary, True)
    self.assertEquals(c.filename, 'foo')
    try:
      c.get()
      self.fail()
    except NotImplementedError:
      pass

  def testFilePatchBinary(self):
    c = patch.FilePatchBinary('foo', 'data', [])
    self.assertEquals(c.is_delete, False)
    self.assertEquals(c.is_binary, True)
    self.assertEquals(c.filename, 'foo')
    self.assertEquals(c.get(), 'data')

  def testFilePatchDiff(self):
    c = patch.FilePatchDiff('foo', 'data', [])
    self.assertEquals(c.is_delete, False)
    self.assertEquals(c.is_binary, False)
    self.assertEquals(c.filename, 'foo')
    self.assertEquals(c.is_git_diff, False)
    self.assertEquals(c.patchlevel, 0)
    self.assertEquals(c.get(), 'data')
    diff = (
        'diff --git a/git_cl/git-cl b/git_cl/git-cl\n'
        'old mode 100644\n'
        'new mode 100755\n')
    c = patch.FilePatchDiff('git_cl/git-cl', diff, [])
    self.assertEquals(c.is_delete, False)
    self.assertEquals(c.is_binary, False)
    self.assertEquals(c.filename, 'git_cl/git-cl')
    self.assertEquals(c.is_git_diff, True)
    self.assertEquals(c.patchlevel, 1)
    self.assertEquals(c.get(), diff)
    diff = (
        'Index: Junk\n'
        'diff --git a/git_cl/git-cl b/git_cl/git-cl\n'
        'old mode 100644\n'
        'new mode 100755\n')
    c = patch.FilePatchDiff('git_cl/git-cl', diff, [])
    self.assertEquals(c.is_delete, False)
    self.assertEquals(c.is_binary, False)
    self.assertEquals(c.filename, 'git_cl/git-cl')
    self.assertEquals(c.is_git_diff, True)
    self.assertEquals(c.patchlevel, 1)
    self.assertEquals(c.get(), diff)

  def testInvalidFilePatchDiffGit(self):
    try:
      patch.FilePatchDiff('svn_utils_test.txt', (
        'diff --git a/tests/svn_utils_test_data/svn_utils_test.txt '
        'b/tests/svn_utils_test_data/svn_utils_test.txt\n'
        'index 0e4de76..8320059 100644\n'
        '--- a/svn_utils_test.txt\n'
        '+++ b/svn_utils_test.txt\n'
        '@@ -3,6 +3,7 @@ bb\n'
        'ccc\n'
        'dd\n'
        'e\n'
        '+FOO!\n'
        'ff\n'
        'ggg\n'
        'hh\n'),
        [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass
    try:
      patch.FilePatchDiff('svn_utils_test2.txt', (
        'diff --git a/svn_utils_test_data/svn_utils_test.txt '
        'b/svn_utils_test.txt\n'
        'index 0e4de76..8320059 100644\n'
        '--- a/svn_utils_test.txt\n'
        '+++ b/svn_utils_test.txt\n'
        '@@ -3,6 +3,7 @@ bb\n'
        'ccc\n'
        'dd\n'
        'e\n'
        '+FOO!\n'
        'ff\n'
        'ggg\n'
        'hh\n'),
        [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testInvalidFilePatchDiffSvn(self):
    try:
      patch.FilePatchDiff('svn_utils_test.txt', (
        '--- svn_utils_test.txt2\n'
        '+++ svn_utils_test.txt\n'
        '@@ -3,6 +3,7 @@ bb\n'
        'ccc\n'
        'dd\n'
        'e\n'
        '+FOO!\n'
        'ff\n'
        'ggg\n'
        'hh\n'),
        [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testValidSvn(self):
    # pylint: disable=R0201
    # Method could be a function
    # Should not throw.
    patch.FilePatchDiff('chrome/file.cc', (
        'Index: chrome/file.cc\n'
        '===================================================================\n'
        '--- chrome/file.cc\t(revision 74690)\n'
        '+++ chrome/file.cc\t(working copy)\n'
        '@@ -80,10 +80,13 @@\n'
        ' // Foo\n'
        ' // Bar\n'
        ' void foo() {\n'
        '-   return bar;\n'
        '+   return foo;\n'
        ' }\n'
        ' \n'
        ' \n'), [])
    patch.FilePatchDiff('chrome/file.cc', (
        '--- /dev/null\t2\n'
        '+++ chrome/file.cc\tfoo\n'), [])
    patch.FilePatchDiff('chrome/file.cc', (
        '--- chrome/file.cc\tbar\n'
        '+++ /dev/null\tfoo\n'), [])


if __name__ == '__main__':
  unittest.main()
