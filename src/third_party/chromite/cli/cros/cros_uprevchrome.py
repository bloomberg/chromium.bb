# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros uprevchrome: Uprev chrome to a new valid version."""

from __future__ import print_function

import os
import re

from chromite.lib import buildstore as buildstore_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.cli import command
from chromite.cli.cros import cros_pinchrome


# Interesting paths.
PUB_OVERLAY = os.path.join(
    constants.SOURCE_ROOT,
    constants.CHROMIUMOS_OVERLAY_DIR)
PUB_OVERLAY_URL = os.path.join(
    config_lib.GetSiteParams().EXTERNAL_GOB_URL,
    'chromiumos/overlays/chromiumos-overlay')
PRIV_OVERLAY = os.path.join(
    constants.SOURCE_ROOT,
    'src', 'private-overlays', 'chromeos-partner-overlay')
PRIV_OVERLAY_URL = os.path.join(
    config_lib.GetSiteParams().INTERNAL_GOB_URL,
    'chromeos/overlays/chromeos-partner-overlay')

# Master branch name
MASTER_BRANCH = 'master'

class MissingBranchException(Exception):
  """Remote branch wasn't found."""

class InvalidPFQBuildbucketIdException(Exception):
  """The PFQ build_id isn't valid."""

class InvalidReviewerEmailException(Exception):
  """The reviewer email address isn't valid."""

