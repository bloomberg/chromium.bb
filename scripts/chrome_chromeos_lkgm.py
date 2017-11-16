# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the CHROMEOS_LKGM file in a chromium repository.

Note: this borrows heavily from cros_best_revision.py. TODO(stevenjb) Remove
cros_best_revision.py once this replaces it and the Chrome LKGM builder is
turned down.
"""

from __future__ import print_function

import distutils.version
import os

from chromite.cbuildbot import manifest_version
from chromite.lib import tree_status
from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gclient
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import retry_util


class LKGMNotValid(Exception):
  """Raised if the LKGM version is unset or not newer than the current value."""


class LKGMNotCommitted(Exception):
  """Raised if we could not submit a new LKGM."""


class ChromeLGTMCommitter(object):
  """Committer object responsible for obtaining a new LKGM and committing it."""

  _COMMIT_MSG_TEMPLATE = ('Automated Commit: Committing new LKGM version '
                          '%(version)s for chromeos.')
  _SLEEP_TIMEOUT = 30
  _TREE_TIMEOUT = 7200

  def __init__(self, checkout_dir, lkgm, dryrun, user_email):
    self._checkout_dir = checkout_dir
    # Strip any chrome branch from the lkgm version.
    self._lkgm = manifest_version.VersionInfo(lkgm).VersionString()
    self._dryrun = dryrun
    self._git_committer_args = ['-c', 'user.email=%s' % user_email,
                                '-c', 'user.name=%s' % user_email]
    self._commit_msg = ''
    self._old_lkgm = None

  def CheckoutChromeLKGM(self):
    """Checkout CHROMEOS_LKGM file for chrome into tmp checkout dir."""
    # TODO(stevenjb): if checkout_dir exists, use 'fetch --depth 1' and
    # 'checkout -f origin/master' instead of 'clone --depth 1'.
    osutils.RmDir(self._checkout_dir, ignore_missing=True)

    cros_build_lib.RunCommand(
        ['git', 'clone', '--depth', '1', constants.CHROMIUM_GOB_URL,
         self._checkout_dir])

    cros_build_lib.RunCommand(
        ['git', 'branch', '-D', 'lkgm-roll'], cwd=self._checkout_dir,
        error_code_ok=True)
    cros_build_lib.RunCommand(
        ['git', 'checkout', '-b', 'lkgm-roll', 'origin/master'],
        cwd=self._checkout_dir)

    self._old_lkgm = osutils.ReadFile(
        os.path.join(self._checkout_dir, constants.PATH_TO_CHROME_LKGM))

  def CommitNewLKGM(self):
    """Commits the new LKGM file using our template commit message."""
    if not self._lkgm:
      raise LKGMNotValid('LKGM not provided.')

    lv = distutils.version.LooseVersion
    if self._old_lkgm is not None and not lv(self._lkgm) > lv(self._old_lkgm):
      raise LKGMNotValid('LKGM version is not newer than current version.')

    logging.info('Committing LKGM version: %s (was %s),',
                 self._lkgm, self._old_lkgm)
    self._commit_msg = self._COMMIT_MSG_TEMPLATE % dict(version=self._lkgm)
    checkout_dir = self._checkout_dir
    try:
      # Overwrite the lkgm file and commit it.
      file_path = os.path.join(checkout_dir, constants.PATH_TO_CHROME_LKGM)
      osutils.WriteFile(file_path, self._lkgm)
      git.AddPath(file_path)
      commit_args = ['commit', '-m', self._commit_msg]
      git.RunGit(checkout_dir, self._git_committer_args + commit_args,
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
      # --send-mail to mark the CL as ready, and --tbrs to +1 the CL.
      upload_args = ['cl', 'upload', '-v', '-m', self._commit_msg,
                     '--bypass-hooks', '-f']
      if not self._dryrun:
        upload_args += ['--send-mail', '--tbrs=chrome-os-gardeners@google.com']
      git.RunGit(self._checkout_dir, self._git_committer_args + upload_args,
                 print_cmd=True, redirect_stderr=True, capture_output=False)
    except cros_build_lib.RunCommandError as e:
      # Log the change for debugging.
      cros_build_lib.RunCommand(['git', 'log', '--pretty=full'],
                                cwd=self._checkout_dir)
      raise LKGMNotCommitted('Could not submit LKGM: upload failed: %r' % e)

  def _TryLandNewLKGM(self):
    """Fetches latest, rebases the CL, and lands the rebased CL."""
    git.RunGit(self._checkout_dir, ['fetch', 'origin', 'master'])

    try:
      git.RunGit(self._checkout_dir, ['rebase'], retry=False)
    except cros_build_lib.RunCommandError as e:
      # A rebase failure was unexpected, so raise a custom LKGMNotCommitted
      # error to avoid further retries.
      git.RunGit(self._checkout_dir, ['rebase', '--abort'])
      raise LKGMNotCommitted('Could not submit LKGM: rebase failed: %r' % e)

    if self._dryrun:
      logging.info('Dry run; rebase succeeded, exiting.')
      return

    git.RunGit(self._checkout_dir, ['cl', 'land', '-f', '--bypass-hooks'])

  def LandNewLKGM(self, max_retry=10):
    """Lands the change after fetching and rebasing."""
    if not tree_status.IsTreeOpen(status_url=gclient.STATUS_URL,
                                  period=self._SLEEP_TIMEOUT,
                                  timeout=self._TREE_TIMEOUT):
      raise LKGMNotCommitted('Chromium Tree is closed')

    logging.info('Landing LKGM commit.')

    # git cl land refuses to land a change that isn't relative to ToT, so any
    # new commits since the last fetch will cause this to fail. Retries should
    # make this edge case ultimately unlikely.
    try:
      retry_util.RetryCommand(self._TryLandNewLKGM,
                              max_retry,
                              sleep=self._SLEEP_TIMEOUT,
                              log_all_retries=True)
    except cros_build_lib.RunCommandError as e:
      raise LKGMNotCommitted('Could not submit LKGM: %r' % e)

    logging.info('git cl land succeeded.')

def _GetParser():
  """Returns the parser to use for this module."""
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
  return parser

def main(argv):
  parser = _GetParser()
  args = parser.parse_args(argv)

  logging.info('lkgm=%s', args.lkgm)
  logging.info('user_email=%s', args.user_email)
  logging.info('workdir=%s', args.workdir)

  committer = ChromeLGTMCommitter(args.workdir, lkgm=args.lkgm,
                                  dryrun=args.dryrun,
                                  user_email=args.user_email)
  try:
    committer.CheckoutChromeLKGM()
    committer.CommitNewLKGM()
    committer.UploadNewLKGM()
    committer.LandNewLKGM()
  except LKGMNotCommitted as e:
    # TODO(stevenjb): Do not catch exceptions (i.e. fail) once this works.
    logging.warning('LKGM Commit failed: %r' % e)

  return 0
