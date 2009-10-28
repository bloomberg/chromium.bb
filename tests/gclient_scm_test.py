#!/usr/bin/python
#
# Copyright 2008-2009 Google Inc.  All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unit tests for gclient_scm.py."""

import os
import shutil
import subprocess
import tempfile
import unittest

import gclient
import gclient_scm
import gclient_test
import gclient_utils
from super_mox import mox


class SVNWrapperTestCase(gclient_test.GClientBaseTestCase):
  class OptionsObject(object):
     def __init__(self, test_case, verbose=False, revision=None):
      self.verbose = verbose
      self.revision = revision
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.force = False
      self.nohooks = False

  def setUp(self):
    gclient_test.GClientBaseTestCase.setUp(self)
    self.root_dir = self.Dir()
    self.args = self.Args()
    self.url = self.Url()
    self.relpath = 'asf'

  def testDir(self):
    members = [
      'FullUrlForRelativeUrl', 'RunCommand', 'cleanup', 'diff', 'export',
      'pack', 'relpath', 'revert', 'runhooks', 'scm_name', 'status',
      'update', 'url',
    ]

    # If you add a member, be sure to add the relevant test!
    self.compareMembers(self._scm_wrapper(), members)

  def testUnsupportedSCM(self):
    args = [self.url, self.root_dir, self.relpath]
    kwargs = {'scm_name' : 'foo'}
    exception_msg = 'Unsupported scm %(scm_name)s' % kwargs
    self.assertRaisesError(exception_msg, self._scm_wrapper, *args, **kwargs)

  def testFullUrlForRelativeUrl(self):
    self.url = 'svn://a/b/c/d'

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    self.assertEqual(scm.FullUrlForRelativeUrl('/crap'), 'svn://a/b/crap')

  def testRunCommandException(self):
    options = self.Options(verbose=False)
    file_path = os.path.join(self.root_dir, self.relpath, '.git')
    gclient.os.path.exists(file_path).AndReturn(False)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    exception = "Unsupported argument(s): %s" % ','.join(self.args)
    self.assertRaisesError(exception, scm.RunCommand,
                           'update', options, self.args)

  def testRunCommandUnknown(self):
    # TODO(maruel): if ever used.
    pass

  def testRevertMissing(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(False)
    # It'll to a checkout instead.
    gclient.os.path.exists(os.path.join(base_path, '.git')).AndReturn(False)
    print("\n_____ %s is missing, synching instead" % self.relpath)
    # Checkout.
    gclient.os.path.exists(base_path).AndReturn(False)
    files_list = self.mox.CreateMockAnything()
    gclient_scm.RunSVNAndGetFileList(options, ['checkout', self.url, base_path],
                                     self.root_dir, files_list)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.revert(options, self.args, files_list)

  def testRevertNone(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(True)
    gclient_scm.CaptureSVNStatus(base_path).AndReturn([])
    gclient_scm.RunSVNAndGetFileList(options, ['update', '--revision', 'BASE'],
                                     base_path, mox.IgnoreArg())

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)

  def testRevert2Files(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(True)
    items = [
      ('M      ', 'a'),
      ('A      ', 'b'),
    ]
    file_path1 = os.path.join(base_path, 'a')
    file_path2 = os.path.join(base_path, 'b')
    gclient_scm.CaptureSVNStatus(base_path).AndReturn(items)
    gclient_scm.os.path.exists(file_path1).AndReturn(True)
    gclient_scm.os.path.isfile(file_path1).AndReturn(True)
    gclient_scm.os.remove(file_path1)
    gclient_scm.os.path.exists(file_path2).AndReturn(True)
    gclient_scm.os.path.isfile(file_path2).AndReturn(True)
    gclient_scm.os.remove(file_path2)
    gclient_scm.RunSVNAndGetFileList(options, ['update', '--revision', 'BASE'],
                                     base_path, mox.IgnoreArg())
    print(os.path.join(base_path, 'a'))
    print(os.path.join(base_path, 'b'))

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)

  def testRevertDirectory(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(True)
    items = [
      ('~      ', 'a'),
    ]
    gclient_scm.CaptureSVNStatus(base_path).AndReturn(items)
    file_path = os.path.join(base_path, 'a')
    print(file_path)
    gclient_scm.os.path.exists(file_path).AndReturn(True)
    gclient_scm.os.path.isfile(file_path).AndReturn(False)
    gclient_scm.os.path.isdir(file_path).AndReturn(True)
    gclient_utils.RemoveDirectory(file_path)
    file_list1 = []
    gclient_scm.RunSVNAndGetFileList(options, ['update', '--revision', 'BASE'],
                                     base_path, mox.IgnoreArg())

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list2 = []
    scm.revert(options, self.args, file_list2)

  def testStatus(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(True)
    gclient_scm.RunSVNAndGetFileList(options, ['status'] + self.args,
                                     base_path, []).AndReturn(None)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    self.assertEqual(scm.status(options, self.args, file_list), None)


  # TODO(maruel):  TEST REVISIONS!!!
  # TODO(maruel):  TEST RELOCATE!!!
  def testUpdateCheckout(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    file_info = gclient_utils.PrintableObject()
    file_info.root = 'blah'
    file_info.url = self.url
    file_info.uuid = 'ABC'
    file_info.revision = 42
    gclient.os.path.exists(os.path.join(base_path, '.git')).AndReturn(False)
    # Checkout.
    gclient.os.path.exists(base_path).AndReturn(False)
    files_list = self.mox.CreateMockAnything()
    gclient_scm.RunSVNAndGetFileList(options, ['checkout', self.url, 
                                     base_path], self.root_dir, files_list)
    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.update(options, (), files_list)

  def testUpdateUpdate(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    options.force = True
    options.nohooks = False
    file_info = {
      'Repository Root': 'blah',
      'URL': self.url,
      'UUID': 'ABC',
      'Revision': 42,
    }
    gclient.os.path.exists(os.path.join(base_path, '.git')).AndReturn(False)
    # Checkout or update.
    gclient.os.path.exists(base_path).AndReturn(True)
    gclient_scm.CaptureSVNInfo(os.path.join(base_path, "."), '.'
                               ).AndReturn(file_info)
    # Cheat a bit here.
    gclient_scm.CaptureSVNInfo(file_info['URL'], '.').AndReturn(file_info)
    additional_args = []
    if options.manually_grab_svn_rev:
      additional_args = ['--revision', str(file_info['Revision'])]
    files_list = []
    gclient_scm.RunSVNAndGetFileList(options,
                                     ['update', base_path] + additional_args,
                                     self.root_dir, files_list)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.update(options, (), files_list)

  def testUpdateGit(self):
    options = self.Options(verbose=True)
    file_path = os.path.join(self.root_dir, self.relpath, '.git')
    gclient.os.path.exists(file_path).AndReturn(True)
    print("________ found .git directory; skipping %s" % self.relpath)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.update(options, self.args, file_list)

  def testGetSVNFileInfo(self):
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
    gclient_scm.CaptureSVN(['info', '--xml', self.url],
                       '.', True).AndReturn(xml_text)
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
    file_info = self._CaptureSVNInfo(self.url, '.', True)
    self.assertEquals(sorted(file_info.items()), sorted(expected.items()))

  def testCaptureSvnInfo(self):
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
    gclient_scm.CaptureSVN(['info', '--xml',
                            self.url], '.', True).AndReturn(xml_text)
    self.mox.ReplayAll()
    file_info = self._CaptureSVNInfo(self.url, '.', True)
    expected = {
      'URL': self.url,
      'UUID': '7b9385f5-0452-0410-af26-ad4892b7a1fb',
      'Revision': 35,
      'Repository Root': self.root_dir,
      'Schedule': 'normal',
      'Copied From URL': None,
      'Copied From Rev': None,
      'Path': '.',
      'Node Kind': 'dir',
    }
    self.assertEqual(file_info, expected)


class GitWrapperTestCase(gclient_test.GClientBaseTestCase):
  class OptionsObject(object):
     def __init__(self, test_case, verbose=False, revision=None):
      self.verbose = verbose
      self.revision = revision
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.force = False
      self.nohooks = False

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
  def Options(self, *args, **kwargs):
    return self.OptionsObject(self, *args, **kwargs)

  def CreateGitRepo(self, git_import, path):
    try:
      subprocess.Popen(['git', 'init'], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, cwd=path).communicate()
    except WindowsError:
      # git is not available, skip this test.
      return False
    subprocess.Popen(['git', 'fast-import'], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     cwd=path).communicate(input=git_import)
    subprocess.Popen(['git', 'checkout'], stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, cwd=path).communicate()
    return True

  def GetGitRev(self, path):
    return subprocess.Popen(['git', 'rev-parse', 'HEAD'],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            cwd=path).communicate()[0].strip()

  def setUp(self):
    gclient_test.BaseTestCase.setUp(self)
    self.args = self.Args()
    self.url = 'git://foo'
    self.root_dir = tempfile.mkdtemp()
    self.relpath = '.'
    self.base_path = os.path.join(self.root_dir, self.relpath)
    self.enabled = self.CreateGitRepo(self.sample_git_import, self.base_path)

  def tearDown(self):
    shutil.rmtree(self.root_dir)
    gclient_test.BaseTestCase.tearDown(self)

  def testDir(self):
    members = [
      'FullUrlForRelativeUrl', 'RunCommand', 'cleanup', 'diff', 'export',
      'relpath', 'revert', 'runhooks', 'scm_name', 'status', 'update', 'url',
    ]

    # If you add a member, be sure to add the relevant test!
    self.compareMembers(gclient_scm.CreateSCM(url=self.url), members)

  def testRevertMissing(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = os.path.join(self.base_path, 'a')
    os.remove(file_path)
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])

  def testRevertNone(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(self.GetGitRev(self.base_path),
                      '069c602044c5388d2d15c3f875b057c852003458')


  def testRevertModified(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = os.path.join(self.base_path, 'a')
    open(file_path, 'a').writelines('touched\n')
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(self.GetGitRev(self.base_path),
                      '069c602044c5388d2d15c3f875b057c852003458')

  def testRevertNew(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = os.path.join(self.base_path, 'c')
    f = open(file_path, 'w')
    f.writelines('new\n')
    f.close()
    subprocess.Popen(['git', 'add', 'c'], stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, cwd=self.base_path).communicate()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(self.GetGitRev(self.base_path),
                      '069c602044c5388d2d15c3f875b057c852003458')

  def testStatusNew(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = os.path.join(self.base_path, 'a')
    open(file_path, 'a').writelines('touched\n')
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.status(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])

  def testStatus2New(self):
    if not self.enabled:
      return
    options = self.Options()
    expected_file_list = []
    for f in ['a', 'b']:
        file_path = os.path.join(self.base_path, f)
        open(file_path, 'a').writelines('touched\n')
        expected_file_list.extend([file_path])
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.status(options, self.args, file_list)
    expected_file_list = [os.path.join(self.base_path, x) for x in ['a', 'b']]
    self.assertEquals(sorted(file_list), expected_file_list)

  def testUpdateCheckout(self):
    if not self.enabled:
      return
    options = self.Options(verbose=True)
    root_dir = tempfile.mkdtemp()
    relpath = 'foo'
    base_path = os.path.join(root_dir, relpath)
    url = os.path.join(self.root_dir, self.relpath, '.git')
    try:
      scm = gclient_scm.CreateSCM(url=url, root_dir=root_dir,
                                  relpath=relpath)
      file_list = []
      scm.update(options, (), file_list)
      self.assertEquals(len(file_list), 2)
      self.assert_(os.path.isfile(os.path.join(base_path, 'a')))
      self.assertEquals(self.GetGitRev(base_path),
                        '069c602044c5388d2d15c3f875b057c852003458')
    finally:
      shutil.rmtree(root_dir)

  def testUpdateUpdate(self):
    if not self.enabled:
      return
    options = self.Options()
    expected_file_list = [os.path.join(self.base_path, x) for x in ['a', 'b']]
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, (), file_list)
    self.assertEquals(self.GetGitRev(self.base_path),
                      'a7142dc9f0009350b96a11f372b6ea658592aa95')


class RunSVNTestCase(gclient_test.BaseTestCase):
  def testRunSVN(self):
    param2 = 'bleh'
    self.mox.StubOutWithMock(gclient_utils, 'SubprocessCall')
    gclient_utils.SubprocessCall(['svn', 'foo', 'bar'], param2).AndReturn(None)
    self.mox.ReplayAll()
    gclient_scm.RunSVN(['foo', 'bar'], param2)


if __name__ == '__main__':
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
