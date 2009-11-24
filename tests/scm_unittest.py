#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for scm.py."""

import shutil
import tempfile

from gclient_test import BaseTestCase
import scm
from super_mox import mox, SuperMoxBaseTestBase


class BaseSCMTestCase(BaseTestCase):
  def setUp(self):
    BaseTestCase.setUp(self)
    self.mox.StubOutWithMock(scm.gclient_utils, 'SubprocessCall')
    self.mox.StubOutWithMock(scm.gclient_utils, 'SubprocessCallAndFilter')


class RootTestCase(BaseSCMTestCase):
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'GIT', 'SVN',
        'gclient_utils', 'os', 're', 'subprocess', 'sys', 'tempfile', 'xml',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm, members)


class GitWrapperTestCase(SuperMoxBaseTestBase):
  sample_git_import = """blob
mark :1
data 6
Hello

blob
mark :2
data 4
Bye

reset refs/heads/master
commit refs/heads/master
mark :3
author Bob <bob@example.com> 1253744361 -0700
committer Bob <bob@example.com> 1253744361 -0700
data 8
A and B
M 100644 :1 a
M 100644 :2 b

blob
mark :4
data 10
Hello
You

blob
mark :5
data 8
Bye
You

commit refs/heads/origin
mark :6
author Alice <alice@example.com> 1253744424 -0700
committer Alice <alice@example.com> 1253744424 -0700
data 13
Personalized
from :3
M 100644 :4 a
M 100644 :5 b

reset refs/heads/master
from :3
"""

  def CreateGitRepo(self, git_import, path):
    try:
      scm.subprocess.Popen(['git', 'init'],
                           stdout=scm.subprocess.PIPE,
                           stderr=scm.subprocess.STDOUT,
                           cwd=path).communicate()
    except OSError:
      # git is not available, skip this test.
      return False
    scm.subprocess.Popen(['git', 'fast-import'],
                         stdin=scm.subprocess.PIPE,
                         stdout=scm.subprocess.PIPE,
                         stderr=scm.subprocess.STDOUT,
                         cwd=path).communicate(input=git_import)
    scm.subprocess.Popen(['git', 'checkout'],
                         stdout=scm.subprocess.PIPE,
                         stderr=scm.subprocess.STDOUT,
                         cwd=path).communicate()
    return True

  def setUp(self):
    SuperMoxBaseTestBase.setUp(self)
    self.args = self.Args()
    self.url = 'git://foo'
    self.root_dir = tempfile.mkdtemp()
    self.relpath = '.'
    self.base_path = scm.os.path.join(self.root_dir, self.relpath)
    self.enabled = self.CreateGitRepo(self.sample_git_import, self.base_path)
    self.fake_root = self.Dir()

  def tearDown(self):
    shutil.rmtree(self.root_dir)
    SuperMoxBaseTestBase.tearDown(self)

  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'COMMAND', 'Capture', 'CaptureStatus', 'GetEmail',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm.GIT, members)

  def testGetEmail(self):
    self.mox.StubOutWithMock(scm.GIT, 'Capture')
    scm.GIT.Capture(['config', 'user.email'], self.fake_root).AndReturn('mini@me.com')
    self.mox.ReplayAll()
    self.assertEqual(scm.GIT.GetEmail(self.fake_root), 'mini@me.com')


