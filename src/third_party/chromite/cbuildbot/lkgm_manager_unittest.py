# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for lkgm_manager"""

from __future__ import print_function

import contextlib
import os
import tempfile
from xml.dom import minidom

import mock

from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib.buildstore import FakeBuildStore


FAKE_VERSION_STRING = '1.2.4-rc3'
FAKE_VERSION_STRING_NEXT = '1.2.4-rc4'
CHROME_BRANCH = '13'

FAKE_VERSION = """
CHROMEOS_BUILD=1
CHROMEOS_BRANCH=2
CHROMEOS_PATCH=4
CHROME_BRANCH=13
"""


# pylint: disable=protected-access


class LKGMCandidateInfoTest(cros_test_lib.TestCase):
  """Test methods testing methods in _LKGMCandidateInfo class."""

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING,
                                           chrome_branch=CHROME_BRANCH)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a lkgm info."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING,
                                           chrome_branch=CHROME_BRANCH)
    info.IncrementVersion()
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING_NEXT)

  def testVersionCompare(self):
    """Tests whether our comparision method works."""
    info0 = lkgm_manager._LKGMCandidateInfo('5.2.3-rc100')
    info1 = lkgm_manager._LKGMCandidateInfo('1.2.3-rc1')
    info2 = lkgm_manager._LKGMCandidateInfo('1.2.3-rc2')
    info3 = lkgm_manager._LKGMCandidateInfo('1.2.200-rc1')
    info4 = lkgm_manager._LKGMCandidateInfo('1.4.3-rc1')

    self.assertGreater(info0, info1)
    self.assertGreater(info0, info2)
    self.assertGreater(info0, info3)
    self.assertGreater(info0, info4)
    self.assertGreater(info2, info1)
    self.assertGreater(info3, info1)
    self.assertGreater(info3, info2)
    self.assertGreater(info4, info1)
    self.assertGreater(info4, info2)
    self.assertGreater(info4, info3)
    self.assertEqual(info0, info0)
    self.assertEqual(info1, info1)
    self.assertEqual(info2, info2)
    self.assertEqual(info3, info3)
    self.assertEqual(info4, info4)
    self.assertNotEqual(info0, info1)
    self.assertNotEqual(info0, info2)
    self.assertNotEqual(info0, info3)
    self.assertNotEqual(info0, info4)
    self.assertNotEqual(info1, info0)
    self.assertNotEqual(info1, info2)
    self.assertNotEqual(info1, info3)
    self.assertNotEqual(info1, info4)
    self.assertNotEqual(info2, info0)
    self.assertNotEqual(info2, info1)
    self.assertNotEqual(info2, info3)
    self.assertNotEqual(info2, info4)
    self.assertNotEqual(info3, info0)
    self.assertNotEqual(info3, info1)
    self.assertNotEqual(info3, info2)
    self.assertNotEqual(info3, info4)
    self.assertNotEqual(info4, info0)
    self.assertNotEqual(info4, info1)
    self.assertNotEqual(info4, info1)
    self.assertNotEqual(info4, info3)


@contextlib.contextmanager
def TemporaryManifest():
  with tempfile.NamedTemporaryFile(mode='w') as f:
    # Create fake but empty manifest file.
    new_doc = minidom.getDOMImplementation().createDocument(
        None, 'manifest', None)
    print(new_doc.toxml())
    new_doc.writexml(f)
    f.flush()
    yield f


