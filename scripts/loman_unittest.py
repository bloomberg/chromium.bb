# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for loman.py"""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import loman


class ParserTest(cros_test_lib.OutputTestCase):
  """Tests for the CLI parser."""

  def setUp(self):
    self.parser = loman.GetParser()

  def testNoCommand(self):
    """Require a command at least."""
    with self.OutputCapturer():
      self.assertRaises(SystemExit, self.parser.parse_args, [])

  def testBadCommand(self):
    """Reject unknown commands."""
    with self.OutputCapturer():
      self.assertRaises(SystemExit, self.parser.parse_args, ['flyaway'])

  def testAddCommand(self):
    """Verify basic add command behavior."""
    with self.OutputCapturer():
      self.parser.parse_args(['add', '--workon', 'project'])
      self.parser.parse_args(['add', 'project', 'path', '--remote', 'foo'])

  def testUpgradeMinilayoutCommand(self):
    """Verify basic upgrade command behavior."""
    with self.OutputCapturer():
      self.parser.parse_args(['upgrade-minilayout'])


class ManifestTest(cros_test_lib.TempDirTestCase):
  """Tests that need a real .repo/ manifest layout."""

  def setUp(self):
    # The loman code looks for the repo root, so make one, and chdir there.
    os.chdir(self.tempdir)

    for d in ('repo', 'manifests', 'manifests.git'):
      osutils.SafeMakedirs(os.path.join('.repo', d))

    for m in ('default.xml', 'full.xml', 'minilayout.xml'):
      osutils.Touch(os.path.join('.repo', 'manifests', m))

    self._SetManifest('default.xml')

  def _SetManifest(self, manifest):
    """Set active manifest to point to |manifest|."""
    source = os.path.join('.repo', 'manifest.xml')
    target = os.path.join('.repo', 'manifests', manifest)
    osutils.SafeUnlink(source)
    os.symlink(target, source)


class UpgradeMinilayoutTest(cros_test_lib.MockOutputTestCase, ManifestTest):
  """Tests for the upgrade minilayout logic."""

  def testNoUpgradeNeeded(self):
    """Don't run update when it isn't needed."""
    with self.OutputCapturer():
      self.PatchObject(loman, '_UpgradeMinilayout', side_effect=Exception)
      loman.main(['upgrade-minilayout'])

  def testUpgradeNeeded(self):
    """Run update when it's needed."""
    class _Exception(Exception):
      """Custom exception."""

    self._SetManifest('minilayout.xml')
    with self.OutputCapturer():
      self.PatchObject(loman, '_UpgradeMinilayout', side_effect=_Exception)
      self.assertRaises(_Exception, loman.main, ['upgrade-minilayout'])

  def testAutoUpgrade(self):
    """Run update automatically."""
    class _Exception(Exception):
      """Custom exception."""

    self._SetManifest('minilayout.xml')
    with self.OutputCapturer():
      self.PatchObject(loman, '_UpgradeMinilayout', side_effect=_Exception)
      self.assertRaises(_Exception, loman.main, ['add', '--workon', 'proj'])


class AddTest(cros_test_lib.MockOutputTestCase, ManifestTest):
  """Tests for the add command."""

  def testRejectBadCommands(self):
    """Reject bad invocations."""
    bad_cmds = (
        # Missing path.
        ['add'],
        # Extra project.
        ['add', '--workon', 'path', 'project'],
        # Missing --remote.
        ['add', 'path', 'project'],
        # Missing project.
        ['add', 'path', '--remote', 'remote'],
    )
    with self.OutputCapturer():
      for cmd in bad_cmds:
        self.assertRaises(SystemExit, loman.main, cmd)
