#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chrome_set_ver."""

import mox
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib as cros_lib
from chromite.scripts import chrome_set_ver


# pylint: disable=W0212,R0904
class TestEndedException(Exception):
  """Exception thrown by test to mark completion."""
  pass


class DEPSFileTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

    self.tempdir = tempfile.mkdtemp()
    self.test_base = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                  'testdata/chrome_set_ver_unittest')
    self.repo_root = os.path.join(self.tempdir, 'repo_root')
    self.repo_root_zip = os.path.join(self.test_base,
                                          'repo_root.tar.gz')

    assert(not os.path.exists(self.repo_root))
    print('Unzipping test repo_root')
    cros_lib.RunCommand(['tar', '-xvf', self.repo_root_zip],
                        cwd=self.tempdir, redirect_stdout=True)
    assert(os.path.exists(self.repo_root))

    self.old_dir = os.getcwd()
    os.chdir(self.repo_root)
    repo_root = cros_lib.FindRepoDir()
    assert(os.path.realpath(os.path.dirname(repo_root)) == self.repo_root)


  def _VerifyReposAndSymlink(self):
    # Verify symlink was generated
    test_file = 'chromium/src/third_party/cros/HELLO'
    self.assertTrue(os.path.exists(os.path.join(self.repo_root, test_file)))
    test_file = 'chromium/src/third_party/cros_system_api/WORLD'
    self.assertTrue(os.path.exists(os.path.join(self.repo_root, test_file)))

    # Verify repo's were reset properly
    repos = {'repo1': '8d35063e1836c79c9ef97bf81eb43f450dc111ac',
             'repo2': 'b2f03c74b48866eb3da5c4cab554c792a70aeda8',
             'repo3': '8d35063e1836c79c9ef97bf81eb43f450dc111ac',
             'repo4': 'b2f03c74b48866eb3da5c4cab554c792a70aeda8',
             'third_party/cros': '8d35063e1836c79c9ef97bf81eb43f450dc111ac',
             'third_party/cros_system_api':
             'b2f03c74b48866eb3da5c4cab554c792a70aeda8'}

    for repo, revision in repos.iteritems():
      repo_path = os.path.join(self.repo_root, 'chromium', 'src', repo)
      self.assertTrue(cros_lib.GetGitRepoRevision(repo_path) == revision)


  def testParseAndRunDEPSFileNoHooks(self):
    """Test resetting of repos, usage of Var's."""
    chrome_set_ver.main(['-d',
                         os.path.join(self.test_base, 'test_1/DEPS.git')])

    self._VerifyReposAndSymlink()

  def testParseAndRunDEPSFileWithHooks(self):
    """Test correct operation with --runhooks option."""
    chrome_set_ver.main(['--runhooks', '-d',
                         os.path.join(self.test_base, 'test_1/DEPS.git')])

    self._VerifyReposAndSymlink()

    # Verify hooks were run
    hook1_output = os.path.join(self.repo_root, 'chromium', 'out1')
    hook2_output = os.path.join(self.repo_root, 'chromium', 'out2')
    self.assertTrue(os.path.exists(hook1_output))
    self.assertTrue(os.path.exists(hook2_output))

  def testParseInternalDEPSFile(self):
    """Test that the internal DEPS file is found and parsed properly."""
    shutil.copyfile(os.path.join(self.test_base, 'test_internal/DEPS.git'),
                    os.path.join(self.repo_root,
                                 'chromium/src-internal/.DEPS.git'))

    chrome_set_ver.main(['-d',
                         os.path.join(self.test_base, 'test_1/DEPS.git')])

    repos = {'repo_internal': '8d35063e1836c79c9ef97bf81eb43f450dc111ac'}

    for repo, revision in repos.iteritems():
      repo_path = os.path.join(self.repo_root, 'chromium', 'src', repo)
      self.assertTrue(cros_lib.GetGitRepoRevision(repo_path) == revision)

  def testErrorOnOverlap(self):
    """Test that an overlapping entry in unix deps causes error."""
    self.assertRaises(AssertionError, chrome_set_ver.main,
                      ['-d', os.path.join(self.test_base, 'test_2/DEPS.git')])

  def testErrorOnFromSyntax(self):
    """Test that using the 'From' keyword causes error."""
    self.assertRaises(NotImplementedError, chrome_set_ver.main,
                      ['-d', os.path.join(self.test_base, 'test_3/DEPS.git')])

  def testErrorOnNonexistentHash(self):
    """Test dying cleanly when trying to pin to nonexistent hash."""
    self.mox.StubOutWithMock(cros_lib, 'Die')
    cros_lib.Die(mox.IgnoreArg()).AndRaise(TestEndedException)
    self.mox.ReplayAll()
    self.assertRaises(TestEndedException, chrome_set_ver.main,
                      ['-d', os.path.join(self.test_base, 'test_4/DEPS.git')])

  def testProjectInManifestNotWorkingDir(self):
    """Test erroring out and telling user to repo sync."""
    self.mox.StubOutWithMock(cros_lib, 'Die')
    cros_lib.Die(mox.StrContains('not in working tree')).AndRaise(
        TestEndedException)
    self.mox.ReplayAll()
    self.assertRaises(TestEndedException, chrome_set_ver.main,
                      ['-d', os.path.join(self.test_base, 'test_5/DEPS.git')])

  def testAutoSyncProject(self):
    """Test pulling down of projects not in manifest."""
    chrome_set_ver.main(['-d',
                         os.path.join(self.test_base, 'test_6/DEPS.git')])
    repo_path = os.path.join(self.repo_root, 'chromium/src/does_not_exist')
    self.assertTrue(cros_lib.GetGitRepoRevision(repo_path) ==
                    '8d35063e1836c79c9ef97bf81eb43f450dc111ac')

  def testProjectThatWeManageNowInManifest(self):
    """Test erroring out and telling user to remove project."""
    self.mox.StubOutWithMock(cros_lib, 'Die')
    cros_lib.Die(mox.StrContains('needs to be replaced')).AndRaise(
        TestEndedException)
    self.mox.ReplayAll()
    self.assertRaises(TestEndedException, chrome_set_ver.main,
                      ['-d', os.path.join(self.test_base, 'test_7/DEPS.git')])

  def testParseUrl(self):
    """Test extracting of project name from url."""
    url = 'http://git.chromium.org/external/libjingle.git'
    self.assertEquals('external/libjingle',
                      chrome_set_ver._ExtractProjectFromUrl(url))

    url = 'ssh://gerrit-int.chromium.org:29419/chrome/data/page_cycler.git'
    self.assertEquals('chrome/data/page_cycler',
                      chrome_set_ver._ExtractProjectFromUrl(url))

    url = 'git+ssh://gerrit-int.chromium.org:29419/chrome/data/page_cycler.git'
    self.assertEquals('chrome/data/page_cycler',
                      chrome_set_ver._ExtractProjectFromUrl(url))

    url = '../chrome/data/page_cycler.git'
    self.assertEquals('../chrome/data/page_cycler',
                      chrome_set_ver._ExtractProjectFromUrl(url))


  def tearDown(self):
    os.chdir(self.old_dir)
    cros_lib.RunCommand(['rm', '-rf', self.tempdir])
    assert(not os.path.exists(self.repo_root))
    mox.MoxTestBase.tearDown(self)

if __name__ == '__main__':
  unittest.main()
