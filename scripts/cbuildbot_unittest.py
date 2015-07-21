# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cbuildbot program"""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.scripts import cbuildbot


# pylint: disable=protected-access


class IsDistributedBuilderTest(cros_test_lib.TestCase):
  """Test for cbuildbot._IsDistributedBuilder."""

  # pylint: disable=W0212
  def testIsDistributedBuilder(self):
    """Tests for _IsDistributedBuilder() under various configurations."""
    parser = cbuildbot._CreateParser()
    argv = ['x86-generic-paladin']
    (options, _) = cbuildbot._ParseCommandLine(parser, argv)
    options.buildbot = False

    build_config = dict(pre_cq=False,
                        manifest_version=False)
    chrome_rev = None

    def _TestConfig(expected):
      self.assertEquals(expected,
                        cbuildbot._IsDistributedBuilder(
                            options=options,
                            chrome_rev=chrome_rev,
                            build_config=build_config))

    # Default options.
    _TestConfig(False)

    build_config['pre_cq'] = True
    _TestConfig(True)

    build_config['pre_cq'] = False
    build_config['manifest_version'] = True
    # Not running in buildbot mode even though manifest_version=True.
    _TestConfig(False)
    options.buildbot = True
    _TestConfig(True)

    for chrome_rev in (constants.CHROME_REV_TOT,
                       constants.CHROME_REV_LOCAL,
                       constants.CHROME_REV_SPEC):
      _TestConfig(False)


class FetchInitialBootstrapConfigRepoTest(cros_test_lib.MockTempDirTestCase):
  """Test for cbuildbot._FetchInitialBootstrapConfig."""


  def setUp(self):
    self.config_dir = os.path.join(self.tempdir, 'config')

    self.PatchObject(constants, "SOURCE_ROOT", self.tempdir)
    self.PatchObject(constants, "SITE_CONFIG_DIR", self.config_dir)
    self.mockGit = self.PatchObject(git, "RunGit")

  def testDoesClone(self):
    # Test
    cbuildbot._FetchInitialBootstrapConfigRepo('repo_url', None)
    # Verify
    self.mockGit.assert_called_once_with(
        self.config_dir, ['clone', 'repo_url', self.config_dir])

  def testDoesCloneBranch(self):
    # Test
    cbuildbot._FetchInitialBootstrapConfigRepo('repo_url', 'test_branch')
    # Verify
    self.assertEqual(
        self.mockGit.mock_calls,
        [mock.call(self.config_dir, ['clone', 'repo_url', self.config_dir]),
         mock.call(self.config_dir, ['checkout', 'test_branch'])])

  def testNoCloneForRepo(self):
    # Setup
    os.mkdir(os.path.join(self.tempdir, '.repo'))
    # Test
    cbuildbot._FetchInitialBootstrapConfigRepo('repo_url', None)
    # Verify
    self.assertEqual(self.mockGit.call_count, 0)

  def testNoCloneIfExists(self):
    # Setup
    os.mkdir(os.path.join(self.tempdir, 'config'))
    # Test
    cbuildbot._FetchInitialBootstrapConfigRepo('repo_url', None)
    # Verify
    self.assertEqual(self.mockGit.call_count, 0)
