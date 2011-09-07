#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for patch.py."""

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

import patch


SVN_PATCH = (
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
    ' \n')


GIT_PATCH = (
    'diff --git a/chrome/file.cc b/chrome/file.cc\n'
    'index 0e4de76..8320059 100644\n'
    '--- a/chrome/file.cc\n'
    '+++ b/chrome/file.cc\n'
    '@@ -3,6 +3,7 @@ bb\n'
    ' ccc\n'
    ' dd\n'
    ' e\n'
    '+FOO!\n'
    ' ff\n'
    ' ggg\n'
    ' hh\n')


# http://codereview.chromium.org/download/issue6368055_22_29.diff
GIT_DELETE = (
    'Index: tools/clang_check/README.chromium\n'
    'diff --git a/tools/clang_check/README.chromium '
        'b/tools/clang_check/README.chromium\n'
    'deleted file mode 100644\n'
    'index fcaa7e0e94bb604a026c4f478fecb1c5796f5413..'
        '0000000000000000000000000000000000000000\n'
    '--- a/tools/clang_check/README.chromium\n'
    '+++ /dev/null\n'
    '@@ -1,9 +0,0 @@\n'
    '-These are terrible, terrible hacks.\n'
    '-\n'
    '-They are meant to be temporary. clang currently doesn\'t allow running a '
        'plugin\n'
    '-AND doing the normal codegen process. We want our syntax complaining '
        'plugins to\n'
    '-run during normal compile, but for now, you can user run_plugin.sh to '
        'hack the\n'
    '-build system to do a syntax check.\n'
    '-\n'
    '-Also see http://code.google.com/p/chromium/wiki/WritingClangPlugins\n'
    '-\n')


# http://codereview.chromium.org/download/issue6250123_3013_6010.diff
GIT_RENAME_PARTIAL = (
    'Index: chrome/browser/chromeos/views/webui_menu_widget.h\n'
    'diff --git a/chrome/browser/chromeos/views/domui_menu_widget.h '
        'b/chrome/browser/chromeos/views/webui_menu_widget.h\n'
    'similarity index 79%\n'
    'rename from chrome/browser/chromeos/views/domui_menu_widget.h\n'
    'rename to chrome/browser/chromeos/views/webui_menu_widget.h\n'
    'index 095d4c474fd9718f5aebfa41a1ccb2d951356d41..'
        '157925075434b590e8acaaf605a64f24978ba08b 100644\n'
    '--- a/chrome/browser/chromeos/views/domui_menu_widget.h\n'
    '+++ b/chrome/browser/chromeos/views/webui_menu_widget.h\n'
    '@@ -1,9 +1,9 @@\n'
    '-// Copyright (c) 2010 The Chromium Authors. All rights reserved.\n'
    '+// Copyright (c) 2011 The Chromium Authors. All rights reserved.\n'
    ' // Use of this source code is governed by a BSD-style license that can be'
        '\n'
    ' // found in the LICENSE file.\n'
    ' \n'
    '-#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_DOMUI_MENU_WIDGET_H_\n'
    '-#define CHROME_BROWSER_CHROMEOS_VIEWS_DOMUI_MENU_WIDGET_H_\n'
    '+#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_WEBUI_MENU_WIDGET_H_\n'
    '+#define CHROME_BROWSER_CHROMEOS_VIEWS_WEBUI_MENU_WIDGET_H_\n'
    ' #pragma once\n'
    ' \n'
    ' #include <string>\n')


# http://codereview.chromium.org/download/issue6287022_3001_4010.diff
GIT_RENAME = (
    'Index: tools/run_local_server.sh\n'
    'diff --git a/tools/run_local_server.py b/tools/run_local_server.sh\n'
    'similarity index 100%\n'
    'rename from tools/run_local_server.py\n'
    'rename to tools/run_local_server.sh\n')


GIT_COPY = (
    'diff --git a/PRESUBMIT.py b/pp\n'
    'similarity index 100%\n'
    'copy from PRESUBMIT.py\n'
    'copy to pp\n')


GIT_NEW = (
    'diff --git a/foo b/foo\n'
    'new file mode 100644\n'
    'index 0000000..5716ca5\n'
    '--- /dev/null\n'
    '+++ b/foo\n'
    '@@ -0,0 +1 @@\n'
    '+bar\n')


NEW = (
    '--- /dev/null\n'
    '+++ foo\n'
    '@@ -0,0 +1 @@\n'
    '+bar\n')


DELETE = (
    '--- tools/clang_check/README.chromium\n'
    '+++ /dev/null\n'
    '@@ -1,1 +0,0 @@\n'
    '-bar\n')


