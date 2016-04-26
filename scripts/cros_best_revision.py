# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Examine success/fail history for Chrome/ium OS builds.

Used to check in a LKGM version for Chrome OS for other consumers.
"""

from __future__ import print_function

import distutils.version
import os

from chromite.cbuildbot import archive_lib
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import tree_status
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gclient
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel


class LKGMNotFound(Exception):
  """Raised if a newer valid LKGM could not be found."""


class LKGMNotCommitted(Exception):
  """Raised if we could not submit a new LKGM."""


class ChromeCommitter(object):
  """Committer object responsible for obtaining a new LKGM and committing it."""

  _COMMIT_MSG = ('Automated Commit: Committing new LKGM version %(version)s '
                 'for chromeos.')
  _CANDIDATES_TO_CONSIDER = 10

  _SLEEP_TIMEOUT = 30
  _TREE_TIMEOUT = 7200

  def __init__(self, checkout_dir, dryrun):
    self._checkout_dir = checkout_dir
    self._dryrun = dryrun
    self._lkgm = None
    self._old_lkgm = None
    self.site_config = config_lib.GetConfig()


  def CheckoutChromeLKGM(self):
    """Checkout chromeos LKGM file for chrome into tmp checkout dir."""
    if not os.path.exists(self._checkout_dir):
      cros_build_lib.RunCommand(
          ['git', 'clone', constants.CHROMIUM_GOB_URL,
           self._checkout_dir])
    else:
      cros_build_lib.RunCommand(
          ['git', 'fetch', 'origin'], cwd=self._checkout_dir)
      cros_build_lib.RunCommand(
          ['git', 'checkout', '-f', 'origin/master'], cwd=self._checkout_dir)

    cros_build_lib.RunCommand(
        ['git', 'branch', '-D', 'lkgm-roll'], cwd=self._checkout_dir,
        error_code_ok=True)
    cros_build_lib.RunCommand(
        ['git', 'checkout', '-b', 'lkgm-roll', 'origin/master'],
        cwd=self._checkout_dir)

    self._old_lkgm = osutils.ReadFile(
        os.path.join(self._checkout_dir, constants.PATH_TO_CHROME_LKGM))

  @cros_build_lib.MemoizedSingleCall
  def _GetLatestCanaryVersions(self):
    """Returns the latest CANDIDATES_TO_CONSIDER canary versions."""
    gs_handle = gs.GSContext()
    version_paths = gs_handle.LS(manifest_version.BUILD_STATUS_URL)

    # Strip gs://<path> prefix and trailing /'s.
    versions = [os.path.basename(v.rstrip('/')) for v in version_paths]

    lv = distutils.version.LooseVersion
    # We only care about canary versions which always end in 0.0.
    canary_versions = [v for v in versions if v.endswith('.0.0')]
    new_canary_versions = [v for v in canary_versions
                           if lv(v) > lv(self._old_lkgm)]
    return sorted(new_canary_versions, key=lv,
                  reverse=True)[0:self._CANDIDATES_TO_CONSIDER]

  def GetCanariesForChromeLKGM(self):
    """Grabs a list of builders that are important for the Chrome LKGM."""
    builders = []
    for build_name, conf in self.site_config.iteritems():
      if (conf['build_type'] == constants.CANARY_TYPE and
          conf['critical_for_chrome'] and not conf['child_configs']):
        builders.append(build_name)

    return builders

  def FindNewLKGM(self):
    """Finds a new LKGM for chrome from previous chromeos releases."""
    versions = self._GetLatestCanaryVersions()
    if not versions:
      raise LKGMNotFound('No valid LKGM found newer than the old LKGM.')

    canaries = self.GetCanariesForChromeLKGM()
    logging.info('Considering the following versions: %s', ' '.join(versions))
    logging.info('Using scores from the following canaries: %s',
                 ' '.join(canaries))

    # Scores are based on passing builders.
    version_scores = {}
    for version in versions:
      version_scores[version] = 0
      failed_builders = []
      for builder in canaries:
        status = manifest_version.BuildSpecsManager.GetBuildStatus(
            builder, version, retries=0)
        if status:
          if status.Passed():
            if version_scores[version] != -1:
              version_scores[version] += 1
          elif status.Failed():
            # We don't consider builds with any reporting failures.
            failed_builders.append(builder)
            version_scores[version] = -1

      if len(failed_builders) > 0:
        logging.warning('Version: %s Failed builders: %s',
                        version, ', '.join(failed_builders))
      logging.info('Version: %s Score: %d', version, version_scores[version])

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
    commit_msg = self._COMMIT_MSG % dict(version=self._lkgm)

    try:
      # Add the new versioned file.
      osutils.WriteFile(
          os.path.join(self._checkout_dir, constants.PATH_TO_CHROME_LKGM),
          self._lkgm)
      cros_build_lib.RunCommand(
          ['git', 'add', constants.PATH_TO_CHROME_LKGM], cwd=self._checkout_dir)

      # Commit it!
      cros_build_lib.RunCommand(
          ['git', 'commit', '-m', commit_msg],
          cwd=self._checkout_dir)
    except cros_build_lib.RunCommandError as e:
      raise LKGMNotCommitted(
          'Could not create git commit with new LKGM: %r' % e)

    if not tree_status.IsTreeOpen(status_url=gclient.STATUS_URL,
                                  period=self._SLEEP_TIMEOUT,
                                  timeout=self._TREE_TIMEOUT):
      raise LKGMNotCommitted('Chromium Tree is closed')

    if not self._dryrun:
      try:
        cros_build_lib.RunCommand(
            ['git', 'cl', 'land', '-f', '--bypass-hooks', '-m', commit_msg],
            cwd=self._checkout_dir)
      except cros_build_lib.RunCommandError as e:
        raise LKGMNotCommitted('Could not submit LKGM: %r' % e)

  def UpdateLatestFilesForBot(self, config, versions):
    """Update the LATEST files, for a given bot, in Google Storage.

    Args:
      config: The builder config to update.
      versions: Versions of ChromeOS to look at, sorted in descending order.
    """
    base_url = archive_lib.GetBaseUploadURI(config)
    acl = archive_lib.GetUploadACL(config)
    latest_url = None
    # gs.GSContext skips over all commands (including read-only checks)
    # when dry_run is True, so we have to create two context objects.
    # TODO(davidjames): Fix this.
    gs_ctx = gs.GSContext()
    copy_ctx = gs.GSContext(dry_run=self._dryrun)
    for version in reversed(versions):
      url = os.path.join(base_url, 'LATEST-%s' % version)
      found = gs_ctx.Exists(url, print_cmd=False)
      if not found and latest_url:
        try:
          copy_ctx.Copy(latest_url, url, version=0, acl=acl)
          logging.info('Copied %s -> %s', latest_url, url)
        except gs.GSContextPreconditionFailed:
          found = True

      if found:
        logging.info('Found %s', url)
        latest_url = url

  def UpdateLatestFiles(self):
    """Update the LATEST files since LKGM, in Google Storage."""
    ext_cfgs, int_cfgs = self.site_config.FindFullConfigsForBoard(board=None)
    versions = self._GetLatestCanaryVersions() + [self._old_lkgm]
    tasks = [[cfg, versions] for cfg in ext_cfgs + int_cfgs]
    parallel.RunTasksInProcessPool(self.UpdateLatestFilesForBot, tasks,
                                   processes=100)


def _GetParser():
  """Returns the parser to use for this module."""
  parser = commandline.ArgumentParser(usage=__doc__, caching=True)
  parser.add_argument('--dryrun', action='store_true', default=False,
                      help="Find the next LKGM but don't commit it.")
  parser.add_argument('--workdir', default=os.path.join(os.getcwd(), 'src'),
                      help=('Path to a checkout of chromium/src. '
                            'Defaults to PWD/src'))

  return parser


def main(argv):
  parser = _GetParser()
  args = parser.parse_args(argv)

  committer = ChromeCommitter(args.workdir, dryrun=args.dryrun)
  committer.CheckoutChromeLKGM()
  committer.UpdateLatestFiles()
  committer.FindNewLKGM()
  committer.CommitNewLKGM()
  return 0
