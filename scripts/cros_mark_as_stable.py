#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs a given package's ebuild to the next revision."""

import optparse
import os
import sys

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel


# Dictionary of valid commands with usage information.
COMMAND_DICTIONARY = {
                        'commit':
                          'Marks given ebuilds as stable locally',
                        'push':
                          'Pushes previous marking of ebuilds to remote repo',
                      }


# ======================= Global Helper Functions ========================


def CleanStalePackages(boards, package_atoms):
  """Cleans up stale package info from a previous build.
  Args:
    boards: Boards to clean the packages from.
    package_atoms: A list of package atoms to unmerge.
  """
  if package_atoms:
    cros_build_lib.Info('Cleaning up stale packages %s.' % package_atoms)

  # First unmerge all the packages for a board, then eclean it.
  # We need these two steps to run in order (unmerge/eclean),
  # but we can let all the boards run in parallel.
  def _CleanStalePackages(board):
    if board:
      suffix = '-' + board
      runcmd = cros_build_lib.RunCommand
    else:
      suffix = ''
      runcmd = cros_build_lib.SudoRunCommand

    emerge, eclean = 'emerge' + suffix, 'eclean' + suffix
    if not osutils.FindMissingBinaries([emerge, eclean]):
      # If nothing was found to be unmerged, emerge will exit(1).
      result = runcmd([emerge, '-q', '--unmerge'] + package_atoms,
                      extra_env={'CLEAN_DELAY': '0'}, error_code_ok=True)
      if not result.returncode in (0, 1):
        raise cros_build_lib.RunCommandError('unexpected error', result)
      runcmd([eclean, '-d', 'packages'],
             redirect_stdout=True, redirect_stderr=True)

  tasks = []
  for board in boards:
    tasks.append([board])
  tasks.append([None])

  parallel.RunTasksInProcessPool(_CleanStalePackages, tasks)


# TODO(build): This code needs to be gutted and rebased to cros_build_lib.
def _DoWeHaveLocalCommits(stable_branch, tracking_branch, cwd):
  """Returns true if there are local commits."""
  current_branch = git.GetCurrentBranch(cwd)

  if current_branch != stable_branch:
    return False
  output = git.RunGit(
      cwd, ['rev-parse', 'HEAD', tracking_branch]).output.split()
  return output[0] != output[1]


def _CheckSaneArguments(command, options):
  """Checks to make sure the flags are sane.  Dies if arguments are not sane."""
  if not command in COMMAND_DICTIONARY.keys():
    _PrintUsageAndDie('%s is not a valid command' % command)
  if not options.packages and command == 'commit' and not options.all:
    _PrintUsageAndDie('Please specify at least one package')
  if options.boards:
    cros_build_lib.AssertInsideChroot()
  if not os.path.isdir(options.srcroot):
    _PrintUsageAndDie('srcroot is not a valid path')
  options.srcroot = os.path.abspath(options.srcroot)


def _PrintUsageAndDie(error_message=''):
  """Prints optional error_message the usage and returns an error exit code."""
  command_usage = 'Commands: \n'
  # Add keys and usage information from dictionary.
  commands = sorted(COMMAND_DICTIONARY.keys())
  for command in commands:
    command_usage += '  %s: %s\n' % (command, COMMAND_DICTIONARY[command])
  commands_str = '|'.join(commands)
  cros_build_lib.Warning('Usage: %s FLAGS [%s]\n\n%s' % (
      sys.argv[0], commands_str, command_usage))
  if error_message:
    cros_build_lib.Die(error_message)
  else:
    sys.exit(1)


# ======================= End Global Helper Functions ========================