@command.CommandDecorator('uprevchrome')
class UprevChromeCommand(command.CliCommand):
  """cros uprevchrome: Uprev chrome to a new valid version

  When a chrome PFQ failure happens, master_chrome_pfq automatically checks the
  status of certain stages and then uploads the ebuild/binhost_mapping
  updates to a staging branch in remote git repository. This tool provides a way
  to fetch the updates from the remote staging branch, generate CLs to manually
  uprev chrome and upload the CLs to Gerrit for review.

  Users need to provide a failed master_chrome_pfq build_id, the bug id to track
  this manual uprev. This tool first verifies if the build_id is valid and if
  the staging branches exist, then fetches the changes into local branches,
  updates commit messages, adds CQ-DEPEND, rebases based on master and uploads
  CLs to Gerrit.

  Examples:

  cros uprevchrome --pfq-build XXXX --bug='chromium:XXXXX'
      [--dry-run] [--debug] [--nowipe] [--draft]

  After successfully executing this tool, review and submit CLs from Gerrit.
  https://chromium-review.googlesource.com
  https://chrome-internal-review.googlesource.com/

  Note:
  A master Chrome PFQ build id argument is required. It must satisfy the
  following requirements:
    * If you are a gardener trying to uprev Chrome please verify if the Chrome
      version you are trying to uprev to is stable and not crashy. Please
      communicate with the corresponding Chrome TPM to find out if that Chrome
      has any known issues or is particularly crashy. If yes, please work with
      them to find a stable Chrome that can be used to uprev.
    * The current  Chrome PFQ run is a failed run;
    * No successful Chrome PFQ runs after this Chrome PFQ run;
    * Master build of this PFQ run must have passed BinhostTest stage;
    * All important slave builds of this PFQ run must have passed
      UploadPrebuiltsTest stage.
    * A bug id is required to track each manual uprev, describing the known
      source of flake that is responsible for this run's failure.
  The public CL and private CL depend on each other,
  please submit them together.
  Please do not revert the generated CLs after they're merged.
  Please use cros_pinchrome to pin/unpin Chrome if needed.
  A detailed guide here:
  http://g3doc/company/teams/chrome/ops/chromeos/continuous_integration/on-call/guides/cros_uprevchrome_guide
  """

  NUM_RESULTS_LIMIT = 100

  def __init__(self, options):
    super(UprevChromeCommand, self).__init__(options)

  @classmethod
  def AddParser(cls, parser):
    super(cls, UprevChromeCommand).AddParser(parser)
    parser.add_argument('--pfq-build', action='store', required=True,
                        metavar='PFQ_BUILD',
                        help='The buildbucket_id of the master chrome pfq'
                        ' build. Note this is a 19 digit ID that can be found'
                        'in the Milo or GoldenEye URL for the build.')
    parser.add_argument('--fake-buildstore', action='store', default=False,
                        help='Use a FakeBuildStore instance')
    parser.add_argument('--bug', action='store', required=True,
                        help='Used in the "BUG" field of CLs.')
    parser.add_argument('--nowipe', default=True, dest='wipe',
                        action='store_false',
                        help='Preserve the working directory.')
    parser.add_argument('--draft', action='store_true',
                        help='Upload the uprev CLs to Gerrit as drafts.')
    parser.add_argument('--dry-run', action='store_true',
                        help="Prepare CLs but don't upload them to Gerrit")
    parser.add_argument('--reviewers',
                        action='append', default=[],
                        help='Add reviewers to the CLs.'
                        'ex:--reviewer x@email.com --reviewer y@email.com')
    return parser

  def ValidatePFQBuild(self, pfq_build, buildstore):
    """Validate the pfq build id.

    Args:
      pfq_build: master chrome pfq buildbucket_id to uprev.
      buildstore: BuildStore instance to make DB calls.

    Returns:
      BuildIdentifier instance of the PFQ Build.

    Raises:
      InvalidPFQBuildbucketIdException when pfq_build is invalid.
    """
    try:
      pfq_info = buildstore.GetBuildStatuses(buildbucket_ids=[pfq_build])[0]
    except IndexError:
      logging.error('Could not find the PFQ build. --pfq-build needs to be'
                    'a valid Buildbucket-ID')
      raise

    # Return False if it's not a master_chromium_pfq build.
    if pfq_info['build_config'] != constants.PFQ_MASTER:
      raise InvalidPFQBuildbucketIdException(
          'pfq_build %s with build_config %s is invalid. '
          'build_config must be master_chromium_pfq.' %
          (pfq_build, pfq_info['build_config']))

    # Can only uprev a failed pfq run
    if pfq_info['status'] != constants.BUILDER_STATUS_FAILED:
      logging.error('pfq_build %s not valid, status: %s',
                    pfq_build, pfq_info['status'])
      raise InvalidPFQBuildbucketIdException(
          'pfq_build %s with status: %s is invalid. '
          'Can only uprev a failed pfq build.' %
          (pfq_build, pfq_info['status']))

    # Get all the pfq builds which were started after
    # the given pfq build_id. If one build passed after this
    # pfq run, should not uprev or overwrite the uprevs.
    build_infos = buildstore.GetBuildHistory(constants.PFQ_MASTER,
                                             self.NUM_RESULTS_LIMIT,
                                             starting_build_id=pfq_build)

    for build_info in build_infos:
      if build_info['status'] == 'pass':
        raise InvalidPFQBuildbucketIdException(
            'pfq_build %s is invalid as build %s passed.' %
            (pfq_build, build_info['buildbucket_id']))

    return buildstore_lib.BuildIdentifier(cidb_id=pfq_info['id'],
                                          buildbucket_id=pfq_build)

  def CheckRemoteBranch(self, git_repo, remote, remote_ref):
    """Check if the remote ref exists."""
    output = git.RunGit(git_repo, ['ls-remote', remote, remote_ref]).output
    if not output.strip():
      raise MissingBranchException("repo %s remote %s ref %s doesn't exist"
                                   % (git_repo, remote, remote_ref))

  def ParseGitLog(self, overlay):
    """Parse the the most recent git log given the overlay.

    The PublishUprevChangesStage committed the uprev changes
    and pushed to a remote temporary branch. Need to get the
    last commit log and remove the unnecessary Change-Ids.

    Args:
      overlay: The overlay to parse the git log.

    Returns:
      Log body with all Change-Ids removed.
    """
    log = git.RunGit(overlay, ['log', '-n', '1', '--format=format:%B']).output

    # Remove Change-Ids.
    lines = log.splitlines()
    parsed_log = []
    for line in lines:
      if not re.search('Change-Id: (I[a-fA-F0-9]*)', line):
        parsed_log.append(line)
    return '\n'.join(parsed_log)

  def CommitMessage(self, body, pfq_build, cq_depend=None, change_id=None):
    """Generate a commit message.

    Args:
      body: The body of the message.
      pfq_build: buildbucket_id of the Chrome PFQ run.
      cq_depend: An optional CQ-DEPEND target.
      change_id: An optional change ID.

    Returns:
      The commit message.
    """
    message = [
        ('Manual Uprev Chrome: generated by cros_uprevchrome based on '
         'buildbucket_id %s' % pfq_build),
        '',
        body,
        '',
        'BUG=%s' % self.options.bug,
        'TEST=None',
    ]
    if cq_depend:
      message += ['Cq-Depend: %s' % cq_depend]
    if change_id:
      message += [
          '',
          'Change-Id: %s' % change_id,
      ]

    return '\n'.join(message)

  def ValidateReviewers(self, reviewers):
    """Raise an exception if one reviewer format isn't valid."""
    for r in reviewers:
      if "'" in r or '"' in r:
        raise InvalidReviewerEmailException(
            'Invalid reviewer email %s: %s' % (
                r, 'it cannot contain " or \''))

  def UprevChrome(self, work_dir, pfq_identifier):
    """Uprev Chrome.

    Args:
      work_dir: directory to clone repository and update uprev.
      pfq_identifier: pfq BuildIdentifier to uprev chrome.

    Raises:
      Exception when no commit ID is found in the public overlay CL.
      Exception when no commit ID is found in the private overlay CL.
    """
    pfq_build = pfq_identifier.buildbucket_id

    # Verify the format of reviewers
    reviewers = self.options.reviewers
    self.ValidateReviewers(reviewers)

    pub_overlay = os.path.join(work_dir, 'pub_overlay')
    priv_overlay = os.path.join(work_dir, 'priv_overlay')

    print('Setting up working directory...')
    # TODO(nxia): move cros_pinchrome.CloneWorkingRepo to a util class?
    cros_pinchrome.CloneWorkingRepo(pub_overlay, PUB_OVERLAY_URL, PUB_OVERLAY,
                                    MASTER_BRANCH)
    cros_pinchrome.CloneWorkingRepo(priv_overlay, PRIV_OVERLAY_URL,
                                    PRIV_OVERLAY,
                                    MASTER_BRANCH)

    print('Preparing CLs...')
    remote = 'origin'
    branch_name = constants.STAGING_PFQ_BRANCH_PREFIX + pfq_build
    remote_ref = ('refs/' + constants.PFQ_REF + '/' + branch_name)
    local_branch = '%s_%s' % (branch_name, cros_build_lib.GetRandomString())
    logging.info('Checking remote refs.')
    self.CheckRemoteBranch(pub_overlay, remote, remote_ref)
    self.CheckRemoteBranch(priv_overlay, remote, remote_ref)

    # Fetch the remote refspec for the public overlay
    logging.info('git fetch %s %s:%s', remote, remote_ref, local_branch)
    git.RunGit(pub_overlay, ['fetch', remote, '%s:%s' % (
        remote_ref, local_branch)])

    logging.info('git checkout %s', local_branch)
    git.RunGit(pub_overlay, ['checkout', local_branch])

    pub_commit_body = self.ParseGitLog(pub_overlay)
    commit_message = self.CommitMessage(pub_commit_body, pfq_build)

    # Update the commit message and reset author.
    pub_cid = git.Commit(pub_overlay, commit_message, amend=True,
                         reset_author=True)
    if not pub_cid:
      raise Exception("Don't know the commit ID of the public overlay CL.")

    # Fetch the remote refspec for the private overlay.
    logging.info('git fetch %s %s:%s', remote, remote_ref, local_branch)
    git.RunGit(priv_overlay, ['fetch', remote, '%s:%s' % (
        remote_ref, local_branch)])

    git.RunGit(priv_overlay, ['checkout', local_branch])
    logging.info('git checkout %s', local_branch)

    priv_commit_body = self.ParseGitLog(priv_overlay)
    # Add CQ-DEPEND (using 'CL:' prefix so cross-site links work).
    commit_message = self.CommitMessage(
        priv_commit_body, pfq_build, 'CL:' + pub_cid)

    # Update the commit message and reset author
    priv_cid = git.Commit(priv_overlay, commit_message, amend=True,
                          reset_author=True)
    if not priv_cid:
      raise Exception("Don't know the commit ID of the private overlay CL.")

    # Add CQ-DEPEND (using 'CL:' prefix so cross-site links work).
    commit_message = self.CommitMessage(
        pub_commit_body, pfq_build, 'CL:*' + priv_cid, pub_cid)

    git.Commit(pub_overlay, commit_message, amend=True)

    overlays = [(pub_overlay, PUB_OVERLAY_URL),
                (priv_overlay, PRIV_OVERLAY_URL)]

    for (_overlay, _overlay_url) in overlays:
      logging.info('git pull --rebase %s %s', remote, MASTER_BRANCH)
      git.RunGit(_overlay,
                 ['pull', '--rebase', remote, MASTER_BRANCH])

      logging.info('Upload CLs to Gerrit.')
      git.UploadCL(_overlay, _overlay_url, MASTER_BRANCH,
                   skip=self.options.dry_run, draft=self.options.draft,
                   debug_level=logging.NOTICE, reviewers=reviewers)

    print('Please review and submit the CLs together from Gerrit.')
    print('Successfully run cros uprevchrome!')

  def Run(self):
    """Run cros uprevchrome.

    Raises:
      Exception if the PFQ build_id is not valid.
      Exception if UprevChrome raises exceptions.
    """
    self.options.Freeze()

    if self.options.fake_buildstore:
      buildstore = buildstore_lib.FakeBuildStore()
    else:
      buildstore = buildstore_lib.BuildStore(_write_to_cidb=False)
    pfq_identifier = self.ValidatePFQBuild(self.options.pfq_build,
                                           buildstore)

    with osutils.TempDir(prefix='uprevchrome_',
                         delete=self.options.wipe) as work_dir:
      self.UprevChrome(work_dir, pfq_identifier)
      logging.info('Used working directory: %s', work_dir)
