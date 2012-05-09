#!/usr/bin/python

# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

import distutils.version
import glob
import logging
import multiprocessing
import optparse
import os
import pprint
import sys
import time

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import gerrit_helper
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import remote_try
from chromite.buildbot import repository
from chromite.buildbot import tee

from chromite.lib import cgroups
from chromite.lib import cleanup
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import sudo


cros_lib.STRICT_SUDO = True

_DEFAULT_LOG_DIR = 'cbuildbot_logs'
_BUILDBOT_LOG_FILE = 'cbuildbot.log'
_DEFAULT_EXT_BUILDROOT = 'trybot'
_DEFAULT_INT_BUILDROOT = 'trybot-internal'
_PATH_TO_CBUILDBOT = os.path.join(constants.CHROMITE_BIN_SUBDIR, 'cbuildbot')
_DISTRIBUTED_TYPES = [constants.COMMIT_QUEUE_TYPE, constants.PFQ_TYPE,
                      constants.CANARY_TYPE, constants.CHROME_PFQ_TYPE,
                      constants.PALADIN_TYPE]
_BUILDBOT_REQUIRED_BINARIES = ('pbzip2',)


def _PrintValidConfigs(display_all=False):
  """Print a list of valid buildbot configs.

  Arguments:
    display_all: Print all configs.  Otherwise, prints only configs with
                 trybot_list=True.
  """
  def _GetSortKey(config_name):
    config_dict = cbuildbot_config.config[config_name]
    return (not config_dict['trybot_list'], config_dict['description'],
            config_name)

  COLUMN_WIDTH = 45
  print 'config'.ljust(COLUMN_WIDTH), 'description'
  print '------'.ljust(COLUMN_WIDTH), '-----------'
  config_names = cbuildbot_config.config.keys()
  config_names.sort(key=_GetSortKey)
  for name in config_names:
    if display_all or cbuildbot_config.config[name]['trybot_list']:
      desc = cbuildbot_config.config[name].get('description')
      desc = desc if desc else ''
      print name.ljust(COLUMN_WIDTH), desc


def _GetConfig(config_name):
  """Gets the configuration for the build"""
  if not cbuildbot_config.config.has_key(config_name):
    print 'Non-existent configuration %s specified.' % config_name
    print 'Please specify one of:'
    _PrintValidConfigs()
    sys.exit(1)

  result = cbuildbot_config.config[config_name]

  return result


def _GetChromiteTrackingBranch():
  """Returns the remote branch associated with chromite."""
  cwd = os.path.dirname(os.path.realpath(__file__))
  branch = cros_lib.GetCurrentBranch(cwd)
  if branch:
    tracking_branch = cros_lib.GetTrackingBranch(branch, cwd)[1]
    if tracking_branch.startswith('refs/heads/'):
      return tracking_branch.replace('refs/heads/', '')
  # If we are not on a branch, or if the tracking branch is a revision,
  # use the push branch. For repo repositories, this will be the manifest
  # branch configured for this project. For other repositories, we'll just
  # guess 'master', since there's no easy way to find out what branch
  # we're on.
  return cros_lib.GetPushBranch(cwd)[1]


def _CheckBuildRootBranch(buildroot, tracking_branch):
  """Make sure buildroot branch is the same as Chromite branch."""
  manifest_branch = cros_lib.GetManifestDefaultBranch(buildroot)
  if manifest_branch != tracking_branch:
    cros_lib.Die('Chromite is not on same branch as buildroot checkout\n' +
                 'Chromite is on branch %s.\n' % tracking_branch +
                 'Buildroot checked out to %s\n' % manifest_branch)


def _PreProcessPatches(gerrit_patches, local_patches):
  """Validate patches ASAP to catch user errors.  Also generate patch info.

  Args:
    gerrit_patches: List of gerrit CL ID's passed in by user.
    local_patches: List of local project branches to generate patches from.

  Returns:
    A tuple containing a list of cros_patch.GerritPatch and a list of
    cros_patch.LocalGitRepoPatch objects.
  """
  gerrit_patch_info = []
  local_patch_info = []

  try:
    if gerrit_patches:
      gerrit_patch_info = gerrit_helper.GetGerritPatchInfo(gerrit_patches)
      for patch in gerrit_patch_info:
        if patch.IsAlreadyMerged():
          cros_lib.Warning('Patch %s has already been merged.' % str(patch))
  except gerrit_helper.GerritException as e:
    cros_lib.Die(str(e))

  try:
    if local_patches:
      local_patch_info = cros_patch.PrepareLocalPatches(
          local_patches,
          _GetChromiteTrackingBranch())

  except cros_patch.PatchException as e:
    cros_lib.Die(str(e))

  return gerrit_patch_info, local_patch_info


def _IsIncrementalBuild(buildroot, clobber):
  """Returns True if we are reusing an existing buildroot."""
  repo_dir = os.path.join(buildroot, '.repo')
  return not clobber and os.path.isdir(repo_dir)


