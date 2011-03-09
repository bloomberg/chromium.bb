#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for scm.py."""

# pylint: disable=E1101,W0403

# Fixes include path.
from super_mox import SuperMoxTestBase

import fake_repos
import scm


class BaseTestCase(SuperMoxTestBase):
  # Like unittest's assertRaises, but checks for Gclient.Error.
  def assertRaisesError(self, msg, fn, *args, **kwargs):
    try:
      fn(*args, **kwargs)
    except scm.gclient_utils.Error, e:
      self.assertEquals(e.args[0], msg)
    else:
      self.fail('%s not raised' % msg)


class BaseSCMTestCase(BaseTestCase):
  def setUp(self):
    BaseTestCase.setUp(self)
    self.mox.StubOutWithMock(scm.gclient_utils, 'CheckCall')
    self.mox.StubOutWithMock(scm.gclient_utils, 'CheckCallAndFilter')
    self.mox.StubOutWithMock(scm.gclient_utils, 'CheckCallAndFilterAndHeader')
    self.mox.StubOutWithMock(scm.gclient_utils, 'Popen')


class RootTestCase(BaseSCMTestCase):
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'GetCasedPath', 'GenFakeDiff', 'GIT', 'SVN', 'ValidateEmail',
        'cStringIO', 'gclient_utils', 'glob',  'logging', 'os', 're', 'shutil',
        'subprocess', 'sys', 'tempfile', 'time', 'xml',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm, members)


class GitWrapperTestCase(BaseSCMTestCase):
  def testMembersChanged(self):
    members = [
        'AssertVersion', 'Capture', 'CaptureStatus',
        'FetchUpstreamTuple',
        'GenerateDiff', 'GetBranch', 'GetBranchRef', 'GetCheckoutRoot',
        'GetDifferentFiles', 'GetEmail', 'GetPatchName', 'GetSVNBranch',
        'GetUpstreamBranch', 'IsGitSvn', 'ShortBranchName',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm.GIT, members)

  def testGetEmail(self):
    self.mox.StubOutWithMock(scm.GIT, 'Capture')
    scm.GIT.Capture(['config', 'user.email'], cwd=self.root_dir
                    ).AndReturn('mini@me.com')
    self.mox.ReplayAll()
    self.assertEqual(scm.GIT.GetEmail(self.root_dir), 'mini@me.com')


class SVNTestCase(BaseSCMTestCase):
  def setUp(self):
    BaseSCMTestCase.setUp(self)
    self.mox.StubOutWithMock(scm.SVN, 'Capture')
    self.url = self.SvnUrl()

  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
        'AssertVersion', 'Capture', 'CaptureRevision', 'CaptureInfo',
        'CaptureStatus', 'current_version', 'DiffItem', 'GenerateDiff',
        'GetCheckoutRoot', 'GetEmail', 'GetFileProperty', 'IsMoved',
        'IsMovedInfo', 'ReadSimpleAuth', 'Revert', 'RunAndGetFileList',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(scm.SVN, members)

  def testGetCheckoutRoot(self):
    self.mox.StubOutWithMock(scm.SVN, 'CaptureInfo')
    self.mox.StubOutWithMock(scm, 'GetCasedPath')
    scm.os.path.abspath = lambda x: x
    scm.GetCasedPath = lambda x: x
    scm.SVN.CaptureInfo(self.root_dir + '/foo/bar').AndReturn({
        'Repository Root': 'svn://svn.chromium.org/chrome',
        'URL': 'svn://svn.chromium.org/chrome/trunk/src',
    })
    scm.SVN.CaptureInfo(self.root_dir + '/foo').AndReturn({
        'Repository Root': 'svn://svn.chromium.org/chrome',
        'URL': 'svn://svn.chromium.org/chrome/trunk',
    })
    scm.SVN.CaptureInfo(self.root_dir).AndReturn({
        'Repository Root': 'svn://svn.chromium.org/chrome',
        'URL': 'svn://svn.chromium.org/chrome/trunk/tools/commit-queue/workdir',
    })
    self.mox.ReplayAll()
    self.assertEquals(scm.SVN.GetCheckoutRoot(self.root_dir + '/foo/bar'),
                      self.root_dir + '/foo')

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
    scm.SVN.Capture(['info', '--xml', self.url]).AndReturn(xml_text)
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
    file_info = scm.SVN.CaptureInfo(self.url)
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
    scm.SVN.Capture(['info', '--xml', self.url]).AndReturn(xml_text)
    self.mox.ReplayAll()
    file_info = scm.SVN.CaptureInfo(self.url)
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
    text = r"""<?xml version="1.0"?>
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
    scm.SVN.Capture(['status', '--xml', '.']).AndReturn(text)

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

  def testCaptureStatusEmpty(self):
    text = r"""<?xml version="1.0"?>
    <status>
    <target
       path="perf">
       </target>
       </status>"""
    scm.SVN.Capture(['status', '--xml']).AndReturn(text)
    self.mox.ReplayAll()
    info = scm.SVN.CaptureStatus(None)
    self.assertEquals(info, [])


class RealSvnTest(fake_repos.FakeReposTestBase):
  # Tests that work with a checkout.
  def setUp(self):
    super(RealSvnTest, self).setUp()
    self.FAKE_REPOS.set_up_svn()
    self.svn_root = scm.os.path.join(self.root_dir, 'base')
    scm.SVN.Capture(
        ['checkout', self.svn_base + 'trunk/third_party', 'base'],
        cwd=self.root_dir)
    self.tree = self.mangle_svn_tree(('trunk/third_party@-1', ''),)

  def _capture(self, cmd, **kwargs):
    kwargs.setdefault('cwd', self.svn_root)
    return scm.SVN.Capture(cmd, **kwargs)

  def testCheckout(self):
    # Checkout and verify the tree.
    self.assertTree(self.tree, self.svn_root)

  def testRevert(self):
    # Mess around and make sure revert works for all corner cases.
    # - svn add a file
    # - svn add a file and delete it
    # - Delete a file
    # - svn delete a file
    # - svn move a directory and svn rename files in it
    self._capture(['move', 'foo', 'foo2'])
    self._capture(
        ['move',
         scm.os.path.join('foo2', 'origin'),
         scm.os.path.join('foo2', 'o')])
    scm.os.remove(scm.os.path.join(self.svn_root, 'origin'))
    self._capture(
        ['propset', 'foo', 'bar',
         scm.os.path.join(self.svn_root, 'prout', 'origin')])
    fake_repos.gclient_utils.rmtree(scm.os.path.join(self.svn_root, 'prout'))
    with open(scm.os.path.join(self.svn_root, 'faa'), 'w') as f:
      f.write('eh')
    with open(scm.os.path.join(self.svn_root, 'faala'), 'w') as f:
      f.write('oh')
    self._capture(['add', scm.os.path.join(self.svn_root, 'faala')])
    added_and_removed = scm.os.path.join(self.svn_root, 'added_and_removed')
    with open(added_and_removed, 'w') as f:
      f.write('oh')
    self._capture(['add', added_and_removed])
    scm.os.remove(added_and_removed)

    scm.SVN.Revert(self.svn_root)
    self._capture(['update', '--revision', 'base'])

    self.assertTree(self.tree, self.svn_root)
    # Asserting the tree is not sufficient, svn status must come out clear too.
    self.assertEquals('', self._capture(['status']))


if __name__ == '__main__':
  import unittest
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
