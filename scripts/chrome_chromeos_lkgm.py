# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the CHROMEOS_LKGM file in a chromium repository."""

from __future__ import print_function

import distutils.version
import os

from chromite.cbuildbot import manifest_version
from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import tree_status


class LKGMNotValid(Exception):
  """Raised if the LKGM version is unset or not newer than the current value."""


class LKGMNotCommitted(Exception):
  """Raised if we could not submit a new LKGM."""


class ChromeLKGMCommitter(object):
  """Committer object responsible for obtaining a new LKGM and committing it."""

  _COMMIT_MSG_TEMPLATE = ('Automated Commit: New LKGM version '
                          '%(version)s for chromeos.\n\nBUG=762641')

  def __init__(self, args):
    self._checkout_dir = args.workdir
    # Strip any chrome branch from the lkgm version.
    self._lkgm = manifest_version.VersionInfo(args.lkgm).VersionString()
    self._dryrun = args.dryrun
    self._git_committer_args = ['-c', 'user.email=%s' % args.user_email,
                                '-c', 'user.name=%s' % args.user_email]
    self._commit_msg = ''
    self._old_lkgm = None

    if not self._lkgm:
      raise LKGMNotValid('LKGM not provided.')

    logging.info('lkgm=%s', args.lkgm)
    logging.info('user_email=%s', args.user_email)
    logging.info('checkout_dir=%s', args.workdir)

  def __del__(self):
    self.Cleanup()

  def Run(self):
    self.Cleanup()
    self.CheckoutChrome()
    self.UpdateLKGM()
    self.CommitNewLKGM()
    self.UploadNewLKGM()

  def CheckoutChrome(self):
    """Checks out chrome into tmp checkout_dir."""
    git.ShallowFetch(self._checkout_dir, constants.CHROMIUM_GOB_URL,
                     sparse_checkout=['codereview.settings',
                                      constants.PATH_TO_CHROME_LKGM])
    git.CreateBranch(self._checkout_dir, 'lkgm-roll',
                     branch_point='origin/master')

  @property
  def lkgm_file(self):
    return os.path.join(self._checkout_dir, constants.PATH_TO_CHROME_LKGM)

  def UpdateLKGM(self):
    """Updates the LKGM file with the new version."""
    self._old_lkgm = osutils.ReadFile(
        os.path.join(self._checkout_dir, constants.PATH_TO_CHROME_LKGM))

    lv = distutils.version.LooseVersion
    if self._old_lkgm is not None and not lv(self._lkgm) > lv(self._old_lkgm):
      raise LKGMNotValid(
          'LKGM version (%s) is not newer than current version (%s).' %
          (self._lkgm, self._old_lkgm))

    logging.info('Updating LKGM version: %s (was %s),',
                 self._lkgm, self._old_lkgm)
    osutils.WriteFile(self.lkgm_file, self._lkgm)

  def CommitNewLKGM(self):
    """Commits the new LKGM file using our template commit message."""
    self._commit_msg = self._COMMIT_MSG_TEMPLATE % dict(version=self._lkgm)
    try:
      git.AddPath(self.lkgm_file)
      commit_args = ['commit', '-m', self._commit_msg]
      git.RunGit(self._checkout_dir, self._git_committer_args + commit_args,
                 print_cmd=True, redirect_stderr=True, capture_output=False)
    except cros_build_lib.RunCommandError as e:
      raise LKGMNotCommitted(
          'Could not create git commit with new LKGM: %r' % e)

  def UploadNewLKGM(self):
    """Uploads the change to gerrit."""
    logging.info('Uploading LKGM commit.')

    try:
      # Run 'git cl upload' with --bypass-hooks to skip running scripts that are
      # not part of the shallow checkout, -f to skip editing the CL message,
      upload_args = ['cl', 'upload', '-v', '-m', self._commit_msg,
                     '--bypass-hooks', '-f']
      if not self._dryrun:
        # Add the gardener(s) as TBR; fall-back to tbr-owners.
        gardeners = tree_status.GetSheriffEmailAddresses('chrome')
        if gardeners:
          for tbr in gardeners:
            upload_args += ['--tbrs', tbr]
        else:
          upload_args += ['--tbr-owners']
        # Marks CL as ready.
        upload_args += ['--send-mail']
      git.RunGit(self._checkout_dir, self._git_committer_args + upload_args,
                 print_cmd=True, redirect_stderr=True, capture_output=False)

      # Flip the CQ commit bit.
      submit_args = ['cl', 'set-commit', '-v']
      if self._dryrun:
        submit_args += ['--dry-run']
      git.RunGit(self._checkout_dir, submit_args,
                 print_cmd=True, redirect_stderr=True, capture_output=False)
    except cros_build_lib.RunCommandError as e:
      # Log the change for debugging.
      git.RunGit(self._checkout_dir, ['--no-pager', 'log', '--pretty=full'],
                 capture_output=False)
      raise LKGMNotCommitted('Could not submit LKGM: %r' % e)

    logging.info('LKGM submitted to CQ.')

  def Cleanup(self):
    """Remove chrome checkout."""
    if hasattr(self, '_checkout_dir'):
      osutils.RmDir(self._checkout_dir, ignore_missing=True)

def GetArgs(argv):
  """Returns a dictionary of parsed args.

  Args:
    argv: raw command line.

  Returns:
    Dictionary of parsed args.
  """
  parser = commandline.ArgumentParser(usage=__doc__, caching=True)
  parser.add_argument('--dryrun', action='store_true', default=False,
                      help="Find the next LKGM but don't commit it.")
  parser.add_argument('--lkgm', required=True,
                      help="LKGM version to update to.")
  parser.add_argument('--user_email', required=False,
                      default='chromeos-commit-bot@chromium.org',
                      help="Email address to use when comitting changes.")
  parser.add_argument('--workdir',
                      default=os.path.join(os.getcwd(), 'chrome_src'),
                      help=('Path to a checkout of the chrome src. '
                            'Defaults to PWD/chrome_src'))
  return parser.parse_args(argv)

def main(argv):
  ChromeLKGMCommitter(GetArgs(argv)).Run()
  return 0
