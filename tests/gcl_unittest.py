#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gcl.py."""

# Fixes include path.
from super_mox import mox, SuperMoxTestBase

import gcl


class GclTestsBase(SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    SuperMoxTestBase.setUp(self)
    self.fake_root_dir = self.RootDir()
    self.mox.StubOutWithMock(gcl, 'RunShell')
    self.mox.StubOutWithMock(gcl.SVN, 'CaptureInfo')
    self.mox.StubOutWithMock(gcl.SVN, 'GetCheckoutRoot')
    self.mox.StubOutWithMock(gcl, 'tempfile')
    self.mox.StubOutWithMock(gcl.upload, 'RealMain')
    self.mox.StubOutWithMock(gcl.gclient_utils, 'FileRead')
    self.mox.StubOutWithMock(gcl.gclient_utils, 'FileWrite')
    gcl.REPOSITORY_ROOT = None


class GclUnittest(GclTestsBase):
  """General gcl.py tests."""
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'CODEREVIEW_SETTINGS', 'CODEREVIEW_SETTINGS_FILE',
        'CMDchange', 'CMDchanges', 'CMDcommit', 'CMDdelete', 'CMDdeleteempties',
        'CMDdescription', 'CMDdiff', 'CMDhelp', 'CMDlint', 'CMDnothave',
        'CMDopened', 'CMDpassthru', 'CMDpresubmit', 'CMDrename', 'CMDsettings',
        'CMDstatus', 'CMDtry', 'CMDupload',
        'ChangeInfo', 'Command', 'DEFAULT_LINT_IGNORE_REGEX',
        'DEFAULT_LINT_REGEX', 'CheckHomeForFile', 'DoPresubmitChecks',
        'ErrorExit', 'FILES_CACHE', 'FilterFlag', 'GenUsage',
        'GenerateChangeName', 'GenerateDiff', 'GetCLs', 'GetCacheDir',
        'GetCachedFile', 'GetChangelistInfoFile', 'GetChangesDir',
        'GetCodeReviewSetting', 'GetEditor', 'GetFilesNotInCL', 'GetInfoDir',
        'GetIssueDescription', 'GetModifiedFiles', 'GetRepositoryRoot',
        'ListFiles',
        'LoadChangelistInfoForMultiple', 'MISSING_TEST_MSG',
        'OptionallyDoPresubmitChecks', 'REPOSITORY_ROOT',
        'RunShell', 'RunShellWithReturnCode', 'SVN',
        'SendToRietveld', 'TryChange', 'UnknownFiles', 'Warn',
        'attrs', 'breakpad', 'defer_attributes', 'gclient_utils', 'getpass',
        'main', 'need_change', 'need_change_and_args', 'no_args', 'os',
        'random', 're', 'string', 'subprocess', 'sys', 'tempfile',
        'time', 'upload', 'urllib2',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl, members)

  def testIsSVNMoved(self):
    # TODO(maruel): TEST ME
    pass

  def testGetSVNFileProperty(self):
    # TODO(maruel): TEST ME
    pass

  def testUnknownFiles(self):
    # TODO(maruel): TEST ME
    pass

  def testCheckHomeForFile(self):
    # TODO(maruel): TEST ME
    pass

  def testGetRepositoryRootNone(self):
    gcl.os.getcwd().AndReturn(self.fake_root_dir)
    gcl.SVN.GetCheckoutRoot(self.fake_root_dir).AndReturn(None)
    self.mox.ReplayAll()
    self.assertRaises(gcl.gclient_utils.Error, gcl.GetRepositoryRoot)

  def testGetRepositoryRootGood(self):
    root_path = gcl.os.path.join('bleh', 'prout', 'pouet')
    gcl.os.getcwd().AndReturn(root_path)
    gcl.SVN.GetCheckoutRoot(root_path).AndReturn(root_path + '.~')
    self.mox.ReplayAll()
    self.assertEquals(gcl.GetRepositoryRoot(), root_path + '.~')

  def testHelp(self):
    gcl.sys.stdout.write = lambda x: None
    self.mox.ReplayAll()
    gcl.CMDhelp([])