class PatchTest(unittest.TestCase):
  def _check_patch(self,
      p,
      filename,
      diff,
      is_binary=False,
      is_delete=False,
      is_git_diff=False,
      is_new=False,
      patchlevel=0,
      svn_properties=None):
    svn_properties = svn_properties or []
    self.assertEquals(p.filename, filename)
    self.assertEquals(p.is_binary, is_binary)
    self.assertEquals(p.is_delete, is_delete)
    if hasattr(p, 'is_git_diff'):
      self.assertEquals(p.is_git_diff, is_git_diff)
    self.assertEquals(p.is_new, is_new)
    if hasattr(p, 'patchlevel'):
      self.assertEquals(p.patchlevel, patchlevel)
    if diff:
      self.assertEquals(p.get(), diff)
    if hasattr(p, 'svn_properties'):
      self.assertEquals(p.svn_properties, svn_properties)

  def testFilePatchDelete(self):
    p = patch.FilePatchDelete('foo', False)
    self._check_patch(p, 'foo', None, is_delete=True)
    try:
      p.get()
      self.fail()
    except NotImplementedError:
      pass

  def testFilePatchDeleteBin(self):
    p = patch.FilePatchDelete('foo', True)
    self._check_patch(p, 'foo', None, is_delete=True, is_binary=True)
    try:
      p.get()
      self.fail()
    except NotImplementedError:
      pass

  def testFilePatchBinary(self):
    p = patch.FilePatchBinary('foo', 'data', [], is_new=False)
    self._check_patch(p, 'foo', 'data', is_binary=True)

  def testFilePatchBinaryNew(self):
    p = patch.FilePatchBinary('foo', 'data', [], is_new=True)
    self._check_patch(p, 'foo', 'data', is_binary=True, is_new=True)

  def testFilePatchDiff(self):
    p = patch.FilePatchDiff('chrome/file.cc', SVN_PATCH, [])
    self._check_patch(p, 'chrome/file.cc', SVN_PATCH)

  def testFilePatchDiffHeaderMode(self):
    diff = (
        'diff --git a/git_cl/git-cl b/git_cl/git-cl\n'
        'old mode 100644\n'
        'new mode 100755\n')
    p = patch.FilePatchDiff('git_cl/git-cl', diff, [])
    self._check_patch(
        p, 'git_cl/git-cl', diff, is_git_diff=True, patchlevel=1,
        svn_properties=[('svn:executable', '*')])

  def testFilePatchDiffHeaderModeIndex(self):
    diff = (
        'Index: Junk\n'
        'diff --git a/git_cl/git-cl b/git_cl/git-cl\n'
        'old mode 100644\n'
        'new mode 100755\n')
    p = patch.FilePatchDiff('git_cl/git-cl', diff, [])
    self._check_patch(
        p, 'git_cl/git-cl', diff, is_git_diff=True, patchlevel=1,
        svn_properties=[('svn:executable', '*')])

  def testFilePatchDiffSvnNew(self):
    # The code path is different for git and svn.
    p = patch.FilePatchDiff('foo', NEW, [])
    self._check_patch(p, 'foo', NEW, is_new=True)

  def testFilePatchDiffGitNew(self):
    # The code path is different for git and svn.
    p = patch.FilePatchDiff('foo', GIT_NEW, [])
    self._check_patch(
        p, 'foo', GIT_NEW, is_new=True, is_git_diff=True, patchlevel=1)

  def testFilePatchDiffBad(self):
    try:
      patch.FilePatchDiff('foo', 'data', [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffEmpty(self):
    try:
      patch.FilePatchDiff('foo', '', [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffNone(self):
    try:
      patch.FilePatchDiff('foo', None, [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchBadDiffName(self):
    try:
      patch.FilePatchDiff('foo', SVN_PATCH, [])
      self.fail()
    except patch.UnsupportedPatchFormat, e:
      self.assertEquals(
          "Can't process patch for file foo.\nUnexpected diff: chrome/file.cc.",
          str(e))

  def testFilePatchDiffBadHeader(self):
    try:
      diff = (
        '+++ b/foo\n'
        '@@ -0,0 +1 @@\n'
        '+bar\n')
      patch.FilePatchDiff('foo', diff, [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffBadGitHeader(self):
    try:
      diff = (
        'diff --git a/foo b/foo\n'
        '+++ b/foo\n'
        '@@ -0,0 +1 @@\n'
        '+bar\n')
      patch.FilePatchDiff('foo', diff, [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffBadHeaderReversed(self):
    try:
      diff = (
        '+++ b/foo\n'
        '--- b/foo\n'
        '@@ -0,0 +1 @@\n'
        '+bar\n')
      patch.FilePatchDiff('foo', diff, [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffGitBadHeaderReversed(self):
    try:
      diff = (
        'diff --git a/foo b/foo\n'
        '+++ b/foo\n'
        '--- b/foo\n'
        '@@ -0,0 +1 @@\n'
        '+bar\n')
      patch.FilePatchDiff('foo', diff, [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testFilePatchDiffInvalidGit(self):
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

  def testValidSvn(self):
    # pylint: disable=R0201
    # Method could be a function
    # Should not throw.
    p = patch.FilePatchDiff('chrome/file.cc', SVN_PATCH, [])
    lines = SVN_PATCH.splitlines(True)
    header = ''.join(lines[:4])
    hunks = ''.join(lines[4:])
    self.assertEquals(header, p.diff_header)
    self.assertEquals(hunks, p.diff_hunks)
    self.assertEquals(SVN_PATCH, p.get())

  def testValidSvnNew(self):
    text = '--- /dev/null\t2\n+++ chrome/file.cc\tfoo\n'
    p = patch.FilePatchDiff('chrome/file.cc', text, [])
    self.assertEquals(text, p.diff_header)
    self.assertEquals('', p.diff_hunks)
    self.assertEquals(text, p.get())

  def testValidSvnDelete(self):
    text = '--- chrome/file.cc\tbar\n+++ /dev/null\tfoo\n'
    p = patch.FilePatchDiff('chrome/file.cc', text, [])
    self.assertEquals(text, p.diff_header)
    self.assertEquals('', p.diff_hunks)
    self.assertEquals(text, p.get())

  def testRelPath(self):
    patches = patch.PatchSet([
        patch.FilePatchDiff('chrome/file.cc', SVN_PATCH, []),
        patch.FilePatchDiff(
            'tools\\clang_check/README.chromium', GIT_DELETE, []),
        patch.FilePatchDiff('tools/run_local_server.sh', GIT_RENAME, []),
        patch.FilePatchDiff(
            'chrome\\browser/chromeos/views/webui_menu_widget.h',
            GIT_RENAME_PARTIAL, []),
        patch.FilePatchDiff('pp', GIT_COPY, []),
        patch.FilePatchDiff('foo', GIT_NEW, []),
        patch.FilePatchDelete('other/place/foo', True),
        patch.FilePatchBinary('bar', 'data', [], is_new=False),
    ])
    expected = [
        'chrome/file.cc', 'tools/clang_check/README.chromium',
        'tools/run_local_server.sh',
        'chrome/browser/chromeos/views/webui_menu_widget.h', 'pp', 'foo',
        'other/place/foo', 'bar']
    self.assertEquals(expected, patches.filenames)
    orig_name = patches.patches[0].filename
    patches.set_relpath(os.path.join('a', 'bb'))
    expected = [os.path.join('a', 'bb', x) for x in expected]
    self.assertEquals(expected, patches.filenames)
    # Make sure each header is updated accordingly.
    header = []
    new_name = os.path.join('a', 'bb', orig_name)
    for line in SVN_PATCH.splitlines(True):
      if line.startswith('@@'):
        break
      if line[:3] in ('---', '+++', 'Ind'):
        line = line.replace(orig_name, new_name)
      header.append(line)
    header = ''.join(header)
    self.assertEquals(header, patches.patches[0].diff_header)

  def testRelPathBad(self):
    patches = patch.PatchSet([
        patch.FilePatchDiff('chrome\\file.cc', SVN_PATCH, []),
        patch.FilePatchDelete('other\\place\\foo', True),
    ])
    try:
      patches.set_relpath('..')
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testRelPathEmpty(self):
    patches = patch.PatchSet([
        patch.FilePatchDiff('chrome\\file.cc', SVN_PATCH, []),
        patch.FilePatchDelete('other\\place\\foo', True),
    ])
    patches.set_relpath('')
    self.assertEquals(
        ['chrome/file.cc', 'other/place/foo'],
        [f.filename for f in patches])

  def testBackSlash(self):
    mangled_patch = SVN_PATCH.replace('chrome/', 'chrome\\')
    patches = patch.PatchSet([
        patch.FilePatchDiff('chrome\\file.cc', mangled_patch, []),
        patch.FilePatchDelete('other\\place\\foo', True),
    ])
    expected = ['chrome/file.cc', 'other/place/foo']
    self.assertEquals(expected, patches.filenames)
    self.assertEquals(SVN_PATCH, patches.patches[0].get())

  def testDelete(self):
    p = patch.FilePatchDiff('tools/clang_check/README.chromium', DELETE, [])
    self._check_patch(
        p, 'tools/clang_check/README.chromium', DELETE, is_delete=True)

  def testGitDelete(self):
    p = patch.FilePatchDiff('tools/clang_check/README.chromium', GIT_DELETE, [])
    self._check_patch(
        p, 'tools/clang_check/README.chromium', GIT_DELETE, is_delete=True,
        is_git_diff=True, patchlevel=1)

  def testGitRename(self):
    p = patch.FilePatchDiff('tools/run_local_server.sh', GIT_RENAME, [])
    self._check_patch(p, 'tools/run_local_server.sh', GIT_RENAME,
        is_git_diff=True, patchlevel=1)

  def testGitRenamePartial(self):
    p = patch.FilePatchDiff(
        'chrome/browser/chromeos/views/webui_menu_widget.h',
        GIT_RENAME_PARTIAL, [])
    self._check_patch(
        p, 'chrome/browser/chromeos/views/webui_menu_widget.h',
        GIT_RENAME_PARTIAL, is_git_diff=True, patchlevel=1)

  def testGitCopy(self):
    p = patch.FilePatchDiff('pp', GIT_COPY, [])
    self._check_patch(p, 'pp', GIT_COPY, is_git_diff=True, patchlevel=1)

  def testOnlyHeader(self):
    diff = '--- file_a\n+++ file_a\n'
    p = patch.FilePatchDiff('file_a', diff, [])
    self._check_patch(p, 'file_a', diff)

  def testSmallest(self):
    diff = '--- file_a\n+++ file_a\n@@ -0,0 +1 @@\n+foo\n'
    p = patch.FilePatchDiff('file_a', diff, [])
    self._check_patch(p, 'file_a', diff)

  def testInverted(self):
    try:
      patch.FilePatchDiff(
        'file_a', '+++ file_a\n--- file_a\n@@ -0,0 +1 @@\n+foo\n', [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testInvertedOnlyHeader(self):
    try:
      patch.FilePatchDiff('file_a', '+++ file_a\n--- file_a\n', [])
      self.fail()
    except patch.UnsupportedPatchFormat:
      pass

  def testRenameOnlyHeader(self):
    diff = '--- file_a\n+++ file_b\n'
    p = patch.FilePatchDiff('file_b', diff, [])
    # Should it be marked as new?
    self._check_patch(p, 'file_b', diff)

  def testGitCopyPartial(self):
    diff = (
        'diff --git a/wtf b/wtf2\n'
        'similarity index 98%\n'
        'copy from wtf\n'
        'copy to wtf2\n'
        'index 79fbaf3..3560689 100755\n'
        '--- a/wtf\n'
        '+++ b/wtf2\n'
        '@@ -1,4 +1,4 @@\n'
        '-#!/usr/bin/env python\n'
        '+#!/usr/bin/env python1.3\n'
        ' # Copyright (c) 2010 The Chromium Authors. All rights reserved.\n'
        ' # blah blah blah as\n'
        ' # found in the LICENSE file.\n')
    p = patch.FilePatchDiff('wtf2', diff, [])
    # Should it be marked as new?
    self._check_patch(p, 'wtf2', diff, is_git_diff=True, patchlevel=1)

  def testGitExe(self):
    diff = (
        'diff --git a/natsort_test.py b/natsort_test.py\n'
        'new file mode 100755\n'
        '--- /dev/null\n'
        '+++ b/natsort_test.py\n'
        '@@ -0,0 +1,1 @@\n'
        '+#!/usr/bin/env python\n')
    p = patch.FilePatchDiff('natsort_test.py', diff, [])
    self._check_patch(
        p, 'natsort_test.py', diff, is_new=True, is_git_diff=True, patchlevel=1,
        svn_properties=[('svn:executable', '*')])

  def testGitExe2(self):
    diff = (
        'diff --git a/natsort_test.py b/natsort_test.py\n'
        'new file mode 100644\n'
        '--- /dev/null\n'
        '+++ b/natsort_test.py\n'
        '@@ -0,0 +1,1 @@\n'
        '+#!/usr/bin/env python\n')
    p = patch.FilePatchDiff('natsort_test.py', diff, [])
    self._check_patch(
        p, 'natsort_test.py', diff, is_new=True, is_git_diff=True, patchlevel=1)


if __name__ == '__main__':
  logging.basicConfig(level=
      [logging.WARNING, logging.INFO, logging.DEBUG][
        min(2, sys.argv.count('-v'))])
  unittest.main()
