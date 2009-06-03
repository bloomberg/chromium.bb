#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gcl.py."""

import unittest

# Local imports
import __init__
import gcl
mox = __init__.mox


class GclTestsBase(mox.MoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(gcl, 'RunShell')
    self.mox.StubOutWithMock(gcl.gclient, 'CaptureSVNInfo')
    self.mox.StubOutWithMock(gcl, 'os')
    self.mox.StubOutWithMock(gcl.os, 'getcwd')

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
      'GetSVNFileProperty', 'Help', 'IGNORE_PATHS', 'IsSVNMoved',
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
    self.mox.StubOutWithMock(gcl.sys, 'stdout')
    gcl.sys.stdout.write(mox.StrContains('GCL is a wrapper for Subversion'))
    gcl.sys.stdout.write('\n')
    self.mox.ReplayAll()
    gcl.Help()

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

  def testGetRepositoryRootGood(self):
    gcl.REPOSITORY_ROOT = None
    root_path = gcl.os.path.join('bleh', 'prout', 'pouet')
    gcl.os.getcwd().AndReturn(root_path)
    result1 = { "Repository Root": "Some root" }
    gcl.gclient.CaptureSVNInfo(root_path, print_error=False).AndReturn(result1)
    gcl.os.getcwd().AndReturn(root_path)
    results2 = { "Repository Root": "A different root" }
    gcl.gclient.CaptureSVNInfo(
        gcl.os.path.dirname(root_path),
        print_error=False).AndReturn(results2)
    self.mox.ReplayAll()
    self.assertEquals(gcl.GetRepositoryRoot(), root_path)


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
    self.mox.StubOutWithMock(gcl.os, 'chdir')
    self.mox.StubOutWithMock(gcl.os, 'close')
    self.mox.StubOutWithMock(gcl.os, 'remove')
    self.mox.StubOutWithMock(gcl.os, 'write')
    self.mox.StubOutWithMock(gcl, 'tempfile')
    self.mox.StubOutWithMock(gcl.upload, 'RealMain')
    self.mox.StubOutWithMock(gcl, 'DoPresubmitChecks')
    self.mox.StubOutWithMock(gcl, 'GenerateDiff')
    self.mox.StubOutWithMock(gcl, 'GetCodeReviewSetting')
    self.mox.StubOutWithMock(gcl, 'GetRepositoryRoot')
    self.mox.StubOutWithMock(gcl, 'SendToRietveld')
    self.mox.StubOutWithMock(gcl, 'TryChange')

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
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)

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

  def testNormal(self):
    change_info = gcl.ChangeInfo('naame', '', 'deescription',
                                 ['aa', 'bb'])
    self.mox.StubOutWithMock(change_info, 'Save')
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


if __name__ == '__main__':
  unittest.main()