class ChangeInfoUnittest(GclTestsBase):
  def setUp(self):
    GclTestsBase.setUp(self)
    self.mox.StubOutWithMock(gcl, 'GetChangelistInfoFile')
    self.mox.StubOutWithMock(gcl, 'GetRepositoryRoot')

  def testChangeInfoMembers(self):
    self.mox.ReplayAll()
    members = [
      'CloseIssue', 'Delete', 'GetFiles', 'GetFileNames', 'GetLocalRoot',
      'Exists', 'Load', 'MissingTests', 'NeedsUpload', 'Save',
      'UpdateRietveldDescription', 'description', 'issue', 'name',
      'needs_upload', 'patch', 'patchset',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gcl.ChangeInfo('', 0, 0, '', None, self.fake_root_dir),
                        members)

  def testChangeInfoBase(self):
    files = [('M', 'foo'), ('A', 'bar')]
    self.mox.ReplayAll()
    o = gcl.ChangeInfo('name2', '42', '53', 'description2', files,
                       self.fake_root_dir)
    self.assertEquals(o.name, 'name2')
    self.assertEquals(o.issue, 42)
    self.assertEquals(o.patchset, 53)
    self.assertEquals(o.description, 'description2')
    self.assertEquals(o.patch, None)
    self.assertEquals(o.GetFileNames(), ['foo', 'bar'])
    self.assertEquals(o.GetFiles(), files)
    self.assertEquals(o.GetLocalRoot(), self.fake_root_dir)

  def testLoadWithIssue(self):
    description = ["This is some description.", "force an extra separator."]
    gcl.GetChangelistInfoFile('bleh').AndReturn('bleeeh')
    gcl.os.path.exists('bleeeh').AndReturn(True)
    gcl.gclient_utils.FileRead('bleeeh', 'r').AndReturn(
      gcl.ChangeInfo._SEPARATOR.join(["42, 53", "G      b.cc"] + description))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo.Load('bleh', self.fake_root_dir, True, False)
    self.assertEquals(change_info.name, 'bleh')
    self.assertEquals(change_info.issue, 42)
    self.assertEquals(change_info.patchset, 53)
    self.assertEquals(change_info.description,
                      gcl.ChangeInfo._SEPARATOR.join(description))
    self.assertEquals(change_info.GetFiles(), [('G      ', 'b.cc')])

  def testLoadEmpty(self):
    gcl.GetChangelistInfoFile('bleh').AndReturn('bleeeh')
    gcl.os.path.exists('bleeeh').AndReturn(True)
    gcl.gclient_utils.FileRead('bleeeh', 'r').AndReturn(
        gcl.ChangeInfo._SEPARATOR.join(["", "", ""]))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo.Load('bleh', self.fake_root_dir, True, False)
    self.assertEquals(change_info.name, 'bleh')
    self.assertEquals(change_info.issue, 0)
    self.assertEquals(change_info.patchset, 0)
    self.assertEquals(change_info.description, "")
    self.assertEquals(change_info.GetFiles(), [])

  def testSaveEmpty(self):
    gcl.GetChangelistInfoFile('').AndReturn('foo')
    gcl.gclient_utils.FileWrite(
        'foo',
        gcl.ChangeInfo._SEPARATOR.join(['0, 0, clean', '', '']))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo('', 0, 0, '', None, self.fake_root_dir)
    change_info.Save()

  def testSaveDirty(self):
    gcl.GetChangelistInfoFile('').AndReturn('foo')
    gcl.gclient_utils.FileWrite(
        'foo',
        gcl.ChangeInfo._SEPARATOR.join(['0, 0, dirty', '', '']))
    self.mox.ReplayAll()

    change_info = gcl.ChangeInfo('', 0, 0, '', None, self.fake_root_dir,
                                 needs_upload=True)
    change_info.Save()