class Builder(object):
  """Parent class for all builder types.

  This class functions as a parent class for various build types.  It's intended
  use is builder_instance.Run().

  Vars:
    build_config:  The configuration dictionary from cbuildbot_config.
    options:  The options provided from optparse in main().
    completed_stages_file: Where we store resume state.
    archive_url:  Where our artifacts for this builder will be archived.
    tracking_branch: The tracking branch for this build.
    release_tag:  The associated "chrome os version" of this build.
    gerrit_patches: Gerrit patches to be included in build.
    local_patches: Local patches to be included in build.
  """

  def __init__(self, options, build_config):
    """Initializes instance variables. Must be called by all subclasses."""
    self.build_config = build_config
    self.options = options

    # TODO, Remove here and in config after bug chromium-os:14649 is fixed.
    if self.build_config['chromeos_official']:
      os.environ['CHROMEOS_OFFICIAL'] = '1'

    self.completed_stages_file = os.path.join(options.buildroot,
                                              '.completed_stages')
    self.archive_stages = {}
    self.archive_urls = {}
    self.release_tag = None
    self.tracking_branch = _GetChromiteTrackingBranch()
    self.gerrit_patches = None
    self.local_patches = None

  def Initialize(self):
    """Runs through the initialization steps of an actual build."""
    if self.options.resume and os.path.exists(self.completed_stages_file):
      with open(self.completed_stages_file, 'r') as load_file:
        results_lib.Results.RestoreCompletedStages(load_file)

    # We only want to do this if we need to patch changes.
    if not results_lib.Results.GetPrevious().get(
        self._GetStageInstance(stages.PatchChangesStage, None, None).name):
      self.gerrit_patches, self.local_patches = _PreProcessPatches(
          self.options.gerrit_patches, self.options.local_patches)

    bs.BuilderStage.SetTrackingBranch(self.tracking_branch)

    # Check branch matching early.
    if _IsIncrementalBuild(self.options.buildroot, self.options.clobber):
      _CheckBuildRootBranch(self.options.buildroot, self.tracking_branch)

    self._RunStage(stages.CleanUpStage)

  def _GetStageInstance(self, stage, *args, **kwargs):
    """Helper function to get an instance given the args.

    Useful as almost all stages just take in options and build_config.
    """
    config = kwargs.pop('config', self.build_config)
    return stage(self.options, config, *args, **kwargs)

  def _SetReleaseTag(self):
    """Sets the release tag from the manifest_manager.

    Must be run after sync stage as syncing enables us to have a release tag.
    """
    # Extract version we have decided to build into self.release_tag.
    manifest_manager = stages.ManifestVersionedSyncStage.manifest_manager
    if manifest_manager:
      self.release_tag = manifest_manager.current_version

  def _RunStage(self, stage, *args, **kwargs):
    """Wrapper to run a stage."""
    stage_instance = self._GetStageInstance(stage, *args, **kwargs)
    return stage_instance.Run()

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run.

    Subclasses must override this method.
    """
    raise NotImplementedError()

  def RunStages(self):
    """Subclasses must override this method.  Runs the appropriate code."""
    raise NotImplementedError()

  def _WriteCheckpoint(self):
    """Drops a completed stages file with current state."""
    with open(self.completed_stages_file, 'w+') as save_file:
      results_lib.Results.SaveCompletedStages(save_file)

  def _ShouldReExecuteInBuildRoot(self):
    """Returns True if this build should be re-executed in the buildroot."""
    abs_buildroot = os.path.abspath(self.options.buildroot)
    return not os.path.abspath(__file__).startswith(abs_buildroot)

  def _ReExecuteInBuildroot(self, sync_instance):
    """Reexecutes self in buildroot and returns True if build succeeds.

    This allows the buildbot code to test itself when changes are patched for
    buildbot-related code.  This is a no-op if the buildroot == buildroot
    of the running chromite checkout.

    Args:
      sync_instance:  Instance of the sync stage that was run to sync.

    Returns:
      True if the Build succeeded.
    """
    # If we are resuming, use last checkpoint.
    if not self.options.resume:
      self._WriteCheckpoint()

    # Re-write paths to use absolute paths.
    # Suppress any timeout options given from the commandline in the
    # invoked cbuildbot; our timeout will enforce it instead.
    args_to_append = ['--resume', '--timeout', '0', '--buildroot',
                      os.path.abspath(self.options.buildroot)]

    if self.options.chrome_root:
      args_to_append += ['--chrome_root',
                         os.path.abspath(self.options.chrome_root)]

    if stages.ManifestVersionedSyncStage.manifest_manager:
      ver = stages.ManifestVersionedSyncStage.manifest_manager.current_version
      args_to_append += ['--version', ver]

    if isinstance(sync_instance, stages.CommitQueueSyncStage):
      vp_file = sync_instance.SaveValidationPool()
      args_to_append += ['--validation_pool', vp_file]

    # Re-run the command in the buildroot.
    # Finally, be generous and give the invoked cbuildbot 30s to shutdown
    # when something occurs.  It should exit quicker, but the sigterm may
    # hit while the system is particularly busy.
    return_obj = cros_lib.RunCommand(
        [_PATH_TO_CBUILDBOT] + sys.argv[1:] + args_to_append,
        cwd=self.options.buildroot, error_code_ok=True, kill_timeout=30)
    return return_obj.returncode == 0

  def Run(self):
    """Main runner for this builder class.  Runs build and prints summary."""
    print_report = True
    exception_thrown = False
    success = True
    try:
      self.Initialize()
      sync_instance = self.GetSyncInstance()
      sync_instance.Run()
      self._SetReleaseTag()

      if (self.gerrit_patches or self.local_patches
          or self.options.remote_patches):
        self._RunStage(stages.PatchChangesStage,
                       self.gerrit_patches, self.local_patches)

      if self._ShouldReExecuteInBuildRoot():
        print_report = False
        success = self._ReExecuteInBuildroot(sync_instance)
      else:
        self.RunStages()
    except Exception:
      exception_thrown = True
      raise
    finally:
      if print_report:
        self._WriteCheckpoint()
        print '\n\n\n@@@BUILD_STEP Report@@@\n'
        results_lib.Results.Report(sys.stdout, self.archive_urls,
                                   self.release_tag)
        success = results_lib.Results.BuildSucceededSoFar()
        if exception_thrown and success:
          success = False
          print >> sys.stderr, """