def PushChange(stable_branch, tracking_branch, dryrun, cwd):
  """Pushes commits in the stable_branch to the remote git repository.

  Pushes local commits from calls to CommitChange to the remote git
  repository specified by current working directory. If changes are
  found to commit, they will be merged to the merge branch and pushed.
  In that case, the local repository will be left on the merge branch.

  Args:
    stable_branch: The local branch with commits we want to push.
    tracking_branch: The tracking branch of the local branch.
    dryrun: Use git push --dryrun to emulate a push.
    cwd: The directory to run commands in.
  Raises:
      OSError: Error occurred while pushing.
  """
  if not _DoWeHaveLocalCommits(stable_branch, tracking_branch, cwd):
    cros_build_lib.Info('No work found to push in %s.  Exiting', cwd)
    return

  # For the commit queue, our local branch may contain commits that were
  # just tested and pushed during the CommitQueueCompletion stage. Sync
  # and rebase our local branch on top of the remote commits.
  remote, push_branch = git.GetTrackingBranch(cwd, for_push=True)
  git.SyncPushBranch(cwd, remote, push_branch)

  # Check whether any local changes remain after the sync.
  if not _DoWeHaveLocalCommits(stable_branch, push_branch, cwd):
    cros_build_lib.Info('All changes already pushed for %s. Exiting', cwd)
    return

  description = cros_build_lib.RunCommandCaptureOutput(
      ['git', 'log', '--format=format:%s%n%n%b', '%s..%s' % (
       push_branch, stable_branch)], cwd=cwd).output
  description = 'Marking set of ebuilds as stable\n\n%s' % description
  cros_build_lib.Info('For %s, using description %s', cwd, description)
  git.CreatePushBranch(constants.MERGE_BRANCH, cwd)
  git.RunGit(cwd, ['merge', '--squash', stable_branch])
  git.RunGit(cwd, ['commit', '-m', description])
  git.RunGit(cwd, ['config', 'push.default', 'tracking'])
  git.PushWithRetry(constants.MERGE_BRANCH, cwd, dryrun=dryrun)


class GitBranch(object):
  """Wrapper class for a git branch."""

  def __init__(self, branch_name, tracking_branch, cwd):
    """Sets up variables but does not create the branch."""
    self.branch_name = branch_name
    self.tracking_branch = tracking_branch
    self.cwd = cwd

  def CreateBranch(self):
    self.Checkout()

  def Checkout(self, branch=None):
    """Function used to check out to another GitBranch."""
    if not branch:
      branch = self.branch_name
    if branch == self.tracking_branch or self.Exists(branch):
      git_cmd = ['git', 'checkout', '-f', branch]
    else:
      git_cmd = ['repo', 'start', branch, '.']
    cros_build_lib.RunCommandCaptureOutput(git_cmd, print_cmd=False,
                                           cwd=self.cwd)

  def Exists(self, branch=None):
    """Returns True if the branch exists."""
    if not branch:
      branch = self.branch_name
    branches = cros_build_lib.RunCommandCaptureOutput(['git', 'branch'],
                                                      print_cmd=False,
                                                      cwd=self.cwd).output
    return branch in branches.split()