class CMDuploadUnittest(GclTestsBase):
  def setUp(self):
    GclTestsBase.setUp(self)
    self.mox.StubOutWithMock(gcl, 'CheckHomeForFile')
    self.mox.StubOutWithMock(gcl, 'DoPresubmitChecks')
    self.mox.StubOutWithMock(gcl, 'GenerateDiff')
    self.mox.StubOutWithMock(gcl, 'GetCodeReviewSetting')
    self.mox.StubOutWithMock(gcl, 'GetRepositoryRoot')
    self.mox.StubOutWithMock(gcl, 'SendToRietveld')
    self.mox.StubOutWithMock(gcl, 'TryChange')
    self.mox.StubOutWithMock(gcl.ChangeInfo, 'Load')

  def testNew(self):
    change_info = self.mox.CreateMock(gcl.ChangeInfo)
    change_info.name = 'naame'
    change_info.issue = 1
    change_info.patchset = 0
    change_info.description = 'deescription',
    change_info.files = [('A', 'aa'), ('M', 'bb')]
    change_info.patch = None
    files = [item[1] for item in change_info.files]
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.os.getcwd().AndReturn('somewhere')
    change_info.GetFiles().AndReturn(change_info.files)
    change_info.GetLocalRoot().AndReturn('proout')
    gcl.os.chdir('proout')
    change_info.GetFileNames().AndReturn(files)
    gcl.GenerateDiff(files)
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server',
                         '-r', 'georges@example.com',
                         '--message=\'\'', '--issue=1'],
                         change_info.patch).AndReturn(("1",
                                                                    "2"))
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.os.chdir('somewhere')
    gcl.sys.stdout.write("*** Upload does not submit a try; use gcl try to"
                         " submit a try. ***")
    gcl.sys.stdout.write("\n")
    change_info.Save()
    gcl.GetRepositoryRoot().AndReturn(self.fake_root_dir)
    gcl.ChangeInfo.Load('naame', self.fake_root_dir, True, True
        ).AndReturn(change_info)
    self.mox.ReplayAll()

    gcl.CMDupload(['naame', '-r', 'georges@example.com'])

  def testServerOverride(self):
    change_info = gcl.ChangeInfo('naame', 0, 0, 'deescription',
                                 [('A', 'aa'), ('M', 'bb')],
                                self.fake_root_dir)
    self.mox.StubOutWithMock(change_info, 'Save')
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.tempfile.mkstemp(text=True).AndReturn((42, 'descfile'))
    gcl.os.write(42, change_info.description)
    gcl.os.close(42)
    gcl.GetCodeReviewSetting('CC_LIST')
    gcl.GetCodeReviewSetting('PRIVATE')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(change_info.GetLocalRoot())
    gcl.GenerateDiff(change_info.GetFileNames())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server', '--server=a',
        "--description_file=descfile",
        "--message=deescription"], change_info.patch).AndReturn(("1", "2"))
    gcl.os.remove('descfile')
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.os.chdir('somewhere')
    gcl.sys.stdout.write("*** Upload does not submit a try; use gcl try to"
                         " submit a try. ***")
    gcl.sys.stdout.write("\n")
    gcl.GetRepositoryRoot().AndReturn(self.fake_root_dir)
    gcl.ChangeInfo.Load('naame', self.fake_root_dir, True, True
        ).AndReturn(change_info)
    self.mox.ReplayAll()

    gcl.CMDupload(['naame', '--server=a', '--no_watchlists'])

  def testNormal(self):
    change_info = gcl.ChangeInfo('naame', 0, 0, 'deescription',
                                 [('A', 'aa'), ('M', 'bb')],
                                 self.fake_root_dir)
    self.mox.StubOutWithMock(change_info, 'Save')
    change_info.Save()
    gcl.DoPresubmitChecks(change_info, False, True).AndReturn(True)
    gcl.GetCodeReviewSetting('CODE_REVIEW_SERVER').AndReturn('my_server')
    gcl.tempfile.mkstemp(text=True).AndReturn((42, 'descfile'))
    gcl.os.write(42, change_info.description)
    gcl.os.close(42)
    gcl.GetCodeReviewSetting('CC_LIST')
    gcl.GetCodeReviewSetting('PRIVATE')
    gcl.os.getcwd().AndReturn('somewhere')
    gcl.os.chdir(change_info.GetLocalRoot())
    gcl.GenerateDiff(change_info.GetFileNames())
    gcl.upload.RealMain(['upload.py', '-y', '--server=my_server',
        "--description_file=descfile",
        "--message=deescription"], change_info.patch).AndReturn(("1", "2"))
    gcl.os.remove('descfile')
    gcl.SendToRietveld("/lint/issue%s_%s" % ('1', '2'), timeout=0.5)
    gcl.os.chdir('somewhere')
    gcl.sys.stdout.write("*** Upload does not submit a try; use gcl try to"
                         " submit a try. ***")
    gcl.sys.stdout.write("\n")
    gcl.GetRepositoryRoot().AndReturn(self.fake_root_dir)
    gcl.ChangeInfo.Load('naame', self.fake_root_dir, True, True
        ).AndReturn(change_info)
    self.mox.ReplayAll()

    gcl.CMDupload(['naame', '--no_watchlists'])
    self.assertEquals(change_info.issue, 1)
    self.assertEquals(change_info.patchset, 2)


if __name__ == '__main__':
  import unittest
  unittest.main()
