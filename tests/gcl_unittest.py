#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gcl.py."""

import StringIO
import os
import sys
import unittest

# Local imports
import gcl


class GclTestsBase(unittest.TestCase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    def RunShellMock(filename):
      return filename
    self._RunShell = gcl.RunShell
    gcl.RunShell = RunShellMock

  def tearDown(self):
    gcl.RunShell = self._RunShell

  def compareMembers(self, object, members):
    """If you add a member, be sure to add the relevant test!"""
    # Skip over members starting with '_' since they are usually not meant to
    # be for public use.
    actual_members = [x for x in sorted(dir(object))
                      if not x.startswith('_')]
    expected_members = sorted(members)
    if actual_members != expected_members:
      diff = ([i for i in actual_members if i not in expected_members] +
              [i for i in expected_members if i not in actual_members])
      print diff
    self.assertEqual(actual_members, expected_members)


class GclUnittest(GclTestsBase):
  """General gcl.py tests."""
  def testMembersChanged(self):
    members = [
      'CODEREVIEW_SETTINGS', 'CODEREVIEW_SETTINGS_FILE', 'CPP_EXTENSIONS',
      'Change', 'ChangeInfo', 'Changes', 'Commit', 'DoPresubmitChecks',
      'ErrorExit', 'GenerateChangeName', 'GenerateDiff', 'GetCLs',
      'GetChangelistInfoFile', 'GetCodeReviewSetting', 'GetEditor',
      'GetFilesNotInCL', 'GetInfoDir', 'GetIssueDescription',
      'GetModifiedFiles', 'GetRepositoryRoot', 'GetSVNStatus',
      'GetSVNFileProperty', 'Help', 'IGNORE_PATHS', 'IsSVNMoved', 'IsTreeOpen',
      'Lint', 'LoadChangelistInfo', 'LoadChangelistInfoForMultiple',
      'MISSING_TEST_MSG', 'Opened', 'PresubmitCL', 'ReadFile',
      'RunShell',
      'RunShellWithReturnCode', 'SEPARATOR', 'SendToRietveld', 'TryChange',
      'UnknownFiles', 'UploadCL', 'Warn', 'WriteFile', 'gclient',
      'gcl_info_dir', 'getpass', 'main', 'os', 'random', 're', 'read_gcl_info',
      'repository_root', 'string', 'subprocess', 'sys', 'tempfile', 'upload',
      'urllib2', 'xml',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl, members)

  def testGetSVNStatus(self):
    def RunShellMock(command):
      return r"""<?xml version="1.0"?>
<status>
<target path=".">
<entry path="unversionned_file.txt">
<wc-status props="none" item="unversioned"></wc-status>
</entry>
<entry path="build\internal\essential.vsprops">
<wc-status props="normal" item="modified" revision="14628">
<commit revision="13818">
<author>ajwong@chromium.org</author>
<date>2009-04-16T00:42:06.872358Z</date>
</commit>
</wc-status>
</entry>
<entry path="chrome\app\d">
<wc-status props="none" copied="true" tree-conflicted="true" item="added">
</wc-status>
</entry>
<entry path="chrome\app\DEPS">
<wc-status props="modified" item="modified" revision="14628">
<commit revision="1279">
<author>brettw@google.com</author>
<date>2008-08-23T17:16:42.090152Z</date>
</commit>
</wc-status>
</entry>
<entry path="scripts\master\factory\gclient_factory.py">
<wc-status props="normal" item="conflicted" revision="14725">
<commit revision="14633">
<author>nsylvain@chromium.org</author>
<date>2009-04-27T19:37:17.977400Z</date>
</commit>
</wc-status>
</entry>
</target>
</status>
"""
    # GclTestsBase.tearDown will restore the original.
    gcl.RunShell = RunShellMock
    info = gcl.GetSVNStatus('.')
    expected = [
      ('?      ', 'unversionned_file.txt'),
      ('M      ', 'build\\internal\\essential.vsprops'),
      ('A  +   ', 'chrome\\app\\d'),
      ('MM     ', 'chrome\\app\\DEPS'),
      ('C      ', 'scripts\\master\\factory\\gclient_factory.py'),
    ]
    self.assertEquals(sorted(info), sorted(expected))

  def testGetSVNStatusEmpty(self):
    def RunShellMock(command):
      return r"""<?xml version="1.0"?>
<status>
<target
   path="perf">
</target>
</status>
"""
    # GclTestsBase.tearDown will restore the original.
    gcl.RunShell = RunShellMock
    info = gcl.GetSVNStatus(None)
    self.assertEquals(info, [])

  def testHelp(self):
    old_stdout = sys.stdout
    try:
      dummy = StringIO.StringIO()
      gcl.sys.stdout = dummy
      gcl.Help()
      self.assertEquals(len(dummy.getvalue()), 1718)
    finally:
      gcl.sys.stdout = old_stdout

  def testGetRepositoryRoot(self):
    try:
      def RunShellMock(filename):
        return '<?xml version="1.0"?>\n<info>'
      gcl.RunShell = RunShellMock
      gcl.GetRepositoryRoot()
    except Exception,e:
      self.assertEquals(e.args[0], "gcl run outside of repository")
    pass


class ChangeInfoUnittest(GclTestsBase):
  def testChangeInfoMembers(self):
    members = [
      'CloseIssue', 'Delete', 'FileList', 'MissingTests', 'Save',
      'UpdateRietveldDescription', 'description', 'files', 'issue', 'name',
      'patch'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl.ChangeInfo(), members)

  def testChangeInfoBase(self):
    files = [('M', 'foo'), ('A', 'bar')]
    o = gcl.ChangeInfo('name2', 'issue2', 'description2', files)
    self.assertEquals(o.name, 'name2')
    self.assertEquals(o.issue, 'issue2')
    self.assertEquals(o.description, 'description2')
    self.assertEquals(o.files, files)
    self.assertEquals(o.patch, None)
    self.assertEquals(o.FileList(), ['foo', 'bar'])


if __name__ == '__main__':
  unittest.main()