def main(_argv):
  parser = optparse.OptionParser('cros_mark_as_stable OPTIONS packages')
  parser.add_option('--all', action='store_true',
                    help='Mark all packages as stable.')
  parser.add_option('-b', '--boards', default='',
                    help='Colon-separated list of boards')
  parser.add_option('--drop_file',
                    help='File to list packages that were revved.')
  parser.add_option('--dryrun', action='store_true',
                    help='Passes dry-run to git push if pushing a change.')
  parser.add_option('-o', '--overlays',
                    help='Colon-separated list of overlays to modify.')
  parser.add_option('-p', '--packages',
                    help='Colon separated list of packages to rev.')
  parser.add_option('-r', '--srcroot',
                    default=os.path.join(constants.SOURCE_ROOT, 'src'),
                    help='Path to root src directory.')
  parser.add_option('--verbose', action='store_true',
                    help='Prints out debug info.')
  (options, args) = parser.parse_args()

  portage_utilities.EBuild.VERBOSE = options.verbose

  if len(args) != 1:
    _PrintUsageAndDie('Must specify a valid command [commit, push]')

  command = args[0]
  package_list = None
  if options.packages:
    package_list = options.packages.split(':')

  _CheckSaneArguments(command, options)
  if options.overlays:
    overlays = {}
    for path in options.overlays.split(':'):
      if not os.path.isdir(path):
        cros_build_lib.Die('Cannot find overlay: %s' % path)
      overlays[path] = []
  else:
    cros_build_lib.Warning('Missing --overlays argument')
    overlays = {
      '%s/private-overlays/chromeos-overlay' % options.srcroot: [],
      '%s/third_party/chromiumos-overlay' % options.srcroot: []
    }

  if command == 'commit':
    portage_utilities.BuildEBuildDictionary(
      overlays, options.all, package_list)

  manifest = git.ManifestCheckout.Cached(options.srcroot)

  # Contains the array of packages we actually revved.
  revved_packages = []
  new_package_atoms = []

  # Slight optimization hack: process the chromiumos overlay before any other
  # cros-workon overlay first so we can do background cache generation in it.
  # A perfect solution would walk all the overlays, figure out any dependencies
  # between them (with layout.conf), and then process them in dependency order.
  # However, this operation isn't slow enough to warrant that level of
  # complexity, so we'll just special case the main overlay.
  #
  # Similarly, generate the cache in the portage-stable tree asap.  We know
  # we won't have any cros-workon packages in there, so generating the cache
  # is the only thing it'll be doing.  The chromiumos overlay instead might
  # have revbumping to do before it can generate the cache.
  keys = overlays.keys()
  for overlay in ('/third_party/chromiumos-overlay',
                  '/third_party/portage-stable'):
    for k in keys:
      if k.endswith(overlay):
        keys.remove(k)
        keys.insert(0, k)
        break

  with parallel.BackgroundTaskRunner(portage_utilities.RegenCache) as queue:
    for overlay in keys:
      ebuilds = overlays[overlay]
      if not os.path.isdir(overlay):
        cros_build_lib.Warning("Skipping %s" % overlay)
        continue

      # Note we intentionally work from the non push tracking branch;
      # everything built thus far has been against it (meaning, http mirrors),
      # thus we should honor that.  During the actual push, the code switches
      # to the correct urls, and does an appropriate rebasing.
      tracking_branch = git.GetTrackingBranchViaManifest(
          overlay, manifest=manifest)[1]

      if command == 'push':
        PushChange(constants.STABLE_EBUILD_BRANCH, tracking_branch,
                   options.dryrun, cwd=overlay)
      elif command == 'commit':
        existing_branch = git.GetCurrentBranch(overlay)
        work_branch = GitBranch(constants.STABLE_EBUILD_BRANCH, tracking_branch,
                                cwd=overlay)
        work_branch.CreateBranch()
        if not work_branch.Exists():
          cros_build_lib.Die('Unable to create stabilizing branch in %s' %
                             overlay)

        # In the case of uprevving overlays that have patches applied to them,
        # include the patched changes in the stabilizing branch.
        if existing_branch:
          cros_build_lib.RunCommand(['git', 'rebase', existing_branch],
                                    print_cmd=False, cwd=overlay)

        for ebuild in ebuilds:
          if options.verbose:
            cros_build_lib.Info('Working on %s', ebuild.package)
          try:
            new_package = ebuild.RevWorkOnEBuild(options.srcroot)
            if new_package:
              revved_packages.append(ebuild.package)
              new_package_atoms.append('=%s' % new_package)
          except (OSError, IOError):
            cros_build_lib.Warning('Cannot rev %s\n' % ebuild.package +
                    'Note you will have to go into %s '
                    'and reset the git repo yourself.' % overlay)
            raise

        if cros_build_lib.IsInsideChroot():
          # Regenerate caches if need be.  We do this all the time to
          # catch when users make changes without updating cache files.
          queue.put([overlay])

  if command == 'commit':
    if cros_build_lib.IsInsideChroot():
      CleanStalePackages(options.boards.split(':'), new_package_atoms)
    if options.drop_file:
      osutils.WriteFile(options.drop_file, ' '.join(revved_packages))