class SVNTestCase(BaseSCMTestCase):
  def setUp(self):
    BaseSCMTestCase.setUp(self)
    self.root_dir = self.Dir()
    self.args = self.Args()
    self.url = self.Url()
    self.relpath = 'asf'

  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'COMMAND', 'Capture', 'CaptureHeadRevision', 'CaptureInfo',
        'CaptureStatus', 'DiffItem', 'GetEmail', 'GetFileProperty', 'IsMoved',
        'ReadSimpleAuth', 'Run', 'RunAndFilterOutput', 'RunAndGetFileList',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm.SVN, members)

  def testGetFileInfo(self):
    xml_text = r"""<?xml version="1.0"?>
<info>
<entry kind="file" path="%s" revision="14628">
<url>http://src.chromium.org/svn/trunk/src/chrome/app/d</url>
<repository><root>http://src.chromium.org/svn</root></repository>
<wc-info>
<schedule>add</schedule>
<depth>infinity</depth>
<copy-from-url>http://src.chromium.org/svn/trunk/src/chrome/app/DEPS</copy-from-url>
<copy-from-rev>14628</copy-from-rev>
<checksum>369f59057ba0e6d9017e28f8bdfb1f43</checksum>
</wc-info>
</entry>
</info>
""" % self.url
    self.mox.StubOutWithMock(scm.SVN, 'Capture')
    scm.SVN.Capture(['info', '--xml', self.url], '.', True).AndReturn(xml_text)
    expected = {
      'URL': 'http://src.chromium.org/svn/trunk/src/chrome/app/d',
      'UUID': None,
      'Repository Root': 'http://src.chromium.org/svn',
      'Schedule': 'add',
      'Copied From URL':
        'http://src.chromium.org/svn/trunk/src/chrome/app/DEPS',
      'Copied From Rev': '14628',
      'Path': self.url,
      'Revision': 14628,
      'Node Kind': 'file',
    }
    self.mox.ReplayAll()
    file_info = scm.SVN.CaptureInfo(self.url, '.', True)
    self.assertEquals(sorted(file_info.items()), sorted(expected.items()))

  def testCaptureInfo(self):
    xml_text = """<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="."
   revision="35">
<url>%s</url>
<repository>
<root>%s</root>
<uuid>7b9385f5-0452-0410-af26-ad4892b7a1fb</uuid>
</repository>
<wc-info>
<schedule>normal</schedule>
<depth>infinity</depth>
</wc-info>
<commit
   revision="35">
<author>maruel</author>
<date>2008-12-04T20:12:19.685120Z</date>
</commit>
</entry>
</info>
""" % (self.url, self.root_dir)
    self.mox.StubOutWithMock(scm.SVN, 'Capture')
    scm.SVN.Capture(['info', '--xml', self.url], '.', True).AndReturn(xml_text)
    self.mox.ReplayAll()
    file_info = scm.SVN.CaptureInfo(self.url, '.', True)
    expected = {
      'URL': self.url,
      'UUID': '7b9385f5-0452-0410-af26-ad4892b7a1fb',
      'Revision': 35,
      'Repository Root': self.root_dir,
      'Schedule': 'normal',
      'Copied From URL': None,
      'Copied From Rev': None,
      'Path': '.',
      'Node Kind': 'directory',
    }
    self.assertEqual(file_info, expected)

  def testCaptureStatus(self):
    text =r"""<?xml version="1.0"?>
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
    proc = self.mox.CreateMockAnything()
    scm.subprocess.Popen(['svn', 'status', '--xml', '.'],
                         cwd=None,
                         shell=scm.sys.platform.startswith('win'),
                         stderr=None,
                         stdout=scm.subprocess.PIPE).AndReturn(proc)
    proc.communicate().AndReturn((text, 0))

    self.mox.ReplayAll()
    info = scm.SVN.CaptureStatus('.')
    expected = [
      ('?      ', 'unversionned_file.txt'),
      ('M      ', 'build\\internal\\essential.vsprops'),
      ('A  +   ', 'chrome\\app\\d'),
      ('MM     ', 'chrome\\app\\DEPS'),
      ('C      ', 'scripts\\master\\factory\\gclient_factory.py'),
    ]
    self.assertEquals(sorted(info), sorted(expected))

  def testRun(self):
    param2 = 'bleh'
    scm.gclient_utils.SubprocessCall(['svn', 'foo', 'bar'],
                                     param2).AndReturn(None)
    self.mox.ReplayAll()
    scm.SVN.Run(['foo', 'bar'], param2)

  def testCaptureStatusEmpty(self):
    text = r"""<?xml version="1.0"?>
    <status>
    <target
       path="perf">
       </target>
       </status>"""
    proc = self.mox.CreateMockAnything()
    scm.subprocess.Popen(['svn', 'status', '--xml'],
                         cwd=None,
                         shell=scm.sys.platform.startswith('win'),
                         stderr=None,
                         stdout=scm.subprocess.PIPE).AndReturn(proc)
    proc.communicate().AndReturn((text, 0))
    self.mox.ReplayAll()
    info = scm.SVN.CaptureStatus(None)
    self.assertEquals(info, [])


if __name__ == '__main__':
  import unittest
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
