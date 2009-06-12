#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gcl.py."""

import unittest

# Local imports
import gcl
import super_mox
from super_mox import mox


class GclTestsBase(super_mox.SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    super_mox.SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(gcl, 'RunShell')
    self.mox.StubOutWithMock(gcl.gclient, 'CaptureSVNInfo')
    self.mox.StubOutWithMock(gcl, 'os')
    self.mox.StubOutWithMock(gcl.os, 'getcwd')


class GclUnittest(GclTestsBase):
  """General gcl.py tests."""
  def testMembersChanged(self):
    members = [
      'CODEREVIEW_SETTINGS', 'CODEREVIEW_SETTINGS_FILE', 'CPP_EXTENSIONS',
      'Change', 'ChangeInfo', 'Changes', 'Commit', 'DoPresubmitChecks',
      'ErrorExit', 'FILES_CACHE', 'FilterFlag', 'GenerateChangeName',
      'GenerateDiff',
      'GetCacheDir', 'GetCachedFile', 'GetChangesDir', 'GetCLs',
      'GetChangelistInfoFile', 'GetCodeReviewSetting', 'GetEditor',
      'GetFilesNotInCL', 'GetInfoDir', 'GetIssueDescription',
      'GetModifiedFiles', 'GetRepositoryRoot',
      'GetSVNFileProperty', 'Help', 'IGNORE_PATHS', 'IsSVNMoved',
      'Lint', 'LoadChangelistInfoForMultiple',
      'MISSING_TEST_MSG', 'Opened', 'OptionallyDoPresubmitChecks',
      'PresubmitCL', 'ReadFile', 'REPOSITORY_ROOT', 'RunShell',
      'RunShellWithReturnCode', 'SendToRietveld', 'TryChange',
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
  def setUp(self):
    GclTestsBase.setUp(self)
    self.mox.StubOutWithMock(gcl, 'GetChangelistInfoFile')
    self.mox.StubOutWithMock(gcl.os.path, 'exists')
    self.mox.StubOutWithMock(gcl, 'ReadFile')
    self.mox.StubOutWithMock(gcl, 'WriteFile')

  def testChangeInfoMembers(self):
    members = [
      'CloseIssue', 'Delete', 'FileList', 'Load', 'MissingTests', 'Save',
      'UpdateRietveldDescription', 'description', 'files', 'issue', 'name',
      'patch', 'patchset',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl.ChangeInfo('', 0, 0, '', None), members)

  def testChangeInfoBase(self):
    files = [('M', 'foo'), ('A', 'bar')]
    self.mox.ReplayAll()
    o = gcl.ChangeInfo('name2', '42', '53', 'description2', files)
    self.assertEquals(o.name, 'name2')
    self.assertEquals(o.issue, 42)
    self.assertEquals(o.patchset, 53)
    self.assertEquals(o.description, 'description2')
    self.assertEquals(o.files, files)
    self.assertEquals(o.patch, None)
    self.assertEquals(o.FileList(), ['foo', 'bar'])

  def testLoadWithIssue(self):
    description = ["This is some description.", "force an extra separator."]
    gcl.GetChangelistInfoFile('bleh').AndReturn('bleeeh')
    gcl.os.path.exists('bleeeh').AndReturn(True)
    gcl.ReadFile('bleeeh').AndReturn(
      gcl.ChangeInfo._SEPARATOR.join(["42,53", "G      b.cc"] + description))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo.Load('bleh', True, False)
    self.assertEquals(change_info.name, 'bleh')
    self.assertEquals(change_info.issue, 42)
    self.assertEquals(change_info.patchset, 53)
    self.assertEquals(change_info.description,
                      gcl.ChangeInfo._SEPARATOR.join(description))
    self.assertEquals(change_info.files, [('G      ', 'b.cc')])

  def testLoadEmpty(self):
    gcl.GetChangelistInfoFile('bleh').AndReturn('bleeeh')
    gcl.os.path.exists('bleeeh').AndReturn(True)
    gcl.ReadFile('bleeeh').AndReturn(
        gcl.ChangeInfo._SEPARATOR.join(["", "", ""]))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo.Load('bleh', True, False)
    self.assertEquals(change_info.name, 'bleh')
    self.assertEquals(change_info.issue, 0)
    self.assertEquals(change_info.patchset, 0)
    self.assertEquals(change_info.description, "")
    self.assertEquals(change_info.files, [])

  def testSaveEmpty(self):
    gcl.GetChangelistInfoFile('').AndReturn('foo')
    gcl.WriteFile('foo', gcl.ChangeInfo._SEPARATOR.join(['0, 0', '', '']))
    self.mox.ReplayAll()
    change_info = gcl.ChangeInfo('', 0, 0, '', None)
    change_info.Save()


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
    change_info = gcl.ChangeInfo('naame', 1, 0, 'deescription',
                                 ['aa', 'bb'])
    self.mox.StubOutWithMock(change_info, 'Save')
    args = ['--foo=bar']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(gcl.GetRepositoryRoot().AndReturn(None))
    gcl.GenerateDiff(change_info.FileList())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server', '--foo=bar',
        "--message=''", '--issue=1'], change_info.patch).AndReturn(("1",
                                                                    "2"))
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.GetCodeReviewSetting('TRY_ON_UPLOAD').AndReturn('True')
    gcl.TryChange(change_info, [], swallow_exception=True)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()
    gcl.UploadCL(change_info, args)

  def testServerOverride(self):
    change_info = gcl.ChangeInfo('naame', 0, 0, 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = ['--server=a', '--no_watchlists']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
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
    change_info = gcl.ChangeInfo('naame', 0, 0, 'deescription',
                                 ['aa', 'bb'])
    change_info.Save = self.mox.CreateMockAnything()
    args = ['--no-try', '--no_watchlists']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
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
    change_info = gcl.ChangeInfo('naame', 0, 0, 'deescription',
                                 ['aa', 'bb'])
    self.mox.StubOutWithMock(change_info, 'Save')
    args = ['--no_watchlists']
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
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
    gcl.TryChange(change_info, [], swallow_exception=True)
    gcl.os.chdir('somewhere')
    self.mox.ReplayAll()

    gcl.UploadCL(change_info, args)
    self.assertEquals(change_info.issue, 1)
    self.assertEquals(change_info.patchset, 2)


if __name__ == '__main__':
  unittest.main()
