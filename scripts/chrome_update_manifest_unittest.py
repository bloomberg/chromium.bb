#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chrome_update_manifest."""

import filecmp
import mox
import os
import re
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib as cros_lib
from chromite.buildbot import manifest_version
from chromite.scripts import chrome_update_manifest

# pylint: disable=W0212,R0904

class DEPSFileTest(mox.MoxTestBase):
  def setUp(self):
    mox.MoxTestBase.setUp(self)

    self.temp_manifest_dir = tempfile.mkdtemp()
    print ('temp manifest dir: %s' % self.temp_manifest_dir)
    self.manifest_path = os.path.join(self.temp_manifest_dir, 'oldlayout.xml')
    self.tempdir = tempfile.mkdtemp()
    self.test_base = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                  'testdata/chrome_update_manifest_unittest')
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

  def _assertRaisesRegexp(self, regexp, func):
    try:
      func()
    except Exception as e:
      self.assertTrue(re.match(regexp, str(e)))

  def _CopyTestFiles(self, test_name):
    shutil.copyfile(os.path.join(self.test_base, test_name, 'DEPS.git'),
                    os.path.join(self.repo_root,
                                 'chromium/src/.DEPS.git'))

    shutil.copyfile(os.path.join(self.test_base, test_name, 'DEPS_cros.git'),
                    os.path.join(self.repo_root,
                                 'chromium/src/tools/cros.DEPS/DEPS'))

    internal_src_deps = os.path.join(self.test_base, test_name,
                                     'DEPS_internal.git')
    if os.path.exists(internal_src_deps):
      shutil.copyfile(internal_src_deps,
                      os.path.join(self.repo_root,
                                   'chromium/src-internal/.DEPS.git'))

    shutil.copyfile(os.path.join(self.test_base, test_name, 'previous.xml'),
                    self.manifest_path)


  def testParseInternalManifestWithDoubleCheckout(self):
    """Test a scenario where one project is checked out to two locations."""
    self._CopyTestFiles('test1')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=True, dryrun=True)
    manifest.CreateNewManifest()
    assert(filecmp.cmp(manifest.new_manifest_path,
                       os.path.join(self.test_base, 'test1/expected.xml'),
                       shallow=False))

  def testNoBeginMarker(self):
    """Test case where manifest has no begin marker for chrome projects."""
    self._CopyTestFiles('test2')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=False, dryrun=True)
    self._assertRaisesRegexp('.* begin/end markers .*',
                            manifest.CreateNewManifest)

  def testNoEndMarker(self):
    """Test case where manifest has no end marker for chrome projects."""
    self._CopyTestFiles('test3')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=False, dryrun=True)

    self._assertRaisesRegexp('.* begin/end markers .*',
                            manifest.CreateNewManifest)

  def testNonChromeProjectOverwritten(self):
    """Test case where """
    self._CopyTestFiles('test4')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=False, dryrun=True)
    self._assertRaisesRegexp('.* accidentally .*',
                            manifest.CreateNewManifest)

  def testParseInternalManifestWithNoCrosDEPS(self):
    """Test a scenario where no cros.DEPS projects are written to manifest."""
    self._CopyTestFiles('test5')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=True, dryrun=True)
    manifest.CreateNewManifest()
    assert(filecmp.cmp(manifest.new_manifest_path,
                       os.path.join(self.test_base, 'test5/expected.xml'),
                       shallow=False))

  def testParseExternalManifestCrosDEPSUnchanged(self):
    """Test that manifest stays the same given same cros.DEPS."""
    self._CopyTestFiles('test6')
    self.mox.StubOutWithMock(manifest_version, 'CreatePushBranch')
    manifest_version.CreatePushBranch(mox.IgnoreArg())
    self.mox.ReplayAll()
    manifest = chrome_update_manifest.Manifest(self.repo_root,
                                               self.manifest_path,
                                               '/does/not/exist',
                                               internal=False, dryrun=True)
    manifest.CreateNewManifest()
    assert(filecmp.cmp(manifest.new_manifest_path,
                       os.path.join(self.test_base, 'test6/expected.xml'),
                       shallow=False))

  def tearDown(self):
    os.chdir(self.old_dir)
    cros_lib.RunCommand(['rm', '-rf', self.tempdir, self.temp_manifest_dir])
    assert(not os.path.exists(self.repo_root))
    mox.MoxTestBase.tearDown(self)


if __name__ == '__main__':
  unittest.main()
