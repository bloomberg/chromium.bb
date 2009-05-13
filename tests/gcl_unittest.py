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
      'GetModifiedFiles', 'GetRepositoryRoot',
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


  def testHelp(self):
    old_stdout = sys.stdout
    try:
      dummy = StringIO.StringIO()
      gcl.sys.stdout = dummy
      gcl.Help()
      self.assertEquals(len(dummy.getvalue()), 1813)
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