class LKGMManagerTest(cros_test_lib.MockTempDirTestCase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    self.push_mock = self.PatchObject(git, 'CreatePushBranch')

    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'amd64-generic'
    self.incr_type = 'branch'
    self.buildstore = FakeBuildStore()

    # Create tmp subdirs based on the one provided TempDirMixin.
    self.tmpdir = os.path.join(self.tempdir, 'base')
    osutils.SafeMakedirs(self.tmpdir)
    self.tmpmandir = os.path.join(self.tempdir, 'man')
    osutils.SafeMakedirs(self.tmpmandir)

    repo = repository.RepoRepository(
        self.source_repo, self.tmpdir, self.branch, depth=1)
    self.manager = lkgm_manager.LKGMManager(
        repo, self.manifest_repo, self.build_name, constants.PFQ_TYPE, 'branch',
        force=False, branch=self.branch, buildstore=self.buildstore,
        dry_run=True)
    self.manager.manifest_dir = self.tmpmandir
    self.manager.lkgm_path = os.path.join(
        self.tmpmandir, constants.LKGM_MANIFEST)

    self.manager.all_specs_dir = '/LKGM/path'
    manifest_dir = self.manager.manifest_dir
    self.manager.specs_for_builder = os.path.join(manifest_dir,
                                                  self.manager.rel_working_dir,
                                                  'build-name', '%(builder)s')
    self.manager.SLEEP_TIMEOUT = 0

  def _GetPathToManifest(self, info):
    return os.path.join(self.manager.all_specs_dir, '%s.xml' %
                        info.VersionString())

  def testCreateFromManifest(self):
    """Tests that we can create a new candidate from another manifest."""
    # Let's stub out other LKGMManager calls cause they're already
    # unit tested.

    version = '2010.0.0-rc7'
    my_info = lkgm_manager._LKGMCandidateInfo('2010.0.0')
    new_candidate = lkgm_manager._LKGMCandidateInfo(version)
    manifest = ('/tmp/manifest-versions-internal/paladin/buildspecs/'
                '20/%s.xml' % version)
    new_manifest = '/path/to/tmp/file.xml'

    build_id = 20162

    site_params = config_lib.GetSiteParams()
    # Patch out our RepoRepository to make sure we don't corrupt real repo.
    self.PatchObject(self.manager, 'cros_source')
    filter_mock = self.PatchObject(manifest_version, 'FilterManifest',
                                   return_value=new_manifest)

    # Do manifest refresh work.
    self.PatchObject(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo',
                     return_value=my_info)
    self.PatchObject(lkgm_manager.LKGMManager, 'RefreshManifestCheckout')
    init_mock = self.PatchObject(lkgm_manager.LKGMManager,
                                 'InitializeManifestVariables')

    # Publish new candidate.
    publish_mock = self.PatchObject(lkgm_manager.LKGMManager, 'PublishManifest')

    candidate_path = self.manager.CreateFromManifest(manifest,
                                                     build_id=build_id)
    self.assertEqual(candidate_path, self._GetPathToManifest(new_candidate))
    self.assertEqual(self.manager.current_version, version)

    filter_mock.assert_called_once_with(
        manifest, whitelisted_remotes=site_params.EXTERNAL_REMOTES)
    publish_mock.assert_called_once_with(new_manifest, version,
                                         build_id=build_id)
    init_mock.assert_called_once_with(my_info)
    self.push_mock.assert_called_once_with(mock.ANY, mock.ANY, sync=False)

  def testCreateNewCandidateReturnNoneIfNoWorkToDo(self):
    """Tests that we return nothing if there is nothing to create."""
    new_manifest = 'some_manifest'
    my_info = lkgm_manager._LKGMCandidateInfo('1.2.3')

    # Patch out our RepoRepository to make sure we don't corrupt real repo.
    cros_source_mock = self.PatchObject(self.manager, 'cros_source')
    cros_source_mock.branch = 'master'
    cros_source_mock.directory = '/foo/repo'

    self.PatchObject(lkgm_manager.LKGMManager, 'CheckoutSourceCode')
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateManifest',
                     return_value=new_manifest)
    self.PatchObject(lkgm_manager.LKGMManager, 'RefreshManifestCheckout')
    self.PatchObject(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo',
                     return_value=my_info)
    init_mock = self.PatchObject(lkgm_manager.LKGMManager,
                                 'InitializeManifestVariables')
    self.PatchObject(lkgm_manager.LKGMManager, 'HasCheckoutBeenBuilt',
                     return_value=True)

    candidate = self.manager.CreateNewCandidate()
    self.assertEqual(candidate, None)
    init_mock.assert_called_once_with(my_info)

  def _CreateManifest(self):
    """Returns a created test manifest in tmpdir with its dir_pfx."""
    self.manager.current_version = '1.2.4-rc21'
    dir_pfx = CHROME_BRANCH
    manifest = os.path.join(self.manager.manifest_dir,
                            self.manager.rel_working_dir, 'buildspecs',
                            dir_pfx, '1.2.4-rc21.xml')
    osutils.Touch(manifest)
    return manifest, dir_pfx

  def _MockParseGitLog(self, fake_git_log, project):
    exists_mock = self.PatchObject(os.path, 'exists', return_value=True)
    link_mock = self.PatchObject(logging, 'PrintBuildbotLink')
    fake_project_handler = mock.Mock(spec=git.Manifest)
    fake_project_handler.checkouts_by_path = {project['path']: project}
    self.PatchObject(git, 'Manifest', return_value=fake_project_handler)

    fake_result = cros_build_lib.CommandResult(output=fake_git_log)
    self.PatchObject(git, 'RunGit', return_value=fake_result)

    return exists_mock, link_mock

  def testGenerateBlameListSinceLKGM(self):
    """Tests that we can generate a blamelist from two commit messages.

    This test tests the functionality of generating a blamelist for a git log.
    Note in this test there are two commit messages, one commited by the
    Commit Queue and another from Non-Commit Queue.  We test the correct
    handling in both cases.
    """
    fake_git_log = """commit abcd
Author: Sammy Sosa <fake@fake.com>
Commit: Chris Sosa <sosa@chromium.org>

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: https://chromium-review.googlesource.com/1234
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>

commit ef01
Author: Sammy Sosa <fake@fake.com>
Commit: Gerrit <chrome-bot@chromium.org>

    Add in a test for cbuildbot

    Random line that says "Author:" in the message:
    Author: _Not_ Sammy Sosa <veryfake@fake.com>

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: https://chromium-review.googlesource.com/1235
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    """
    project = {
        'name': 'fake/repo',
        'path': 'fake/path',
        'revision': '1234567890',
    }
    self.manager.incr_type = 'build'
    self.PatchObject(cros_build_lib, 'run', side_effect=Exception())
    exists_mock, link_mock = self._MockParseGitLog(fake_git_log, project)
    self.manager.GenerateBlameListSinceLKGM()

    exists_mock.assert_called_once_with(
        os.path.join(self.tmpdir, project['path']))
    link_mock.assert_has_calls([
        mock.call('CHUMP | repo | fake | 1234',
                  'https://chromium-review.googlesource.com/1234'),
        mock.call('repo | fake | 1235',
                  'https://chromium-review.googlesource.com/1235'),
    ])

  def testGenerateBlameListHasChumpCL(self):
    """Test GenerateBlameList with chump CLs."""
    fake_git_log = """
commit 1234
Author: Sammy Sosa <fake@fake.com>
Commit: Chris Sosa <sosa@chromium.org>

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: https://chromium-review.googlesource.com/1234
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    """
    project = {
        'name': 'fake/repo',
        'path': 'fake/path',
        'revision': '1234567890',
    }
    _, link_mock = self._MockParseGitLog(fake_git_log, project)
    has_chump_cls = lkgm_manager.GenerateBlameList(
        self.manager.cros_source, self.manager.lkgm_path)

    self.assertTrue(has_chump_cls)
    link_mock.assert_has_calls([
        mock.call('CHUMP | repo | fake | 1234',
                  'https://chromium-review.googlesource.com/1234')])

  def testGenerateBlameListNoChumpCL(self):
    """Test GenerateBlameList without chump CLs."""
    fake_git_log = """
commit 5678
Author: Sammy Sosa <fake@fake.com>
Commit: Gerrit <chrome-bot@chromium.org>

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: https://chromium-review.googlesource.com/1235
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    """
    project = {
        'name': 'fake/repo',
        'path': 'fake/path',
        'revision': '1234567890',
    }
    _, link_mock = self._MockParseGitLog(fake_git_log, project)
    has_chump_cl = lkgm_manager.GenerateBlameList(
        self.manager.cros_source, self.manager.lkgm_path)

    self.assertFalse(has_chump_cl)
    link_mock.assert_has_calls([
        mock.call('repo | fake | 1235',
                  'https://chromium-review.googlesource.com/1235')])

  def testAddChromeVersionToManifest(self):
    """Tests whether we can write the chrome version to the manifest file."""
    with TemporaryManifest() as f:
      chrome_version = '35.0.1863.0'
      # Write the chrome element to manifest.
      self.manager._AddChromeVersionToManifest(f.name, chrome_version)

      # Read the manifest file.
      new_doc = minidom.parse(f.name)
      elements = new_doc.getElementsByTagName(lkgm_manager.CHROME_ELEMENT)
      self.assertEqual(len(elements), 1)
      self.assertEqual(
          elements[0].getAttribute(lkgm_manager.CHROME_VERSION_ATTR),
          chrome_version)
