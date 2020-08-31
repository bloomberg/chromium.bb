# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the CHROMEOS_LKGM file in a chromium repository."""

from __future__ import print_function

import distutils.version  # pylint: disable=import-error,no-name-in-module
import os
import sys

from chromite.cbuildbot import manifest_version
from chromite.lib import chrome_committer
from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import gerrit
from chromite.lib import osutils


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class LKGMNotValid(chrome_committer.CommitError):
  """Raised if the LKGM version is unset or not newer than the current value."""


class LKGMFileNotFound(chrome_committer.CommitError):
  """Raised if the LKGM file is not found."""


class ChromeLKGMCommitter(object):
  """Committer object responsible for obtaining a new LKGM and committing it."""

  # The list of trybots we require LKGM updates to run and pass on before
  # landing. Since they're internal trybots, the CQ won't automatically trigger
  # them, so we have to explicitly tell it to.
  _PRESUBMIT_BOTS = [
      'chromeos-betty-pi-arc-chrome',
      'chromeos-eve-compile-chrome',
      'chromeos-kevin-compile-chrome',
  ]
  # Files needed in a local checkout to successfully update the LKGM. The OWNERS
  # file allows the --tbr-owners mechanism to select an appropriate OWNER to
  # TBR. TRANSLATION_OWNERS is necesssary to parse CHROMEOS_OWNERS file since
  # it has the reference.
  _NEEDED_FILES = [
      constants.PATH_TO_CHROME_CHROMEOS_OWNERS,
      constants.PATH_TO_CHROME_LKGM,
      'tools/translation/TRANSLATION_OWNERS',
  ]

  def __init__(self, user_email, workdir, lkgm, dryrun=False):
    self._committer = chrome_committer.ChromeCommitter(
        user_email, workdir, dryrun)

    # Strip any chrome branch from the lkgm version.
    self._lkgm = manifest_version.VersionInfo(lkgm).VersionString()
    self._old_lkgm = None

    if not self._lkgm:
      raise LKGMNotValid('LKGM not provided.')
    logging.info('lkgm=%s', lkgm)

  def Run(self):
    self.CloseOldLKGMRolls()
    self._committer.Cleanup()
    self._committer.Checkout(self._NEEDED_FILES)
    self.UpdateLKGM()
    self.CommitNewLKGM()
    self._committer.Upload()

  def CheckoutChrome(self):
    """Checks out chrome into tmp checkout_dir."""
    self._committer.Checkout(self._NEEDED_FILES)

  @property
  def lkgm_file(self):
    return self._committer.FullPath(constants.PATH_TO_CHROME_LKGM)

  def CloseOldLKGMRolls(self):
    """Closes all open LKGM roll CLs that were last modified >24 hours ago.

    Any roll that hasn't passed the CQ in 24 hours is likely broken and can be
    discarded.
    """
    query_params = {
        'project': constants.CHROMIUM_SRC_PROJECT,
        'branch': 'master',
        'author': self._committer.author,
        'file': constants.PATH_TO_CHROME_LKGM,
        'age': '1d',
        'status': 'open',
    }
    gerrit_helper = gerrit.GetCrosExternal()
    for open_issue in gerrit_helper.Query(**query_params):
      logging.info(
          'Closing old LKGM roll crrev.com/c/%s', open_issue.gerrit_number)
      gerrit_helper.AbandonChange(
          open_issue, msg='Superceded by LKGM %s' % self._lkgm)

  def UpdateLKGM(self):
    """Updates the LKGM file with the new version."""
    lkgm_file = self.lkgm_file
    if not os.path.exists(lkgm_file):
      raise LKGMFileNotFound('%s is an invalid file' % lkgm_file)

    self._old_lkgm = osutils.ReadFile(lkgm_file)

    lv = distutils.version.LooseVersion
    if self._old_lkgm is not None and lv(self._lkgm) <= lv(self._old_lkgm):
      raise LKGMNotValid(
          'LKGM version (%s) is not newer than current version (%s).' %
          (self._lkgm, self._old_lkgm))

    logging.info('Updating LKGM version: %s (was %s),',
                 self._lkgm, self._old_lkgm)
    osutils.WriteFile(lkgm_file, self._lkgm)

  def ComposeCommitMsg(self):
    """Constructs and returns the commit message for the LKGM update."""
    commit_msg_template = (
        'LKGM %(version)s for chromeos.'
        '\n\n%(cq_includes)s'
        '\nBUG=762641')
    cq_includes = ''
    for bot in self._PRESUBMIT_BOTS:
      cq_includes += 'CQ_INCLUDE_TRYBOTS=luci.chrome.try:%s\n' % bot
    return commit_msg_template % dict(
        version=self._lkgm, cq_includes=cq_includes)

  def CommitNewLKGM(self):
    """Commits the new LKGM file using our template commit message."""
    self._committer.Commit([constants.PATH_TO_CHROME_LKGM],
                           self.ComposeCommitMsg())


def GetOpts(argv):
  """Returns a dictionary of parsed options.

  Args:
    argv: raw command line.

  Returns:
    Dictionary of parsed options.
  """
  committer_parser = chrome_committer.ChromeCommitter.GetParser()
  parser = commandline.ArgumentParser(description=__doc__,
                                      parents=[committer_parser],
                                      add_help=False, logging=False)
  parser.add_argument('--lkgm', required=True,
                      help='LKGM version to update to.')
  return parser.parse_args(argv)

def main(argv):
  opts = GetOpts(argv)
  committer = ChromeLKGMCommitter(opts.user_email, opts.workdir,
                                  opts.lkgm, opts.dryrun)
  committer.Run()
  return 0
