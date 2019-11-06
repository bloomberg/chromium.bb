# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bootstrap for cbuildbot.

This script is intended to checkout chromite on the branch specified by -b or
--branch (as normally accepted by cbuildbot), and then invoke cbuildbot. Most
arguments are not parsed, only passed along. If a branch is not specified, this
script will use 'master'.

Among other things, this allows us to invoke build configs that exist on a given
branch, but not on TOT.
"""

from __future__ import print_function

import base64
import functools
import os
import time

from chromite.cbuildbot import repository
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import boto_compat
from chromite.lib import build_summary
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import metrics
from chromite.lib import osutils
from chromite.lib import ts_mon_config
from chromite.scripts import cbuildbot

# This number should be incremented when we change the layout of the buildroot
# in a non-backwards compatible way. This wipes all buildroots.
BUILDROOT_BUILDROOT_LAYOUT = 2
_DISTFILES_CACHE_EXPIRY_HOURS = 8 * 24

# Metrics reported to Monarch.
METRIC_PREFIX = 'chromeos/chromite/cbuildbot_launch/'
METRIC_ACTIVE = METRIC_PREFIX + 'active'
METRIC_INVOKED = METRIC_PREFIX + 'invoked'
METRIC_COMPLETED = METRIC_PREFIX + 'completed'
METRIC_PREP = METRIC_PREFIX + 'prep_completed'
METRIC_CLEAN = METRIC_PREFIX + 'clean_buildroot_durations'
METRIC_INITIAL = METRIC_PREFIX + 'initial_checkout_durations'
METRIC_CBUILDBOT = METRIC_PREFIX + 'cbuildbot_durations'
METRIC_CBUILDBOT_INSTANCE = METRIC_PREFIX + 'cbuildbot_instance_durations'
METRIC_CLOBBER = METRIC_PREFIX + 'clobber'
METRIC_BRANCH_CLEANUP = METRIC_PREFIX + 'branch_cleanup'
METRIC_DISTFILES_CLEANUP = METRIC_PREFIX + 'distfiles_cleanup'
METRIC_CHROOT_CLEANUP = METRIC_PREFIX + 'chroot_cleanup'

# Builder state
BUILDER_STATE_FILENAME = '.cbuildbot_build_state.json'


def StageDecorator(functor):
  """A Decorator that adds buildbot stage tags around a method.

  It uses the method name as the stage name, and assumes failure on a true
  return value, or an exception.
  """
  @functools.wraps(functor)
  def wrapped_functor(*args, **kwargs):
    try:
      logging.PrintBuildbotStepName(functor.__name__)
      result = functor(*args, **kwargs)
    except Exception:
      logging.PrintBuildbotStepFailure()
      raise

    if result:
      logging.PrintBuildbotStepFailure()
    return result

  return wrapped_functor


def field(fields, **kwargs):
  """Helper for inserting more fields into a metrics fields dictionary.

  Args:
    fields: Dictionary of metrics fields.
    kwargs: Each argument is a key/value pair to insert into dict.

  Returns:
    Copy of original dictionary with kwargs set as fields.
  """
  f = fields.copy()
  f.update(kwargs)
  return f


def PrependPath(prepend):
  """Generate path with new directory at the beginning.

  Args:
    prepend: Directory to add at the beginning of the path.

  Returns:
    Extended path as a string.
  """
  return os.pathsep.join([prepend, os.environ.get('PATH', os.defpath)])


def PreParseArguments(argv):
  """Extract the branch name from cbuildbot command line arguments.

  Args:
    argv: The command line arguments to parse.

  Returns:
    Branch as a string ('master' if nothing is specified).
  """
  parser = cbuildbot.CreateParser()
  options = cbuildbot.ParseCommandLine(parser, argv)

  if not options.cache_dir:
    options.cache_dir = os.path.join(options.buildroot,
                                     'repository', '.cache')

  options.Freeze()

  # This option isn't required for cbuildbot, but is for us.
  if not options.buildroot:
    cros_build_lib.Die('--buildroot is a required option.')

  return options


def GetCurrentBuildState(options, branch):
  """Extract information about the current build state from command-line args.

  Args:
    options: A parsed options object from a cbuildbot parser.
    branch: The name of the branch this builder was called with.

  Returns:
    A BuildSummary object describing the current build.
  """
  build_state = build_summary.BuildSummary(
      status=constants.BUILDER_STATUS_INFLIGHT,
      buildroot_layout=BUILDROOT_BUILDROOT_LAYOUT,
      branch=branch)
  if options.buildnumber:
    build_state.build_number = options.buildnumber
  if options.buildbucket_id:
    build_state.buildbucket_id = options.buildbucket_id
  if options.master_build_id:
    build_state.master_build_id = options.master_build_id
  return build_state


def GetLastBuildState(root):
  """Fetch the state of the last build run from |root|.

  If the saved state file can't be read or doesn't contain valid JSON, a default
  state will be returned.

  Args:
    root: Root of the working directory tree as a string.

  Returns:
    A BuildSummary object representing the previous build.
  """
  state_file = os.path.join(root, BUILDER_STATE_FILENAME)

  state = build_summary.BuildSummary()

  try:
    state_raw = osutils.ReadFile(state_file)
    state.from_json(state_raw)
  except IOError as e:
    logging.warning('Unable to read %s: %s', state_file, e)
    return state
  except ValueError as e:
    logging.warning('Saved state file %s is not valid JSON: %s', state_file, e)
    return state

  if not state.is_valid():
    logging.warning('Previous build state is not valid.  Ignoring.')
    state = build_summary.BuildSummary()

  return state


def SetLastBuildState(root, new_state):
  """Save the state of the last build under |root|.

  Args:
    root: Root of the working directory tree as a string.
    new_state: BuildSummary object containing the state to be saved.
  """
  state_file = os.path.join(root, BUILDER_STATE_FILENAME)
  osutils.WriteFile(state_file, new_state.to_json())

  # Remove old state file.  Its contents have been migrated into the new file.
  old_state_file = os.path.join(root, '.cbuildbot_launch_state')
  osutils.SafeUnlink(old_state_file)


def _MaybeCleanDistfiles(cache_dir, distfiles_ts):
  """Cleans the distfiles directory if too old.

  Args:
    cache_dir: Directory of the cache, as a string.
    distfiles_ts: A timestamp str for the last time distfiles was cleaned. May
    be None.

  Returns:
    The new distfiles_ts to persist in state.
  """
  # distfiles_ts can be None for a fresh environment, which means clean.
  if distfiles_ts is None:
    return time.time()

  distfiles_age = (time.time() - distfiles_ts) / 3600.0
  if distfiles_age < _DISTFILES_CACHE_EXPIRY_HOURS:
    return distfiles_ts

  logging.info('Remove old distfiles cache (cache expiry %d hours)',
               _DISTFILES_CACHE_EXPIRY_HOURS)
  osutils.RmDir(os.path.join(cache_dir, 'distfiles'),
                ignore_missing=True, sudo=True)
  metrics.Counter(METRIC_DISTFILES_CLEANUP).increment(
      fields=field({}, reason='cache_expired'))

  # Cleaned cache, so reset distfiles_ts
  return time.time()


def SanitizeCacheDir(cache_dir):
  """Make certain the .cache directory is valid.

  Args:
    cache_dir: Directory of the cache, as a string.
  """
  logging.info('Cleaning up cache dir at %s', cache_dir)
  # Verify that .cache is writable by the current user.
  try:
    osutils.Touch(os.path.join(cache_dir, '.cbuildbot_launch'), makedirs=True)
  except IOError:
    logging.info('Bad Permissions for cache dir, wiping: %s', cache_dir)
    osutils.RmDir(cache_dir, sudo=True)
    osutils.Touch(os.path.join(cache_dir, '.cbuildbot_launch'), makedirs=True)

  osutils.RmDir(os.path.join(cache_dir, 'paygen_cache'),
                ignore_missing=True, sudo=True)
  logging.info('Finished cleaning cache_dir.')


@StageDecorator
def CleanBuildRoot(root, repo, cache_dir, build_state):
  """Some kinds of branch transitions break builds.

  This method ensures that cbuildbot's buildroot is a clean checkout on the
  given branch when it starts. If necessary (a branch transition) it will wipe
  assorted state that cannot be safely reused from the previous build.

  Args:
    root: Root directory owned by cbuildbot_launch.
    repo: repository.RepoRepository instance.
    cache_dir: Cache directory.
    build_state: BuildSummary object containing the current build state that
        will be saved into the cleaned root.  The distfiles_ts property will
        be updated if the distfiles cache is cleaned.
  """
  previous_state = GetLastBuildState(root)
  SetLastBuildState(root, build_state)
  SanitizeCacheDir(cache_dir)
  build_state.distfiles_ts = _MaybeCleanDistfiles(
      cache_dir, previous_state.distfiles_ts)

  if previous_state.buildroot_layout != BUILDROOT_BUILDROOT_LAYOUT:
    logging.PrintBuildbotStepText('Unknown layout: Wiping buildroot.')
    metrics.Counter(METRIC_CLOBBER).increment(
        fields=field({}, reason='layout_change'))
    chroot_dir = os.path.join(root, constants.DEFAULT_CHROOT_DIR)
    if os.path.exists(chroot_dir) or os.path.exists(chroot_dir + '.img'):
      cros_sdk_lib.CleanupChrootMount(chroot_dir, delete=True)
    osutils.RmDir(root, ignore_missing=True, sudo=True)
    osutils.RmDir(cache_dir, ignore_missing=True, sudo=True)
  else:
    if previous_state.branch != repo.branch:
      logging.PrintBuildbotStepText('Branch change: Cleaning buildroot.')
      logging.info('Unmatched branch: %s -> %s', previous_state.branch,
                   repo.branch)
      metrics.Counter(METRIC_BRANCH_CLEANUP).increment(
          fields=field({}, old_branch=previous_state.branch))

      logging.info('Remove Chroot.')
      chroot_dir = os.path.join(repo.directory, constants.DEFAULT_CHROOT_DIR)
      if os.path.exists(chroot_dir) or os.path.exists(chroot_dir + '.img'):
        cros_sdk_lib.CleanupChrootMount(chroot_dir, delete=True)

      logging.info('Remove Chrome checkout.')
      osutils.RmDir(os.path.join(repo.directory, '.cache', 'distfiles'),
                    ignore_missing=True, sudo=True)

  try:
    # If there is any failure doing the cleanup, wipe everything.
    # The previous run might have been killed in the middle leaving stale git
    # locks. Clean those up, first.
    repo.PreLoad()

    # If the previous build didn't exit normally, run an expensive step to
    # cleanup abandoned git locks.
    if previous_state.status not in (constants.BUILDER_STATUS_FAILED,
                                     constants.BUILDER_STATUS_PASSED):
      repo.CleanStaleLocks()

    repo.BuildRootGitCleanup(prune_all=True)
  except Exception:
    logging.info('Checkout cleanup failed, wiping buildroot:', exc_info=True)
    metrics.Counter(METRIC_CLOBBER).increment(
        fields=field({}, reason='repo_cleanup_failure'))
    repository.ClearBuildRoot(repo.directory)

  # Ensure buildroot exists. Save the state we are prepped for.
  osutils.SafeMakedirs(repo.directory)
  SetLastBuildState(root, build_state)


@StageDecorator
def InitialCheckout(repo):
  """Preliminary ChromeOS checkout.

  Perform a complete checkout of ChromeOS on the specified branch. This does NOT
  match what the build needs, but ensures the buildroot both has a 'hot'
  checkout, and is close enough that the branched cbuildbot can successfully get
  the right checkout.

  This checks out full ChromeOS, even if a ChromiumOS build is going to be
  performed. This is because we have no knowledge of the build config to be
  used.

  Args:
    repo: repository.RepoRepository instance.
  """
  logging.PrintBuildbotStepText('Branch: %s' % repo.branch)
  logging.info('Bootstrap script starting initial sync on branch: %s',
               repo.branch)
  repo.PreLoad('/preload/chromeos')
  repo.Sync(detach=True)


def ShouldFixBotoCerts(options):
  """Decide if FixBotoCerts should be applied for this branch."""
  try:
    # Only apply to factory and firmware branches.
    branch = options.branch or ''
    prefix = branch.split('-')[0]
    if prefix not in ('factory', 'firmware'):
      return False

    # Only apply to "old" branches.
    if branch.endswith('.B'):
      version = branch[:-2].split('-')[-1]
      major = int(version.split('.')[0])
      return major <= 9667  # This is the newest known to be failing.

    return False
  except Exception as e:
    logging.warning(' failed: %s', e)
    # Conservatively continue without the fix.
    return False


@StageDecorator
def Cbuildbot(buildroot, depot_tools_path, argv):
  """Start cbuildbot in specified directory with all arguments.

  Args:
    buildroot: Directory to be passed to cbuildbot with --buildroot.
    depot_tools_path: Directory for depot_tools to be used by cbuildbot.
    argv: Command line options passed to cbuildbot_launch.

  Returns:
    Return code of cbuildbot as an integer.
  """
  logging.info('Bootstrap cbuildbot in: %s', buildroot)

  # Fixup buildroot parameter.
  argv = argv[:]
  for i, arg in enumerate(argv):
    if arg in ('-r', '--buildroot'):
      argv[i + 1] = buildroot

  # This filters out command line arguments not supported by older versions
  # of cbuildbot.
  parser = cbuildbot.CreateParser()
  options = cbuildbot.ParseCommandLine(parser, argv)
  cbuildbot_path = os.path.join(buildroot, 'chromite', 'bin', 'cbuildbot')
  cmd = sync_stages.BootstrapStage.FilterArgsForTargetCbuildbot(
      buildroot, cbuildbot_path, options)

  # We want cbuildbot to use branched depot_tools scripts from our manifest,
  # so that depot_tools is branched to match cbuildbot.
  logging.info('Adding depot_tools into PATH: %s', depot_tools_path)
  extra_env = {'PATH': PrependPath(depot_tools_path)}

  # TODO(crbug.com/845304): Remove once underlying boto issues are resolved.
  fix_boto = ShouldFixBotoCerts(options)

  with boto_compat.FixBotoCerts(activate=fix_boto):
    result = cros_build_lib.RunCommand(
        cmd, extra_env=extra_env, error_code_ok=True, cwd=buildroot)

  return result.returncode


@StageDecorator
def CleanupChroot(buildroot):
  """Unmount/clean up an image-based chroot without deleting the backing image.

  Args:
    buildroot: Directory containing the chroot to be cleaned up.
  """
  chroot_dir = os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR)
  logging.info('Cleaning up chroot at %s', chroot_dir)
  if os.path.exists(chroot_dir) or os.path.exists(chroot_dir + '.img'):
    cros_sdk_lib.CleanupChrootMount(chroot_dir, delete=False)


def ConfigureGlobalEnvironment():
  """Setup process wide environmental changes."""
  # Set umask to 022 so files created by buildbot are readable.
  os.umask(0o22)

  # These variables can interfere with LANG / locale behavior.
  unwanted_local_vars = [
      'LC_ALL', 'LC_CTYPE', 'LC_COLLATE', 'LC_TIME', 'LC_NUMERIC',
      'LC_MONETARY', 'LC_MESSAGES', 'LC_PAPER', 'LC_NAME', 'LC_ADDRESS',
      'LC_TELEPHONE', 'LC_MEASUREMENT', 'LC_IDENTIFICATION',
  ]
  for v in unwanted_local_vars:
    os.environ.pop(v, None)

  # This variable is required for repo sync's to work in all cases.
  os.environ['LANG'] = 'en_US.UTF-8'


def _main(options, argv):
  """main method of script.

  Args:
    options: preparsed options object for the build.
    argv: All command line arguments to pass as list of strings.

  Returns:
    Return code of cbuildbot as an integer.
  """
  branchname = options.branch or 'master'
  root = options.buildroot
  buildroot = os.path.join(root, 'repository')
  workspace = os.path.join(root, 'workspace')
  depot_tools_path = os.path.join(buildroot, constants.DEPOT_TOOLS_SUBPATH)

  # Does the entire build pass or fail.
  with metrics.Presence(METRIC_ACTIVE), \
       metrics.SuccessCounter(METRIC_COMPLETED) as s_fields:

    # Preliminary set, mostly command line parsing.
    with metrics.SuccessCounter(METRIC_INVOKED):
      if options.enable_buildbot_tags:
        logging.EnableBuildbotMarkers()
      ConfigureGlobalEnvironment()

    # Prepare the buildroot with source for the build.
    with metrics.SuccessCounter(METRIC_PREP):
      manifest_url = config_lib.GetSiteParams().MANIFEST_INT_URL
      repo = repository.RepoRepository(manifest_url, buildroot,
                                       branch=branchname,
                                       git_cache_dir=options.git_cache_dir)
      previous_build_state = GetLastBuildState(root)

      # Clean up the buildroot to a safe state.
      with metrics.SecondsTimer(METRIC_CLEAN):
        build_state = GetCurrentBuildState(options, branchname)
        CleanBuildRoot(root, repo, options.cache_dir, build_state)

      # Get a checkout close enough to the branch that cbuildbot can handle it.
      if options.sync:
        with metrics.SecondsTimer(METRIC_INITIAL):
          InitialCheckout(repo)

    # Run cbuildbot inside the full ChromeOS checkout, on the specified branch.
    with metrics.SecondsTimer(METRIC_CBUILDBOT), \
         metrics.SecondsInstanceTimer(METRIC_CBUILDBOT_INSTANCE):
      if previous_build_state.is_valid():
        argv.append('--previous-build-state')
        argv.append(base64.b64encode(previous_build_state.to_json()))
      argv.extend(['--workspace', workspace])

      if not options.cache_dir_specified:
        argv.extend(['--cache-dir', options.cache_dir])

      result = Cbuildbot(buildroot, depot_tools_path, argv)
      s_fields['success'] = (result == 0)

      build_state.status = (
          constants.BUILDER_STATUS_PASSED
          if result == 0 else constants.BUILDER_STATUS_FAILED)
      SetLastBuildState(root, build_state)

      with metrics.SecondsTimer(METRIC_CHROOT_CLEANUP):
        CleanupChroot(buildroot)

      return result


def main(argv):
  options = PreParseArguments(argv)
  metric_fields = {
      'branch_name': options.branch or 'master',
      'build_config': options.build_config_name,
      'tryjob': options.remote_trybot,
  }

  # Enable Monarch metrics gathering.
  with ts_mon_config.SetupTsMonGlobalState('cbuildbot_launch',
                                           common_metric_fields=metric_fields,
                                           indirect=True):
    return _main(options, argv)
