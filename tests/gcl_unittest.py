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
import __init__
import gcl
mox = __init__.mox


class GclTestsBase(unittest.TestCase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    self.mox = mox.Mox()
    def RunShellMock(filename):
      return filename
    self._RunShell = gcl.RunShell
    gcl.RunShell = RunShellMock
    self._gcl_gclient_CaptureSVNInfo = gcl.gclient.CaptureSVNInfo
    gcl.gclient.CaptureSVNInfo = self.mox.CreateMockAnything()
    self._gcl_os_getcwd = gcl.os.getcwd
    gcl.os.getcwd = self.mox.CreateMockAnything()

  def tearDown(self):
    gcl.RunShell = self._RunShell
    gcl.gclient.CaptureSVNInfo = self._gcl_gclient_CaptureSVNInfo
    gcl.os.getcwd = self._gcl_os_getcwd

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
      'ErrorExit', 'FILES_CACHE', 'GenerateChangeName', 'GenerateDiff',
      'GetCacheDir', 'GetCachedFile', 'GetChangesDir', 'GetCLs',
      'GetChangelistInfoFile', 'GetCodeReviewSetting', 'GetEditor',
      'GetFilesNotInCL', 'GetInfoDir', 'GetIssueDescription',
      'GetModifiedFiles', 'GetRepositoryRoot',
      'GetSVNFileProperty', 'Help', 'IGNORE_PATHS', 'IsSVNMoved', 'IsTreeOpen',
      'Lint', 'LoadChangelistInfo', 'LoadChangelistInfoForMultiple',
      'MISSING_TEST_MSG', 'Opened', 'PresubmitCL', 'ReadFile',
      'REPOSITORY_ROOT', 'RunShell',
      'RunShellWithReturnCode', 'SEPARATOR', 'SendToRietveld', 'TryChange',
      'UnknownFiles', 'UploadCL', 'Warn', 'WriteFile',
      'gclient', 'getpass', 'main', 'os', 'random', 're',
      'shutil', 'string', 'subprocess', 'sys', 'tempfile',
      'upload', 'urllib2', 'xml',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl, members)


  def testHelp(self):
    old_stdout = sys.stdout
    try:
      dummy = StringIO.StringIO()
      gcl.sys.stdout = dummy
      gcl.Help()
      self.assertEquals(len(dummy.getvalue()), 1815)
    finally:
      gcl.sys.stdout = old_stdout

  def testGetRepositoryRootNone(self):
    gcl.REPOSITORY_ROOT = None
    gcl.os.getcwd().AndReturn("/bleh/prout")
    result = {
      "Repository Root": ""
    }
    gcl.gclient.CaptureSVNInfo("/bleh/prout", print_error=False).AndReturn(
        result)
    self.mox.ReplayAll()
    self.assertRaises(Exception, gcl.GetRepositoryRoot)
    self.mox.VerifyAll()

  def testGetRepositoryRootGood(self):
    gcl.REPOSITORY_ROOT = None
    root_path = os.path.join('bleh', 'prout', 'pouet')
    gcl.os.getcwd().AndReturn(root_path)
    result1 = { "Repository Root": "Some root" }
    gcl.gclient.CaptureSVNInfo(root_path, print_error=False).AndReturn(result1)
    gcl.os.getcwd().AndReturn(root_path)
    results2 = { "Repository Root": "A different root" }
    gcl.gclient.CaptureSVNInfo(
        os.path.dirname(root_path),
        print_error=False).AndReturn(results2)
    self.mox.ReplayAll()
    self.assertEquals(gcl.GetRepositoryRoot(), root_path)
    self.mox.VerifyAll()


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