@@@STEP_FAILURE@@@
Exception thrown, but all stages marked successful. This is an internal error,
because the stage that threw the exception should be marked as failing."""

    return success


class SimpleBuilder(Builder):
  """Builder that performs basic vetting operations."""

  def GetSyncInstance(self):
    """Sync to lkgm or TOT as necessary.

    Returns: the instance of the sync stage that was run.
    """
    if self.options.lkgm or self.build_config['use_lkgm']:
      sync_stage = self._GetStageInstance(stages.LKGMSyncStage)
    else:
      sync_stage = self._GetStageInstance(stages.SyncStage)

    return sync_stage

  def _RunBackgroundStagesForBoard(self, board):
    """Run background board-specific stages for the specified board."""
    archive_stage = self.archive_stages[board]
    configs = self.build_config['board_specific_configs']
    config = configs.get(board, self.build_config)
    stage_list = [[stages.VMTestStage, board, archive_stage],
                  [stages.ChromeTestStage, board, archive_stage],
                  [stages.UnitTestStage, board],
                  [stages.UploadPrebuiltsStage, board, archive_stage]]

    # We can not run hw tests without archiving the payloads.
    if self.options.archive:
      for suite in config['hw_tests']:
        stage_list.append([stages.HWTestStage, board, archive_stage, suite])

    steps = [self._GetStageInstance(*x, config=config).Run for x in stage_list]
    background.RunParallelSteps(steps + [archive_stage.Run])

  def RunStages(self):
    """Runs through build process."""
    self._RunStage(stages.BuildBoardStage)

    # TODO(sosa): Split these out into classes.
    if self.build_config['build_type'] == constants.CHROOT_BUILDER_TYPE:
      self._RunStage(stages.SDKTestStage)
      self._RunStage(stages.UploadPrebuiltsStage,
                     constants.CHROOT_BUILDER_BOARD, None)
    elif self.build_config['build_type'] == constants.REFRESH_PACKAGES_TYPE:
      self._RunStage(stages.RefreshPackageStatusStage)
    else:
      self._RunStage(stages.UprevStage)

      configs = self.build_config['board_specific_configs']
      for board in self.build_config['boards']:
        config = configs.get(board, self.build_config)
        archive_stage = self._GetStageInstance(stages.ArchiveStage, board,
                                               config=config)
        self.archive_stages[board] = archive_stage

      # Set up a process pool to run test/archive stages in the background.
      # This process runs task(board) for each board added to the queue.
      queue = multiprocessing.Queue()
      task = self._RunBackgroundStagesForBoard
      with background.BackgroundTaskRunner(queue, task):
        for board in self.build_config['boards']:
          # Run BuildTarget in the foreground.
          archive_stage = self.archive_stages[board]
          config = configs.get(board, self.build_config)
          self._RunStage(stages.BuildTargetStage, board, archive_stage,
                         self.release_tag, config=config)
          self.archive_urls[board] = archive_stage.GetDownloadUrl()

          # Kick off task(board) in the background.
          queue.put([board])


class DistributedBuilder(SimpleBuilder):
  """Build class that has special logic to handle distributed builds.

  These builds sync using git/manifest logic in manifest_versions.  In general
  they use a non-distributed builder code for the bulk of the work.
  """
  def __init__(self, options, build_config):
    """Initializes a buildbot builder.

    Extra variables:
      completion_stage_class:  Stage used to complete a build.  Set in the Sync
        stage.
    """
    super(DistributedBuilder, self).__init__(options, build_config)
    self.completion_stage_class = None

  def GetSyncInstance(self):
    """Syncs the tree using one of the distributed sync logic paths.

    Returns: the instance of the sync stage that was run.
    """
    # Determine sync class to use.  CQ overrides PFQ bits so should check it
    # first.
    if cbuildbot_config.IsCQType(self.build_config['build_type']):
      sync_stage = self._GetStageInstance(stages.CommitQueueSyncStage)
      self.completion_stage_class = stages.CommitQueueCompletionStage
    elif cbuildbot_config.IsPFQType(self.build_config['build_type']):
      sync_stage = self._GetStageInstance(stages.LKGMCandidateSyncStage)
      self.completion_stage_class = stages.LKGMCandidateSyncCompletionStage
    else:
      sync_stage = self._GetStageInstance(stages.ManifestVersionedSyncStage)
      self.completion_stage_class = stages.ManifestVersionedSyncCompletionStage

    return sync_stage

  def Publish(self, was_build_successful):
    """Completes build by publishing any required information."""
    completion_stage = self._GetStageInstance(self.completion_stage_class,
                                              was_build_successful)
    completion_stage.Run()
    name = completion_stage.name
    if not results_lib.Results.WasStageSuccessful(name):
      should_publish_changes = False
    else:
      should_publish_changes = (self.build_config['master'] and
                                was_build_successful)

    if should_publish_changes:
      self._RunStage(stages.PublishUprevChangesStage)

  def RunStages(self):
    """Runs simple builder logic and publishes information to overlays."""
    was_build_successful = False
    try:
      super(DistributedBuilder, self).RunStages()
      was_build_successful = results_lib.Results.BuildSucceededSoFar()
    except SystemExit as ex:
      # If a stage calls sys.exit(0), it's exiting with success, so that means
      # we should mark ourselves as successful.
      if ex.code == 0:
        was_build_successful = True
      raise
    finally:
      self.Publish(was_build_successful)


def _ConfirmBuildRoot(buildroot):
  """Confirm with user the inferred buildroot, and mark it as confirmed."""
  warning = 'Using default directory %s as buildroot' % buildroot
  response = cros_lib.YesNoPrompt(default=cros_lib.NO, warning=warning,
                                  full=True)
  if response == cros_lib.NO:
    print('Please specify a buildroot with the --buildroot option.')
    sys.exit(0)

  if not os.path.exists(buildroot):
    os.mkdir(buildroot)

  repository.CreateTrybotMarker(buildroot)


def _ConfirmRemoteBuildbotRun():
  """Confirm user wants to run with --buildbot --remote."""
  warning = ('You are about to launch a PRODUCTION job!  This is *NOT* a '
             'trybot run! Are you sure?')
  response = cros_lib.YesNoPrompt(default=cros_lib.NO, warning=warning,
                                  full=True)

  if response == cros_lib.NO:
    print('Please specify --pass-through="--debug".')
    sys.exit(0)


def _DetermineDefaultBuildRoot(internal_build):
  """Default buildroot to be under the directory that contains current checkout.

  Arguments:
    internal_build: Whether the build is an internal build
  """
  repo_dir = cros_lib.FindRepoDir()
  if not repo_dir:
    cros_lib.Die('Could not find root of local checkout.  Please specify'
                 'using --buildroot option.')

  # Place trybot buildroot under the directory containing current checkout.
  top_level = os.path.dirname(os.path.realpath(os.path.dirname(repo_dir)))
  if internal_build:
    buildroot = os.path.join(top_level, _DEFAULT_INT_BUILDROOT)
  else:
    buildroot = os.path.join(top_level, _DEFAULT_EXT_BUILDROOT)

  return buildroot


def _BackupPreviousLog(log_file, backup_limit=25):
  """Rename previous log.

  Args:
    log_file: The absolute path to the previous log.
  """
  if os.path.exists(log_file):
    old_logs = sorted(glob.glob(log_file + '.*'),
                      key=distutils.version.LooseVersion)

    if len(old_logs) >= backup_limit:
      os.remove(old_logs[0])

    last = 0
    if old_logs:
      last = int(old_logs.pop().rpartition('.')[2])

    os.rename(log_file, log_file + '.' + str(last + 1))


def _RunBuildStagesWrapper(options, build_config):
  """Helper function that wraps RunBuildStages()."""
  def IsDistributedBuilder():
    """Determines whether the build_config should be a DistributedBuilder."""
    if not options.buildbot:
      return False
    elif build_config['build_type'] in _DISTRIBUTED_TYPES:
      chrome_rev = build_config['chrome_rev']
      if options.chrome_rev: chrome_rev = options.chrome_rev
      # We don't do distributed logic to TOT Chrome PFQ's, nor local
      # chrome roots (e.g. chrome try bots)
      if chrome_rev not in [constants.CHROME_REV_TOT,
                            constants.CHROME_REV_LOCAL,
                            constants.CHROME_REV_SPEC]:
        return True

    return False

  # Start tee-ing output to file.
  log_file = None
  if options.tee:
    default_dir = os.path.join(options.buildroot, _DEFAULT_LOG_DIR)
    dirname = options.log_dir or default_dir
    log_file = os.path.join(dirname, _BUILDBOT_LOG_FILE)

    cros_lib.SafeMakedirs(dirname)
    _BackupPreviousLog(log_file)

  try:
    with cros_lib.AllowDisabling(options.tee, tee.Tee, log_file):
      cros_lib.Info("cbuildbot executed with args %s"
                    % ' '.join(map(repr, sys.argv)))
      if IsDistributedBuilder():
        buildbot = DistributedBuilder(options, build_config)
      else:
        buildbot = SimpleBuilder(options, build_config)

      if not buildbot.Run():
        sys.exit(1)
  finally:
    if options.tee:
      cros_lib.Info('Output should be saved to %s' % log_file)


# Parser related functions
def _CheckLocalPatches(local_patches):
  """Do an early quick check of the passed-in patches.

  If the branch of a project is not specified we append the current branch the
  project is on.
  """
  verified_patches = []
  for patch in local_patches:
    components = patch.split(':')
    if len(components) > 2:
      msg = 'Specify local patches in project[:branch] format.'
      raise optparse.OptionValueError(msg)

    # validate project
    project = components[0]
    if not cros_lib.DoesProjectExist('.', project):
      raise optparse.OptionValueError('Project %s does not exist.' % project)

    project_dir = cros_lib.GetProjectDir('.', project)

    # If no branch was specified, we use the project's current branch.
    if len(components) == 1:
      branch = cros_lib.GetCurrentBranch(project_dir)
      if not branch:
        raise optparse.OptionValueError('project %s is not on a branch!'
                                        % project)
      # Append branch information to patch
      patch = '%s:%s' % (project, branch)
    else:
      branch = components[1]
      if not cros_lib.DoesLocalBranchExist(project_dir, branch):
        raise optparse.OptionValueError('Project %s does not have branch %s'
                                        % (project, branch))

    verified_patches.append(patch)

  return verified_patches


def _CheckBuildRootOption(_option, _opt_str, value, parser):
  """Validate and convert buildroot to full-path form."""
  value = value.strip()
  if not value or value == '/':
    raise optparse.OptionValueError('Invalid buildroot specified')

  parser.values.buildroot = os.path.realpath(os.path.expanduser(value))


def _CheckLogDirOption(_option, _opt_str, value, parser):
  """Validate and convert buildroot to full-path form."""
  parser.values.log_dir = os.path.abspath(os.path.expanduser(value))


def _CheckChromeVersionOption(_option, _opt_str, value, parser):
  """Upgrade other options based on chrome_version being passed."""
  value = value.strip()

  if parser.values.chrome_rev is None and value:
    parser.values.chrome_rev = constants.CHROME_REV_SPEC

  parser.values.chrome_version = value


def _CheckChromeRootOption(_option, _opt_str, value, parser):
  """Validate and convert chrome_root to full-path form."""
  value = value.strip()
  if not value or value == '/':
    raise optparse.OptionValueError('Invalid chrome_root specified')

  if parser.values.chrome_rev is None:
    parser.values.chrome_rev = constants.CHROME_REV_LOCAL

  parser.values.chrome_root = os.path.realpath(os.path.expanduser(value))


def _CheckChromeRevOption(_option, _opt_str, value, parser):
  """Validate the chrome_rev option."""
  value = value.strip()
  if value not in constants.VALID_CHROME_REVISIONS:
    raise optparse.OptionValueError('Invalid chrome rev specified')

  parser.values.chrome_rev = value


def _CreateParser():
  class CustomParser(optparse.OptionParser):
    def add_remote_option(self, *args, **kwargs):
      """For arguments that are passed-through to remote trybot."""
      return optparse.OptionParser.add_option(self, *args,
                                              remote_pass_through=True,
                                              **kwargs)

  class CustomGroup(optparse.OptionGroup):
    def add_remote_option(self, *args, **kwargs):
      """For arguments that are passed-through to remote trybot."""
      return optparse.OptionGroup.add_option(self, *args,
                                             remote_pass_through=True,
                                             **kwargs)

  class CustomOption(optparse.Option):
    """Subclass Option class to implement pass-through."""
    def __init__(self, *args, **kwargs):
      # The remote_pass_through argument specifies whether we should directly
      # pass the argument (with its value) onto the remote trybot.
      self.pass_through = kwargs.pop('remote_pass_through', False)
      optparse.Option.__init__(self, *args, **kwargs)

    def take_action(self, action, dest, opt, value, values, parser):
      optparse.Option.take_action(self, action, dest, opt, value, values,
                                  parser)
      if self.pass_through:
        parser.values.pass_through_args.append(opt)
        if self.nargs and self.nargs > 1:
          # value is a tuple if nargs > 1
          string_list = [str(val) for val in list(value)]
          parser.values.pass_through_args.extend(string_list)
        elif value:
          parser.values.pass_through_args.append(str(value))

  """Generate and return the parser with all the options."""
  # Parse options
  usage = "usage: %prog [options] buildbot_config"
  parser = CustomParser(usage=usage, option_class=CustomOption)

  # Main options
  # The remote_pass_through parameter to add_option is implemented by the
  # CustomOption class.  See CustomOption for more information.
  parser.add_option('-a', '--all', action='store_true', dest='print_all',
                    default=False,
                    help=('List all of the buildbot configs available. Use '
                          'with the --list option'))
  parser.add_option('-r', '--buildroot', action='callback', dest='buildroot',
                    type='string', callback=_CheckBuildRootOption,
                    help='Root directory where source is checked out to, and '
                         'where the build occurs. For external build configs, '
                         "defaults to 'trybot' directory at top level of your "
                         'repo-managed checkout.')
  parser.add_remote_option('--chrome_rev', default=None, type='string',
                           action='callback', dest='chrome_rev',
                           callback=_CheckChromeRevOption,
                           help=('Revision of Chrome to use, of type [%s]'
                                 % '|'.join(constants.VALID_CHROME_REVISIONS)))
  parser.add_remote_option('-g', '--gerrit-patches', action='append',
                           default=[], type='string',
                           metavar="'Id1 *int_Id2...IdN'",
                           help=("Space-separated list of short-form Gerrit "
                                 "Change-Id's or change numbers to patch. "
                                 "Please prepend '*' to internal Change-Id's"))
  parser.add_option('-l', '--list', action='store_true', dest='list',
                    default=False,
                    help=('List the suggested trybot configs to use.  Use '
                          '--all to list all of the available configs.'))
  parser.add_option('--local', default=False, action='store_true',
                    help=('Specifies that this tryjob should be run locally.'))
  parser.add_option('-p', '--local-patches', action='append', default=[],
                    metavar="'<project1>[:<branch1>]...<projectN>[:<branchN>]'",
                    help=('Space-separated list of project branches with '
                          'patches to apply.  Projects are specified by name. '
                          'If no branch is specified the current branch of the '
                          'project will be used.'))
  parser.add_remote_option('--profile', default=None, type='string',
                           action='store', dest='profile',
                           help='Name of profile to sub-specify board variant.')
  parser.add_option('--remote', default=False, action='store_true',
                    help=('Specifies that this tryjob should be run remotely.'))

  # Advanced options
  group = CustomGroup(
      parser,
      'Advanced Options',
      'Caution: use these options at your own risk.')

  group.add_remote_option('--buildbot', dest='buildbot', action='store_true',
                          default=False, help='This is running on a buildbot')
  group.add_remote_option('--buildnumber', help='build number', type='int',
                          default=0)
  group.add_option('--chrome_root', default=None, type='string',
                   action='callback', dest='chrome_root',
                   callback=_CheckChromeRootOption,
                   help='Local checkout of Chrome to use.')
  group.add_remote_option('--chrome_version', default=None, type='string',
                          action='callback', dest='chrome_version',
                          callback=_CheckChromeVersionOption,
                          help='Used with SPEC logic to force a particular SVN '
                               'revision of chrome rather than the latest.')
  group.add_remote_option('--clobber', action='store_true', dest='clobber',
                          default=False,
                          help='Clears an old checkout before syncing')
  group.add_remote_option('--lkgm', action='store_true', dest='lkgm',
                          default=False,
                          help='Sync to last known good manifest blessed by '
                               'PFQ')
  parser.add_option('--log_dir', action='callback', dest='log_dir',
                    type='string', callback=_CheckLogDirOption,
                    help=('Directory where logs are stored.'))
  group.add_remote_option('--maxarchives', dest='max_archive_builds',
                          default=3, type='int',
                          help="Change the local saved build count limit.")
  group.add_remote_option('--noarchive', action='store_false', dest='archive',
                          default=True, help="Don't run archive stage.")
  group.add_remote_option('--nobuild', action='store_false', dest='build',
                          default=True,
                          help="Don't actually build (for cbuildbot dev)")
  group.add_remote_option('--noclean', action='store_false', dest='clean',
                          default=True, help="Don't clean the buildroot")
  group.add_remote_option('--noprebuilts', action='store_false',
                          dest='prebuilts', default=True,
                          help="Don't upload prebuilts.")
  group.add_remote_option('--nosync', action='store_false', dest='sync',
                          default=True, help="Don't sync before building.")
  group.add_remote_option('--nocgroups', action='store_false', dest='cgroups',
                          default=True,
                          help='Disable cbuildbots usage of cgroups.')
  group.add_remote_option('--notests', action='store_false', dest='tests',
                          default=True,
                          help='Override values from buildconfig and run no '
                               'tests.')
  group.add_remote_option('--nouprev', action='store_false', dest='uprev',
                          default=True,
                          help='Override values from buildconfig and never '
                               'uprev.')
  group.add_option('--pass-through', dest='pass_through_args', action='append',
                   type='string', default=[], help=optparse.SUPPRESS_HELP)
  group.add_option('--reference-repo', action='store', default=None,
                   dest='reference_repo',
                   help='Reuse git data stored in an existing repo '
                        'checkout. This can drastically reduce the network '
                        'time spent setting up the trybot checkout.  By '
                        "default, if this option isn't given but cbuildbot "
                        'is invoked from a repo checkout, cbuildbot will '
                        'use the repo root.')
  # Indicates this is running on a remote trybot machine.
  group.add_option('--remote-trybot', dest='remote_trybot', action='store_true',
                   default=False, help=optparse.SUPPRESS_HELP)
  # Patches uploaded by trybot client when run using the -p option.
  group.add_remote_option('--remote-patches', action='append', default=[],
                          help=optparse.SUPPRESS_HELP)
  group.add_option('--resume', action='store_true', default=False,
                   help='Skip stages already successfully completed.')
  group.add_remote_option('--timeout', action='store', type='int', default=0,
                          help='Specify the maximum amount of time this job '
                               'can run for, at which point the build will be '
                               'aborted.  If set to zero, then there is no '
                               'timeout.')
  group.add_option('--test-tryjob', action='store_true',
                   default=False,
                   help='Submit a tryjob to the test repository.  Will not '
                        'show up on the production trybot waterfall.')
  group.add_remote_option('--validation_pool', default=None,
                          help='Path to a pickled validation pool. Intended '
                               'for use only with the commit queue.')
  group.add_remote_option('--version', dest='force_version', default=None,
                          help='Used with manifest logic.  Forces use of this '
                               'version rather than create or get latest.')

  parser.add_option_group(group)

  # Debug options
  group = CustomGroup(parser, "Debug Options")

  group.add_option('--debug', action='store_true', default=None,
                    help='Override some options to run as a developer.')
  group.add_option('--dump_config', action='store_true', dest='dump_config',
                    default=False,
                    help='Dump out build config options, and exit.')
  group.add_option('--notee', action='store_false', dest='tee', default=True,
                    help="Disable logging and internal tee process.  Primarily "
                         "used for debugging cbuildbot itself.")
  parser.add_option_group(group)
  return parser


def _FinishParsing(options, args):
  """Perform some parsing tasks that need to take place after optparse.

  This function needs to be easily testable!  Keep it free of
  environment-dependent code.  Put more detailed usage validation in
  _PostParseCheck().

  Args:
    options, args: The options/args object returned by optparse
  """
  if options.chrome_root:
    if options.chrome_rev != constants.CHROME_REV_LOCAL:
      cros_lib.Die('Chrome rev must be %s if chrome_root is set.' %
                   constants.CHROME_REV_LOCAL)
  else:
    if options.chrome_rev == constants.CHROME_REV_LOCAL:
      cros_lib.Die('Chrome root must be set if chrome_rev is %s.' %
                   constants.CHROME_REV_LOCAL)

  if options.chrome_version:
    if options.chrome_rev != constants.CHROME_REV_SPEC:
      cros_lib.Die('Chrome rev must be %s if chrome_version is set.' %
                   constants.CHROME_REV_SPEC)
  else:
    if options.chrome_rev == constants.CHROME_REV_SPEC:
      cros_lib.Die('Chrome rev must not be %s if chrome_version is not set.' %
                   constants.CHROME_REV_SPEC)

  patches = bool(options.gerrit_patches or options.local_patches)
  if options.remote:
    if options.local:
      cros_lib.Die('Cannot specify both --remote and --local')

    if not options.buildbot and not patches:
      cros_lib.Die('Must provide patches when running with --remote.')

    # --debug needs to be explicitly passed through for remote invocations.
    release_mode_with_patches = (options.buildbot and patches and
                                 '--debug' not in options.pass_through_args)
  else:
    if len(args) > 1:
      cros_lib.Die('Multiple configs not supported if not running with '
                   '--remote.')

    release_mode_with_patches = (options.buildbot and patches and
                                 not options.debug)

  # When running in release mode, make sure we are running with checked-in code.
  # We want checked-in cbuildbot/scripts to prevent errors, and we want to build
  # a release image with checked-in code for CrOS packages.
  if release_mode_with_patches:
    cros_lib.Die('Cannot provide patches when running with --buildbot!')

  if options.buildbot and options.remote_trybot:
    cros_lib.Die('--buildbot and --remote-trybot cannot be used together.')

  # Record whether --debug was set explicitly vs. it was inferred.
  options.debug_forced = False
  if options.debug:
    options.debug_forced = True
  else:
    # We don't set debug by default for
    # 1. --buildbot invocations.
    # 2. --remote invocations, because it needs to push changes to the tryjob
    #    repo.
    options.debug = not options.buildbot and not options.remote


def _SplitAndFlatten(appended_items):
  """Given a list of space-separated items, split into flattened list.

  Given ['abc def', 'hij'] return ['abc', 'def', 'hij'].
  Arguments:
    appended_items: List of delimiter-separated items.

  Returns: Flattened list.
  """
  new_list = []
  for item in appended_items:
    new_list.extend(item.split())
  return new_list


# pylint: disable=W0613
def _PostParseCheck(options, args):
  """Perform some usage validation after we've parsed the arguments

  Args:
    options/args: The options/args object returned by optparse
  """
  if options.resume:
    return

  options.gerrit_patches = _SplitAndFlatten(options.gerrit_patches)
  options.remote_patches = _SplitAndFlatten(options.remote_patches)
  try:
    # TODO(rcui): Split this into two stages, one that parses, another that
    # validates.  Parsing step will be called by _FinishParsing().
    options.local_patches = _CheckLocalPatches(
        _SplitAndFlatten(options.local_patches))
  except optparse.OptionValueError as e:
    cros_lib.Die(str(e))

  default = os.environ.get('CBUILDBOT_DEFAULT_MODE')
  if (default and not any([options.local, options.buildbot,
                           options.remote, options.remote_trybot])):
    cros_lib.Info("CBUILDBOT_DEFAULT_MODE=%s env var detected, using it."
                  % default)
    default = default.lower()
    if default == 'local':
      options.local = True
    elif default == 'remote':
      options.remote = True
    elif default == 'buildbot':
      options.buildbot = True
    else:
      cros_lib.Die("CBUILDBOT_DEFAULT_MODE value %s isn't supported. "
                   % default)


def _ParseCommandLine(parser, argv):
  """Completely parse the commandline arguments"""
  (options, args) = parser.parse_args(argv)
  if options.list:
    _PrintValidConfigs(options.print_all)
    sys.exit(0)

  # Strip out null arguments.
  # TODO(rcui): Remove when buildbot is fixed
  args = [arg for arg in args if arg]
  if not args:
    parser.error('Invalid usage.  Use -h to see usage.  Use -l to list '
                 'supported configs.')

  _FinishParsing(options, args)
  return options, args


def main(argv):
  # Set umask to 022 so files created by buildbot are readable.
  os.umask(022)

  if cros_lib.IsInsideChroot():
    cros_lib.Die('Please run cbuildbot from outside the chroot.')

  parser = _CreateParser()
  (options, args) = _ParseCommandLine(parser, argv)

  _PostParseCheck(options, args)

  if options.remote:
    cros_lib.logger.setLevel(logging.WARNING)

    # Verify configs are valid.
    for bot in args:
      _GetConfig(bot)

    # Verify gerrit patches are valid.
    print 'Verifying patches...'
    _, local_patches = _PreProcessPatches(options.gerrit_patches,
                                          options.local_patches)
    # --debug need to be explicitly passed through for remote invocations.
    if options.buildbot and '--debug' not in options.pass_through_args:
      _ConfirmRemoteBuildbotRun()

    print 'Submitting tryjob...'
    tryjob = remote_try.RemoteTryJob(options, args, local_patches)
    tryjob.Submit(testjob=options.test_tryjob, dryrun=options.debug)
    print 'Tryjob submitted!'
    print ('Go to %s to view the status of your job.'
           % tryjob.GetTrybotWaterfallLink())
    sys.exit(0)
  elif (not options.buildbot and not options.remote_trybot
        and not options.resume and not options.local):
    cros_lib.Warning('Running in LOCAL TRYBOT mode!  Use --remote to submit '
                     'REMOTE tryjobs.  Use --local to suppress this message.')
    cros_lib.Warning('Starting April 30th, --local will be required to run the '
                     'local trybot.')
    time.sleep(5)

  # Only expecting one config
  bot_id = args[-1]
  build_config = _GetConfig(bot_id)

  if options.reference_repo is None:
    repo_path = os.path.join(constants.SOURCE_ROOT, '.repo')
    # If we're being run from a repo checkout, reuse the repo's git pool to
    # cut down on sync time.
    if os.path.exists(repo_path):
      options.reference_repo = constants.SOURCE_ROOT
  elif options.reference_repo:
    if not os.path.exists(options.reference_repo):
      parser.error('Reference path %s does not exist'
                   % (options.reference_repo,))
    elif not os.path.exists(os.path.join(options.reference_repo, '.repo')):
      parser.error('Reference path %s does not look to be the base of a '
                   'repo checkout; no .repo exists in the root.'
                   % (options.reference_repo,))

  if options.buildbot or options.remote_trybot:
    if not options.cgroups:
      parser.error('Options --buildbot/--remote-trybot and --nocgroups cannot '
                   'be used together.  Cgroup support is required for '
                   'buildbot/remote-trybot mode.')
    if not cgroups.Cgroup.CgroupsSupported():
      parser.error('Option --buildbot/--remote-trybot was given, but this '
                   'system does not support cgroups.  Failing.')

    missing = []
    for program in _BUILDBOT_REQUIRED_BINARIES:
      ret = cros_lib.RunCommand('which %s' % program, shell=True,
                                redirect_stderr=True, redirect_stdout=True,
                                error_code_ok=True, print_cmd=False)
      if ret.returncode != 0:
        missing.append(program)

    if missing:
      parser.error("Option --buildbot/--remote-trybot requires the following "
                   "binaries which couldn't be found in $PATH: %s"
                   % (', '.join(missing)))

  if options.reference_repo:
    options.reference_repo = os.path.abspath(options.reference_repo)

  if options.dump_config:
    # This works, but option ordering is bad...
    print 'Configuration %s:' % bot_id
    pretty_printer = pprint.PrettyPrinter(indent=2)
    pretty_printer.pprint(build_config)
    sys.exit(0)

  if not options.buildroot:
    if options.buildbot:
      parser.error('Please specify a buildroot with the --buildroot option.')

    options.buildroot = _DetermineDefaultBuildRoot(build_config['internal'])
    # We use a marker file in the buildroot to indicate the user has
    # consented to using this directory.
    if not os.path.exists(repository.GetTrybotMarkerPath(options.buildroot)):
      _ConfirmBuildRoot(options.buildroot)

  # Sanity check of buildroot- specifically that it's not pointing into the
  # midst of an existing repo since git-repo doesn't support nesting.
  if (not repository.IsARepoRoot(options.buildroot) and
      repository.InARepoRepository(options.buildroot)):
    parser.error('Configured buildroot %s points into a repository checkout, '
                 'rather than the root of it.  This is not supported.'
                 % options.buildroot)

  with cleanup.EnforcedCleanupSection() as critical_section:
    with sudo.SudoKeepAlive():
      with cros_lib.AllowDisabling(options.cgroups,
                                   cgroups.SimpleContainChildren, 'cbuildbot'):
        # Mark everything between EnforcedCleanupSection and here as having to
        # be rolled back via the contextmanager cleanup handlers.  This ensures
        # that sudo bits cannot outlive cbuildbot, that anything cgroups
        # would kill gets killed, etc.
        critical_section.ForkWatchdog()

        with cros_lib.AllowDisabling(options.timeout > 0,
                                     cros_lib.Timeout, options.timeout):
          if not options.buildbot:
            build_config = cbuildbot_config.OverrideConfigForTrybot(
                build_config,
                options.remote_trybot)

          _RunBuildStagesWrapper(options, build_config)
