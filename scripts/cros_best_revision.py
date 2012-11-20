#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program examines success/fail history for Chrome/ium OS builds and
checks in a LKGM version for Chrome OS for other consumers.
"""

import distutils.version
import logging
import os
import shutil
import tempfile
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gclient
from chromite.lib import gs


class LKGMNotFound(Exception):
  """Raised if a newer valid LKGM could not be found."""


class ChromeCommitter(object):
  """Committer object responsible for obtaining a new LKGM and committing it."""

  _COMMIT_MSG = ('Automated Commit: Committing new LKGM version %(version)s '
                 'for chromeos.')
  _CANDIDATES_TO_CONSIDER = 8

  def __init__(self, checkout_dir, dryrun):
    self._checkout_dir = checkout_dir
    self._dryrun = dryrun
    self._lkgm = None
    self._old_lkgm = None

  def CheckoutChromeLKGM(self):
    """Checkout chromeos LKGM file for chrome into tmp checkout dir."""
    # We function only on an empty directory.
    lkgm_dir = '/'.join([gclient.CHROME_COMMITTER_URL,
                         constants.PATH_TO_CHROME_LKGM])

    # We checkout the bare necessities to get the .svn/file needed to commit.
    cros_build_lib.RunCommand(['svn', 'checkout', '--depth=empty', lkgm_dir,
                               self._checkout_dir])
    cros_build_lib.RunCommand(['svn', 'update', constants.CHROME_LKGM_FILE],
                              cwd=self._checkout_dir)

    with open(os.path.join(self._checkout_dir,
                           constants.CHROME_LKGM_FILE)) as fh:
      self._old_lkgm = fh.read()

  def _GetLatestCanaryVersions(self):
    """Returns the latest CANDIDATES_TO_CONSIDER canary versions."""
    gs_handle = gs.GSContext()
    version_paths = gs_handle.LS(
        manifest_version.BUILD_STATUS_URL).output.splitlines()

    # Strip gs://<path> prefix and trailing /'s.
    versions = [os.path.basename(v.rstrip('/')) for v in version_paths]

    lv = distutils.version.LooseVersion
    # We only care about canary versions which always end in 0.0.
    canary_versions = [v for v in versions if v.endswith('.0.0')]
    new_canary_versions = [v for v in canary_versions
                           if lv(v) > lv(self._old_lkgm)]
    return sorted(new_canary_versions, key=lv,
                  reverse=True)[0:self._CANDIDATES_TO_CONSIDER]

  def FindNewLKGM(self):
    """Finds a new LKGM for chrome from previous chromeos releases."""
    versions = self._GetLatestCanaryVersions()
    if not versions:
      raise LKGMNotFound('No valid LKGM found newer than the old lkgm.')

    canaries = cbuildbot_config.GetCanariesForChromeLKGM()
    logging.info('Considering the following versions %s', ' '.join(versions))
    logging.info('Using scores from the following canaries: %s',
                 ' '.join(canaries))

    # Scores are based on passing builders.
    version_scores = {}
    for version in versions:
      for builder in canaries:
        status = manifest_version.BuildSpecsManager.GetBuildStatus(
            builder, version, retries=0)
        if status:
          if status.Passed():
            version_scores[version] = version_scores.setdefault(version, 0) + 1
          elif status.Failed():
            # We don't consider builds with any reporting failures.
            version_scores[version] = 0
            break

      logging.info('Version %s had score %d', version,
                   version_scores.get(version, 0))

    # We want to get the version with the highest score. In case of a tie, we
    # want to choose the highest version.
    lkgm = max((v, k) for k, v in version_scores.iteritems())[1]
    if not version_scores[lkgm] > 0:
      raise LKGMNotFound('No valid LKGM found. Scores are too low.')

    self._lkgm = lkgm

  def CommitNewLKGM(self):
    """Commits the new LKGM file using our template commit message."""
    lv = distutils.version.LooseVersion
    if not self._lkgm and not lv(self._lkgm) < lv(self._old_lkgm):
      raise LKGMNotFound('No valid LKGM found. Did you run FindNewLKGM?')

    # Add the new versioned file.
    with open(os.path.join(self._checkout_dir,
                           constants.CHROME_LKGM_FILE), 'w') as fh:
      fh.write(self._lkgm)

    add_cmd = ['svn', 'add', constants.CHROME_LKGM_FILE]
    cros_build_lib.RunCommand(add_cmd, cwd=self._checkout_dir)

    # Commit it!
    commit_cmd = ['svn', 'commit', '--message',
                  self. _COMMIT_MSG % dict(version=self._lkgm)]

    # Sadly svn commit does not have a dryrun option.
    if not self._dryrun:
      cros_build_lib.RunCommand(commit_cmd, cwd=self._checkout_dir)
    else:
      logging.info('Would have run: %s', ' '.join(commit_cmd))


def _GetParser():
  """Returns the parser to use for this module."""
  parser = commandline.ArgumentParser(usage=__doc__)
  parser.add_argument('--dryrun', action='store_true', default=False,
                      help="Find the next LKGM but don't commit it.")
  return parser


def main(argv):
  parser = _GetParser()
  args = parser.parse_args(argv)

  checkout_dir = tempfile.mkdtemp(prefix='cbr_chrome_checkout')
  try:
    committer = ChromeCommitter(checkout_dir, dryrun=args.dryrun)
    committer.CheckoutChromeLKGM()
    committer.FindNewLKGM()
    committer.CommitNewLKGM()
    return 0
  finally:
    shutil.rmtree(checkout_dir)