class UploadCLUnittest(GclTestsBase):
  def setUp(self):
    GclTestsBase.setUp(self)
    self._os_chdir = gcl.os.chdir
    gcl.os.chdir = self.mox.CreateMockAnything()
    self._os_close = gcl.os.close
    gcl.os.close = self.mox.CreateMockAnything()
    self._os_remove = gcl.os.remove
    gcl.os.remove = self.mox.CreateMockAnything()
    self._os_write = gcl.os.write
    gcl.os.write = self.mox.CreateMockAnything()
    self._tempfile = gcl.tempfile
    gcl.tempfile = self.mox.CreateMockAnything()
    self._upload_RealMain = gcl.upload.RealMain
    gcl.upload.RealMain = self.mox.CreateMockAnything()
    self._DoPresubmitChecks = gcl.DoPresubmitChecks
    gcl.DoPresubmitChecks = self.mox.CreateMockAnything()
    self._GenerateDiff = gcl.GenerateDiff
    gcl.GenerateDiff = self.mox.CreateMockAnything()
    self._GetCodeReviewSetting = gcl.GetCodeReviewSetting
    gcl.GetCodeReviewSetting = self.mox.CreateMockAnything()
    self._GetRepositoryRoot = gcl.GetRepositoryRoot
    gcl.GetRepositoryRoot = self.mox.CreateMockAnything()
    self._SendToRietveld = gcl.SendToRietveld
    gcl.SendToRietveld = self.mox.CreateMockAnything()
    self._TryChange = gcl.TryChange
    gcl.TryChange = self.mox.CreateMockAnything()
    
  def tearDown(self):
    GclTestsBase.tearDown(self)
    gcl.os.chdir = self._os_chdir
    gcl.os.close = self._os_close
    gcl.os.remove = self._os_remove
    gcl.os.write = self._os_write
    gcl.tempfile = self._tempfile
    gcl.upload.RealMain = self._upload_RealMain
    gcl.DoPresubmitChecks = self._DoPresubmitChecks
    gcl.GenerateDiff = self._GenerateDiff
    gcl.GetCodeReviewSetting = self._GetCodeReviewSetting
    gcl.GetRepositoryRoot = self._GetRepositoryRoot
    gcl.SendToRietveld = self._SendToRietveld
    gcl.TryChange = self._TryChange

  def testNew(self):
    change_info = gcl.ChangeInfo('naame', 'iissue', 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = ['--foo=bar']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, committing=False).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(gcl.GetRepositoryRoot().AndReturn(None))
    gcl.GenerateDiff(change_info.FileList())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server', '--foo=bar',
        "--message=''", '--issue=iissue'], change_info.patch).AndReturn(("1",
                                                                         "2"))
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.GetCodeReviewSetting('TRY_ON_UPLOAD').AndReturn('True')
    gcl.TryChange(change_info,
                  ['--issue', '1', '--patchset', '2'],
                  swallow_exception=True)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)
    self.mox.VerifyAll()

  def testServerOverride(self):
    change_info = gcl.ChangeInfo('naame', '', 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = ['--server=a']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, committing=False).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.tempfile.mkstemp(text=True).AndReturn((42, 'descfile'))
    gcl.os.write(42, change_info.description)
    gcl.os.close(42)
    gcl.GetCodeReviewSetting('CC_LIST')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(gcl.GetRepositoryRoot().AndReturn(None))
    gcl.GenerateDiff(change_info.FileList())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server', '--server=a',
        "--description_file=descfile",
        "--message=deescription"], change_info.patch).AndReturn(("1", "2"))
    gcl.os.remove('descfile')
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    #gcl.GetCodeReviewSetting('TRY_ON_UPLOAD').AndReturn('True')
    #gcl.TryChange(change_info,
    #              ['--issue', '1', '--patchset', '2'],
    #              swallow_exception=True)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)
    self.mox.VerifyAll()

  def testNoTry(self):
    change_info = gcl.ChangeInfo('naame', '', 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = ['--no-try']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, committing=False).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.tempfile.mkstemp(text=True).AndReturn((42, 'descfile'))
    gcl.os.write(42, change_info.description)
    gcl.os.close(42)
    gcl.GetCodeReviewSetting('CC_LIST')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(gcl.GetRepositoryRoot().AndReturn(None))
    gcl.GenerateDiff(change_info.FileList())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server',
        "--description_file=descfile",
        "--message=deescription"], change_info.patch).AndReturn(("1", "2"))
    gcl.os.remove('descfile')
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)
    self.mox.VerifyAll()

  def testNormal(self):
    change_info = gcl.ChangeInfo('naame', '', 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = []
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, committing=False).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.tempfile.mkstemp(text=True).AndReturn((42, 'descfile'))
    gcl.os.write(42, change_info.description)
    gcl.os.close(42)
    gcl.GetCodeReviewSetting('CC_LIST')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(gcl.GetRepositoryRoot().AndReturn(None))
    gcl.GenerateDiff(change_info.FileList())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server',
        "--description_file=descfile",
        "--message=deescription"], change_info.patch).AndReturn(("1", "2"))
    gcl.os.remove('descfile')
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.GetCodeReviewSetting('TRY_ON_UPLOAD').AndReturn('True')
    gcl.TryChange(change_info,
                  ['--issue', '1', '--patchset', '2'],
                  swallow_exception=True)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
