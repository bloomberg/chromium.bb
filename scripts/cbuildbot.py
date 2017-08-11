# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

from __future__ import print_function

import distutils.version # pylint: disable=import-error,no-name-in-module
import glob
import json
import mock
import optparse  # pylint: disable=deprecated-module
import os
import pickle
import sys

from chromite.cbuildbot import builders
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import remote_try
from chromite.cbuildbot import repository
from chromite.cbuildbot import tee
from chromite.cbuildbot import topology
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot.stages import completion_stages
from chromite.lib.const import waterfall
from chromite.lib import builder_status_lib
from chromite.lib import cidb
from chromite.lib import cgroups
from chromite.lib import cleanup
from chromite.lib import commandline
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import retry_stats
from chromite.lib import sudo
from chromite.lib import timeout_util
from chromite.lib import tree_status
from chromite.lib import ts_mon_config


_DEFAULT_LOG_DIR = 'cbuildbot_logs'
_BUILDBOT_LOG_FILE = 'cbuildbot.log'
_DEFAULT_EXT_BUILDROOT = 'trybot'
_DEFAULT_INT_BUILDROOT = 'trybot-internal'
_BUILDBOT_REQUIRED_BINARIES = ('pbzip2',)
_API_VERSION_ATTR = 'api_version'


def _PrintValidConfigs(site_config, display_all=False):
  """Print a list of valid buildbot configs.

  Args:
    site_config: config_lib.SiteConfig containing all config info.
    display_all: Print all configs.  Otherwise, prints only configs with
                 trybot_list=True.
  """
  def _GetSortKey(config_name):
    config_dict = site_config[config_name]
    return (not config_dict['trybot_list'], config_dict['description'],
            config_name)

  COLUMN_WIDTH = 45
  if not display_all:
    print('Note: This is the common list; for all configs, use --all.')
  print('config'.ljust(COLUMN_WIDTH), 'description')
  print('------'.ljust(COLUMN_WIDTH), '-----------')
  config_names = site_config.keys()
  config_names.sort(key=_GetSortKey)
  for name in config_names:
    if display_all or site_config[name]['trybot_list']:
      desc = site_config[name].get('description')
      desc = desc if desc else ''
      print(name.ljust(COLUMN_WIDTH), desc)


def _ConfirmBuildRoot(buildroot):
  """Confirm with user the inferred buildroot, and mark it as confirmed."""
  logging.warning('Using default directory %s as buildroot', buildroot)
  if not cros_build_lib.BooleanPrompt(default=False):
    print('Please specify a different buildroot via the --buildroot option.')
    sys.exit(0)

  if not os.path.exists(buildroot):
    os.mkdir(buildroot)

  repository.CreateTrybotMarker(buildroot)


def _ConfirmRemoteBuildbotRun():
  """Confirm user wants to run with --buildbot --remote."""
  logging.warning(
      'You are about to launch a PRODUCTION job!  This is *NOT* a '
      'trybot run! Are you sure?')
  if not cros_build_lib.BooleanPrompt(default=False):
    print('Please specify --pass-through="--debug".')
    sys.exit(0)


def _DetermineDefaultBuildRoot(sourceroot, internal_build):
  """Default buildroot to be under the directory that contains current checkout.

  Args:
    internal_build: Whether the build is an internal build
    sourceroot: Use specified sourceroot.
  """
  if not repository.IsARepoRoot(sourceroot):
    cros_build_lib.Die(
        'Could not find root of local checkout at %s.  Please specify '
        'using the --sourceroot option.' % sourceroot)

  # Place trybot buildroot under the directory containing current checkout.
  top_level = os.path.dirname(os.path.realpath(sourceroot))
  if internal_build:
    buildroot = os.path.join(top_level, _DEFAULT_INT_BUILDROOT)
  else:
    buildroot = os.path.join(top_level, _DEFAULT_EXT_BUILDROOT)

  return buildroot


def _BackupPreviousLog(log_file, backup_limit=25):
  """Rename previous log.

  Args:
    log_file: The absolute path to the previous log.
    backup_limit: Maximum number of old logs to keep.
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


def _IsDistributedBuilder(options, chrome_rev, build_config):
  """Determines whether the builder should be a DistributedBuilder.

  Args:
    options: options passed on the commandline.
    chrome_rev: Chrome revision to build.
    build_config: Builder configuration dictionary.

  Returns:
    True if the builder should be a distributed_builder
  """
  if build_config['pre_cq']:
    return True
  elif not options.buildbot:
    return False
  elif chrome_rev in (constants.CHROME_REV_TOT,
                      constants.CHROME_REV_LOCAL,
                      constants.CHROME_REV_SPEC):
    # We don't do distributed logic to TOT Chrome PFQ's, nor local
    # chrome roots (e.g. chrome try bots)
    # TODO(davidjames): Update any builders that rely on this logic to use
    # manifest_version=False instead.
    return False
  elif build_config['manifest_version']:
    return True

  return False


def _RunBuildStagesWrapper(options, site_config, build_config):
  """Helper function that wraps RunBuildStages()."""
  logging.info('cbuildbot was executed with args %s' %
               cros_build_lib.CmdToStr(sys.argv))

  chrome_rev = build_config['chrome_rev']
  if options.chrome_rev:
    chrome_rev = options.chrome_rev
  if chrome_rev == constants.CHROME_REV_TOT:
    options.chrome_version = gob_util.GetTipOfTrunkRevision(
        constants.CHROMIUM_GOB_URL)
    options.chrome_rev = constants.CHROME_REV_SPEC

  # If it's likely we'll need to build Chrome, fetch the source.
  if build_config['sync_chrome'] is None:
    options.managed_chrome = (
        chrome_rev != constants.CHROME_REV_LOCAL and
        (not build_config['usepkg_build_packages'] or chrome_rev or
         build_config['profile'] or options.rietveld_patches))
  else:
    options.managed_chrome = build_config['sync_chrome']

  if options.managed_chrome:
    # Tell Chrome to fetch the source locally.
    internal = constants.USE_CHROME_INTERNAL in build_config['useflags']
    chrome_src = 'chrome-src-internal' if internal else 'chrome-src'
    target_name = 'target'
    if options.branch:
      # Tie the cache per branch
      target_name = 'target-%s' % options.branch
    options.chrome_root = os.path.join(options.cache_dir, 'distfiles',
                                       target_name, chrome_src)
    # Create directory if in need
    osutils.SafeMakedirsNonRoot(options.chrome_root)
  elif options.rietveld_patches:
    cros_build_lib.Die('This builder does not support Rietveld patches.')

  metadata_dump_dict = {}
  if options.metadata_dump:
    with open(options.metadata_dump, 'r') as metadata_file:
      metadata_dump_dict = json.loads(metadata_file.read())

  # We are done munging options values, so freeze options object now to avoid
  # further abuse of it.
  # TODO(mtennant): one by one identify each options value override and see if
  # it can be handled another way.  Try to push this freeze closer and closer
  # to the start of the script (e.g. in or after _PostParseCheck).
  options.Freeze()

  with parallel.Manager() as manager:
    builder_run = cbuildbot_run.BuilderRun(
        options, site_config, build_config, manager)
    if metadata_dump_dict:
      builder_run.attrs.metadata.UpdateWithDict(metadata_dump_dict)

    if builder_run.config.builder_class_name is None:
      # TODO: This should get relocated to chromeos_config.
      if _IsDistributedBuilder(options, chrome_rev, build_config):
        builder_cls_name = 'simple_builders.DistributedBuilder'
      else:
        builder_cls_name = 'simple_builders.SimpleBuilder'
      builder_cls = builders.GetBuilderClass(builder_cls_name)
      builder = builder_cls(builder_run)
    else:
      builder = builders.Builder(builder_run)

    if not builder.Run():
      sys.exit(1)


# Parser related functions
def _CheckLocalPatches(sourceroot, local_patches):
  """Do an early quick check of the passed-in patches.

  If the branch of a project is not specified we append the current branch the
  project is on.

  TODO(davidjames): The project:branch format isn't unique, so this means that
  we can't differentiate what directory the user intended to apply patches to.
  We should references by directory instead.

  Args:
    sourceroot: The checkout where patches are coming from.
    local_patches: List of patches to check in project:branch format.

  Returns:
    A list of patches that have been verified, in project:branch format.
  """
  verified_patches = []
  manifest = git.ManifestCheckout.Cached(sourceroot)
  for patch in local_patches:
    project, _, branch = patch.partition(':')

    checkouts = manifest.FindCheckouts(project)
    if not checkouts:
      cros_build_lib.Die('Project %s does not exist.' % (project,))
    if len(checkouts) > 1:
      cros_build_lib.Die(
          'We do not yet support local patching for projects that are checked '
          'out to multiple directories. Try uploading your patch to gerrit '
          'and referencing it via the -g option instead.'
      )

    ok = False
    for checkout in checkouts:
      project_dir = checkout.GetPath(absolute=True)

      # If no branch was specified, we use the project's current branch.
      if not branch:
        local_branch = git.GetCurrentBranch(project_dir)
      else:
        local_branch = branch

      if local_branch and git.DoesCommitExistInRepo(project_dir, local_branch):
        verified_patches.append('%s:%s' % (project, local_branch))
        ok = True

    if not ok:
      if branch:
        cros_build_lib.Die('Project %s does not have branch %s'
                           % (project, branch))
      else:
        cros_build_lib.Die('Project %s is not on a branch!' % (project,))

  return verified_patches


def _CheckChromeVersionOption(_option, _opt_str, value, parser):
  """Upgrade other options based on chrome_version being passed."""
  value = value.strip()

  if parser.values.chrome_rev is None and value:
    parser.values.chrome_rev = constants.CHROME_REV_SPEC

  parser.values.chrome_version = value


def _CheckChromeRootOption(_option, _opt_str, value, parser):
  """Validate and convert chrome_root to full-path form."""
  if parser.values.chrome_rev is None:
    parser.values.chrome_rev = constants.CHROME_REV_LOCAL

  parser.values.chrome_root = value


def FindCacheDir(_parser, _options):
  return None


class CustomGroup(optparse.OptionGroup):
  """Custom option group which supports arguments passed-through to trybot."""

  def add_remote_option(self, *args, **kwargs):
    """For arguments that are passed-through to remote trybot."""
    return optparse.OptionGroup.add_option(self, *args,
                                           remote_pass_through=True,
                                           **kwargs)


class CustomOption(commandline.FilteringOption):
  """Subclass FilteringOption class to implement pass-through and api."""

  def __init__(self, *args, **kwargs):
    # The remote_pass_through argument specifies whether we should directly
    # pass the argument (with its value) onto the remote trybot.
    self.pass_through = kwargs.pop('remote_pass_through', False)
    self.api_version = int(kwargs.pop('api', '0'))
    commandline.FilteringOption.__init__(self, *args, **kwargs)


class CustomParser(commandline.FilteringParser):
  """Custom option parser which supports arguments passed-trhough to trybot"""

  DEFAULT_OPTION_CLASS = CustomOption

  def add_remote_option(self, *args, **kwargs):
    """For arguments that are passed-through to remote trybot."""
    return self.add_option(*args, remote_pass_through=True, **kwargs)


def CreateParser():
  """Expose _CreateParser publicly."""
  # Name _CreateParser is needed for commandline library.
  return _CreateParser()


def _CreateParser():
  """Generate and return the parser with all the options."""
  # Parse options
  usage = 'usage: %prog [options] buildbot_config [buildbot_config ...]'
  parser = CustomParser(usage=usage, caching=FindCacheDir)

  # Main options
  parser.add_option('-l', '--list', action='store_true', dest='list',
                    default=False,
                    help='List the suggested trybot configs to use (see --all)')
  parser.add_option('-a', '--all', action='store_true', dest='print_all',
                    default=False,
                    help='List all of the buildbot configs available w/--list')

  parser.add_option('--local', action='store_true', default=False,
                    help='Specifies that this tryjob should be run locally. '
                         'Implies --debug.')
  parser.add_option('--remote', action='store_true', default=False,
                    help='Specifies that this tryjob should be run remotely.')

  parser.add_remote_option('-b', '--branch',
                           help='The manifest branch to test.  The branch to '
                                'check the buildroot out to.')
  parser.add_option('-r', '--buildroot', type='path', dest='buildroot',
                    help='Root directory where source is checked out to, and '
                         'where the build occurs. For external build configs, '
                         "defaults to 'trybot' directory at top level of your "
                         'repo-managed checkout.')
  parser.add_option('--bootstrap-dir', type='path',
                    help='Bootstrapping cbuildbot may involve checking out '
                         'multiple copies of chromite. All these checkouts '
                         'will be contained in the directory specified here. '
                         'Default:%s' % osutils.GetGlobalTempDir())
  parser.add_remote_option('--android_rev', type='choice',
                           choices=constants.VALID_ANDROID_REVISIONS,
                           help=('Revision of Android to use, of type [%s]'
                                 % '|'.join(constants.VALID_ANDROID_REVISIONS)))
  parser.add_remote_option('--chrome_rev', type='choice',
                           choices=constants.VALID_CHROME_REVISIONS,
                           help=('Revision of Chrome to use, of type [%s]'
                                 % '|'.join(constants.VALID_CHROME_REVISIONS)))
  parser.add_remote_option('--profile',
                           help='Name of profile to sub-specify board variant.')
  parser.add_option('-c', '--config_repo',
                    help='Deprecated option. Do not use!')
  # TODO(crbug.com/279618): Running GOMA is under development. Following
  # flags are added for development purpose due to repository dependency,
  # but not officially supported yet.
  parser.add_option('--goma_dir', type='path',
                    api=constants.REEXEC_API_GOMA,
                    help='Specify a directory containing goma. When this is '
                         'set, GOMA is used to build Chrome.')
  parser.add_option('--goma_client_json', type='path',
                    api=constants.REEXEC_API_GOMA,
                    help='Specify a service-account-goma-client.json path. '
                         'The file is needed on bots to run GOMA.')

  #
  # Patch selection options.
  #

  group = CustomGroup(
      parser,
      'Patch Options')

  group.add_remote_option('-g', '--gerrit-patches', action='split_extend',
                          type='string', default=[],
                          metavar="'Id1 *int_Id2...IdN'",
                          help='Space-separated list of short-form Gerrit '
                               "Change-Id's or change numbers to patch. "
                               "Please prepend '*' to internal Change-Id's")
  group.add_remote_option('-G', '--rietveld-patches', action='split_extend',
                          type='string', default=[],
                          metavar="'id1[:subdir1]...idN[:subdirN]'",
                          help='Space-separated list of short-form Rietveld '
                               'issue numbers to patch. If no subdir is '
                               'specified, the src directory is used.')
  group.add_option('-p', '--local-patches', action='split_extend', default=[],
                   metavar="'<project1>[:<branch1>]...<projectN>[:<branchN>]'",
                   help='Space-separated list of project branches with '
                        'patches to apply.  Projects are specified by name. '
                        'If no branch is specified the current branch of the '
                        'project will be used.')

  parser.add_argument_group(group)

  #
  # Remote trybot options.
  #

  group = CustomGroup(
      parser,
      'Remote Trybot Options (--remote)')

  # TODO(dgarrett): Remove after a reasonable delay.
  group.add_option('--use-buildbucket', action='store_true',
                   dest='deprecated_use_buildbucket',
                   help='Deprecated option. Ignored.')

  group.add_option('--do-not-use-buildbucket', action='store_false',
                   dest='use_buildbucket', default=True,
                   help='Use buildbucket instead of git to request'
                        'the tryjob(s).')

  group.add_remote_option('--hwtest', action='store_true', default=False,
                          help='Run the HWTest stage (tests on real hardware)')
  group.add_option('--remote-description',
                   help='Attach an optional description to a --remote run '
                        'to make it easier to identify the results when it '
                        'finishes')
  group.add_option('--slaves', action='split_extend', default=[],
                   help='Specify specific remote tryslaves to run on (e.g. '
                        'build149-m2); if the bot is busy, it will be queued')
  group.add_remote_option('--channel', action='split_extend', dest='channels',
                          default=[],
                          help='Specify a channel for a payloads trybot. Can '
                               'be specified multiple times. No valid for '
                               'non-payloads configs.')
  group.add_option('--test-tryjob', action='store_true', default=False,
                   help='Submit a tryjob to the test repository.  Will not '
                        'show up on the production trybot waterfall.')
  group.add_option('--committer-email', type='string',
                   help='Override default git committer email.')

  parser.add_argument_group(group)

  #
  # Branch creation options.
  #

  group = CustomGroup(
      parser,
      'Branch Creation Options (used with branch-util)')

  group.add_remote_option('--branch-name',
                          help='The branch to create or delete.')
  group.add_remote_option('--delete-branch', action='store_true', default=False,
                          help='Delete the branch specified in --branch-name.')
  group.add_remote_option('--rename-to',
                          help='Rename a branch to the specified name.')
  group.add_remote_option('--force-create', action='store_true', default=False,
                          help='Overwrites an existing branch.')
  group.add_remote_option('--skip-remote-push', action='store_true',
                          default=False,
                          help='Do not actually push to remote git repos.  '
                               'Used for end-to-end testing branching.')

  parser.add_argument_group(group)

  #
  # Advanced options.
  #

  group = CustomGroup(
      parser,
      'Advanced Options',
      'Caution: use these options at your own risk.')

  group.add_remote_option('--bootstrap-args', action='append', default=[],
                          help='Args passed directly to the bootstrap re-exec '
                               'to skip verification by the bootstrap code')
  group.add_remote_option('--buildbot', action='store_true', dest='buildbot',
                          default=False,
                          help='This is running on a buildbot. '
                               'This can be used to make a build operate '
                               'like an official builder, e.g. generate '
                               'new version numbers and archive official '
                               'artifacts and such. This should only be '
                               'used if you are confident in what you are '
                               'doing, as it will make automated commits.')
  parser.add_remote_option('--repo-cache', type='path', dest='_repo_cache',
                           help='Present for backwards compatibility, ignored.')
  group.add_remote_option('--no-buildbot-tags', action='store_false',
                          dest='enable_buildbot_tags', default=True,
                          help='Suppress buildbot specific tags from log '
                               'output. This is used to hide recursive '
                               'cbuilbot runs on the waterfall.')
  group.add_remote_option('--buildnumber', type='int', default=0,
                          help='build number')
  group.add_option('--chrome_root', action='callback', type='path',
                   callback=_CheckChromeRootOption,
                   help='Local checkout of Chrome to use.')
  group.add_remote_option('--chrome_version', action='callback', type='string',
                          dest='chrome_version',
                          callback=_CheckChromeVersionOption,
                          help='Used with SPEC logic to force a particular '
                               'git revision of chrome rather than the '
                               'latest.')
  group.add_remote_option('--clobber', action='store_true', default=False,
                          help='Clears an old checkout before syncing')
  group.add_remote_option('--latest-toolchain', action='store_true',
                          default=False,
                          help='Use the latest toolchain.')
  parser.add_option('--log_dir', dest='log_dir', type='path',
                    help='Directory where logs are stored.')
  group.add_remote_option('--maxarchives', type='int',
                          dest='max_archive_builds', default=3,
                          help='Change the local saved build count limit.')
  parser.add_remote_option('--manifest-repo-url',
                           help='Overrides the default manifest repo url.')
  group.add_remote_option('--compilecheck', action='store_true', default=False,
                          help='Only verify compilation and unit tests.')
  group.add_remote_option('--noarchive', action='store_false', dest='archive',
                          default=True, help="Don't run archive stage.")
  group.add_remote_option('--nobootstrap', action='store_false',
                          dest='bootstrap', default=True,
                          help="Don't checkout and run from a standalone "
                               'chromite repo.')
  group.add_remote_option('--nobuild', action='store_false', dest='build',
                          default=True,
                          help="Don't actually build (for cbuildbot dev)")
  group.add_remote_option('--noclean', action='store_false', dest='clean',
                          default=True, help="Don't clean the buildroot")
  group.add_remote_option('--nocgroups', action='store_false', dest='cgroups',
                          default=True,
                          help='Disable cbuildbots usage of cgroups.')
  group.add_remote_option('--nochromesdk', action='store_false',
                          dest='chrome_sdk', default=True,
                          help="Don't run the ChromeSDK stage which builds "
                               'Chrome outside of the chroot.')
  group.add_remote_option('--noprebuilts', action='store_false',
                          dest='prebuilts', default=True,
                          help="Don't upload prebuilts.")
  group.add_remote_option('--nopatch', action='store_false',
                          dest='postsync_patch', default=True,
                          help="Don't run PatchChanges stage.  This does not "
                               'disable patching in of chromite patches '
                               'during BootstrapStage.')
  group.add_remote_option('--nopaygen', action='store_false',
                          dest='paygen', default=True,
                          help="Don't generate payloads.")
  group.add_remote_option('--noreexec', action='store_false',
                          dest='postsync_reexec', default=True,
                          help="Don't reexec into the buildroot after syncing.")
  group.add_remote_option('--nosdk', action='store_true', default=False,
                          help='Re-create the SDK from scratch.')
  group.add_remote_option('--nosync', action='store_false', dest='sync',
                          default=True, help="Don't sync before building.")
  group.add_remote_option('--notests', action='store_false', dest='tests',
                          default=True,
                          help='Override values from buildconfig, run no '
                               'tests, and build no autotest and artifacts.')
  group.add_remote_option('--novmtests', action='store_false', dest='vmtests',
                          default=True,
                          help='Override values from buildconfig, run no '
                               'vmtests.')
  group.add_remote_option('--noimagetests', action='store_false',
                          dest='image_test', default=True,
                          help='Override values from buildconfig and run no '
                               'image tests.')
  group.add_remote_option('--nouprev', action='store_false', dest='uprev',
                          default=True,
                          help='Override values from buildconfig and never '
                               'uprev.')
  group.add_option('--reference-repo',
                   help='Reuse git data stored in an existing repo '
                        'checkout. This can drastically reduce the network '
                        'time spent setting up the trybot checkout.  By '
                        "default, if this option isn't given but cbuildbot "
                        'is invoked from a repo checkout, cbuildbot will '
                        'use the repo root.')
  group.add_option('--resume', action='store_true', default=False,
                   help='Skip stages already successfully completed.')
  group.add_remote_option('--timeout', type='int', default=0,
                          help='Specify the maximum amount of time this job '
                               'can run for, at which point the build will be '
                               'aborted.  If set to zero, then there is no '
                               'timeout.')
  group.add_remote_option('--version', dest='force_version',
                          help='Used with manifest logic.  Forces use of this '
                               'version rather than create or get latest. '
                               'Examples: 4815.0.0-rc1, 4815.1.2')
  group.add_remote_option('--git-cache-dir', type='path',
                          api=constants.REEXEC_API_GIT_CACHE_DIR,
                          help='Specify the cache directory to store the '
                               'project caches populated by the git-cache '
                               'tool. Bootstrap the projects based on the git '
                               'cache files instead of fetching them directly '
                               'from the GoB servers.')
  group.add_remote_option('--sanity-check-build', action='store_true',
                          default=False, dest='sanity_check_build',
                          api=constants.REEXEC_API_SANITY_CHECK_BUILD,
                          help='Run the build as a sanity check build.')

  parser.add_argument_group(group)

  #
  # Internal options.
  #

  group = CustomGroup(
      parser,
      'Internal Chromium OS Build Team Options',
      'Caution: these are for meant for the Chromium OS build team only')

  group.add_remote_option('--archive-base', type='gs_path',
                          help='Base GS URL (gs://<bucket_name>/<path>) to '
                               'upload archive artifacts to')
  group.add_remote_option(
      '--cq-gerrit-query', dest='cq_gerrit_override',
      help='If given, this gerrit query will be used to find what patches to '
           "test, rather than the normal 'CommitQueue>=1 AND Verified=1 AND "
           "CodeReview=2' query it defaults to.  Use with care- note "
           'additionally this setting only has an effect if the buildbot '
           "target is a cq target, and we're in buildbot mode.")
  group.add_option('--pass-through', action='append', type='string',
                   dest='pass_through_args', default=[])
  group.add_option('--reexec-api-version', action='store_true',
                   dest='output_api_version', default=False,
                   help='Used for handling forwards/backwards compatibility '
                        'with --resume and --bootstrap')
  group.add_option('--remote-trybot', action='store_true', default=False,
                   help='Indicates this is running on a remote trybot machine')
  group.add_option('--buildbucket-id',
                   help='The unique ID in buildbucket of current build '
                        'generated by buildbucket.')
  group.add_remote_option('--remote-patches', action='split_extend', default=[],
                          help='Patches uploaded by the trybot client when '
                               'run using the -p option')
  # Note the default here needs to be hardcoded to 3; that is the last version
  # that lacked this functionality.
  group.add_option('--remote-version', type='int', default=3,
                   help='Used for compatibility checks w/tryjobs running in '
                        'older chromite instances')
  group.add_option('--sourceroot', type='path', default=constants.SOURCE_ROOT)
  group.add_option('--ts-mon-task-num', type='int', default=0,
                   api=constants.REEXEC_API_TSMON_TASK_NUM,
                   help='The task number of this process. Defaults to 0. '
                        'This argument is useful for running multiple copies '
                        'of cbuildbot without their metrics colliding.')
  group.add_remote_option('--test-bootstrap', action='store_true',
                          default=False,
                          help='Causes cbuildbot to bootstrap itself twice, '
                               'in the sequence A->B->C: A(unpatched) patches '
                               'and bootstraps B; B patches and bootstraps C')
  group.add_remote_option('--validation_pool',
                          help='Path to a pickled validation pool. Intended '
                               'for use only with the commit queue.')
  group.add_remote_option('--metadata_dump',
                          help='Path to a json dumped metadata file. This '
                               'will be used as the initial metadata.')
  group.add_remote_option('--master-build-id', type='int',
                          api=constants.REEXEC_API_MASTER_BUILD_ID,
                          help='cidb build id of the master build to this '
                               'slave build.')
  group.add_remote_option('--mock-tree-status',
                          help='Override the tree status value that would be '
                               'returned from the the actual tree. Example '
                               'values: open, closed, throttled. When used '
                               'in conjunction with --debug, the tree status '
                               'will not be ignored as it usually is in a '
                               '--debug run.')
  group.add_remote_option('--mock-slave-status',
                          metavar='MOCK_SLAVE_STATUS_PICKLE_FILE',
                          help='Override the result of the _FetchSlaveStatuses '
                               'method of MasterSlaveSyncCompletionStage, by '
                               'specifying a file with a pickle of the result '
                               'to be returned.')

  parser.add_argument_group(group)

  #
  # Debug options
  #
  # Temporary hack; in place till --dry-run replaces --debug.
  # pylint: disable=W0212
  group = parser.debug_group
  debug = [x for x in group.option_list if x._long_opts == ['--debug']][0]
  debug.help += '  Currently functions as --dry-run in addition.'
  debug.pass_through = True
  group.add_option('--notee', action='store_false', dest='tee', default=True,
                   help='Disable logging and internal tee process.  Primarily '
                        'used for debugging cbuildbot itself.')
  return parser


def _FinishParsing(options, args):
  """Perform some parsing tasks that need to take place after optparse.

  This function needs to be easily testable!  Keep it free of
  environment-dependent code.  Put more detailed usage validation in
  _PostParseCheck().

  Args:
    options: The options object returned by optparse
    args: The args object returned by optparse
  """
  # Populate options.pass_through_args.
  accepted, _ = commandline.FilteringParser.FilterArgs(
      options.parsed_args, lambda x: x.opt_inst.pass_through)
  options.pass_through_args.extend(accepted)

  if options.chrome_root:
    if options.chrome_rev != constants.CHROME_REV_LOCAL:
      cros_build_lib.Die('Chrome rev must be %s if chrome_root is set.' %
                         constants.CHROME_REV_LOCAL)
  elif options.chrome_rev == constants.CHROME_REV_LOCAL:
    cros_build_lib.Die('Chrome root must be set if chrome_rev is %s.' %
                       constants.CHROME_REV_LOCAL)

  if options.chrome_version:
    if options.chrome_rev != constants.CHROME_REV_SPEC:
      cros_build_lib.Die('Chrome rev must be %s if chrome_version is set.' %
                         constants.CHROME_REV_SPEC)
  elif options.chrome_rev == constants.CHROME_REV_SPEC:
    cros_build_lib.Die(
        'Chrome rev must not be %s if chrome_version is not set.'
        % constants.CHROME_REV_SPEC)

  patches = bool(options.gerrit_patches or options.local_patches or
                 options.rietveld_patches)
  if options.remote:
    if options.local:
      cros_build_lib.Die('Cannot specify both --remote and --local')

    # options.channels is a convenient way to detect payloads builds.
    if (not options.list and not options.buildbot and not options.channels and
        not patches):
      prompt = ('No patches were provided; are you sure you want to just '
                'run a remote build of %s?' % (
                    options.branch if options.branch else 'ToT'))
      if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
        cros_build_lib.Die('Must provide patches when running with --remote.')

    # --debug needs to be explicitly passed through for remote invocations.
    release_mode_with_patches = (options.buildbot and patches and
                                 '--debug' not in options.pass_through_args)
  else:
    if len(args) > 1:
      cros_build_lib.Die('Multiple configs not supported if not running with '
                         '--remote.  Got %r', args)

    if options.slaves:
      cros_build_lib.Die('Cannot use --slaves if not running with --remote.')

    release_mode_with_patches = (options.buildbot and patches and
                                 not options.debug)

  # When running in release mode, make sure we are running with checked-in code.
  # We want checked-in cbuildbot/scripts to prevent errors, and we want to build
  # a release image with checked-in code for CrOS packages.
  if release_mode_with_patches:
    cros_build_lib.Die(
        'Cannot provide patches when running with --buildbot!')

  if options.buildbot and options.remote_trybot:
    cros_build_lib.Die(
        '--buildbot and --remote-trybot cannot be used together.')

  # Record whether --debug was set explicitly vs. it was inferred.
  options.debug_forced = False
  if options.debug:
    options.debug_forced = True
  if not options.debug:
    # We don't set debug by default for
    # 1. --buildbot invocations.
    # 2. --remote invocations, because it needs to push changes to the tryjob
    #    repo.
    options.debug = not options.buildbot and not options.remote

  # Record the configs targeted.
  options.build_targets = args[:]

  if constants.BRANCH_UTIL_CONFIG in options.build_targets:
    if options.remote:
      cros_build_lib.Die(
          'Running %s as a remote tryjob is not yet supported.',
          constants.BRANCH_UTIL_CONFIG)
    if len(options.build_targets) > 1:
      cros_build_lib.Die(
          'Cannot run %s with any other configs.',
          constants.BRANCH_UTIL_CONFIG)
    if not options.branch_name:
      cros_build_lib.Die(
          'Must specify --branch-name with the %s config.',
          constants.BRANCH_UTIL_CONFIG)
    if options.branch and options.branch != options.branch_name:
      cros_build_lib.Die(
          'If --branch is specified with the %s config, it must'
          ' have the same value as --branch-name.',
          constants.BRANCH_UTIL_CONFIG)

    exclusive_opts = {'--version': options.force_version,
                      '--delete-branch': options.delete_branch,
                      '--rename-to': options.rename_to}
    if 1 != sum(1 for x in exclusive_opts.values() if x):
      cros_build_lib.Die('When using the %s config, you must'
                         ' specifiy one and only one of the following'
                         ' options: %s.', constants.BRANCH_UTIL_CONFIG,
                         ', '.join(exclusive_opts.keys()))

    # When deleting or renaming a branch, the --branch and --nobootstrap
    # options are implied.
    if options.delete_branch or options.rename_to:
      if not options.branch:
        logging.info('Automatically enabling sync to branch %s for this %s '
                     'flow.', options.branch_name,
                     constants.BRANCH_UTIL_CONFIG)
        options.branch = options.branch_name
      if options.bootstrap:
        logging.info('Automatically disabling bootstrap step for this %s flow.',
                     constants.BRANCH_UTIL_CONFIG)
        options.bootstrap = False

  elif any([options.delete_branch, options.rename_to, options.branch_name]):
    cros_build_lib.Die(
        'Cannot specify --delete-branch, --rename-to or --branch-name when not '
        'running the %s config', constants.BRANCH_UTIL_CONFIG)


# pylint: disable=W0613
def _PostParseCheck(parser, options, args, site_config):
  """Perform some usage validation after we've parsed the arguments

  Args:
    parser: Option parser that was used to parse arguments.
    options: The options returned by optparse.
    args: The args returned by optparse.
    site_config: config_lib.SiteConfig containing all config info.
  """
  if not args:
    parser.error('Invalid usage: no configuration targets provided.'
                 'Use -h to see usage.  Use -l to list supported configs.')

  if not options.branch:
    options.branch = git.GetChromiteTrackingBranch()

  if not repository.IsARepoRoot(options.sourceroot):
    if options.local_patches:
      raise Exception('Could not find repo checkout at %s!'
                      % options.sourceroot)

  # Because the default cache dir depends on other options, FindCacheDir
  # always returns None, and we setup the default here.
  if options.cache_dir is None:
    # Note, options.sourceroot is set regardless of the path
    # actually existing.
    if options.buildroot is not None:
      options.cache_dir = os.path.join(options.buildroot, '.cache')
    elif os.path.exists(options.sourceroot):
      options.cache_dir = os.path.join(options.sourceroot, '.cache')
    else:
      options.cache_dir = parser.FindCacheDir(parser, options)
    options.cache_dir = os.path.abspath(options.cache_dir)
    parser.ConfigureCacheDir(options.cache_dir)

  osutils.SafeMakedirsNonRoot(options.cache_dir)

  if options.local_patches:
    options.local_patches = _CheckLocalPatches(
        options.sourceroot, options.local_patches)

  default = os.environ.get('CBUILDBOT_DEFAULT_MODE')
  if (default and not any([options.local, options.buildbot,
                           options.remote, options.remote_trybot])):
    logging.info('CBUILDBOT_DEFAULT_MODE=%s env var detected, using it.'
                 % default)
    default = default.lower()
    if default == 'local':
      options.local = True
    elif default == 'remote':
      options.remote = True
    elif default == 'buildbot':
      options.buildbot = True
    else:
      cros_build_lib.Die("CBUILDBOT_DEFAULT_MODE value %s isn't supported. "
                         % default)

  # Ensure that all args are legitimate config targets.
  invalid_targets = []
  for arg in args:
    if arg not in site_config:
      invalid_targets.append(arg)
      logging.error('No such configuraton target: "%s".', arg)
      continue

    build_config = site_config[arg]

    is_payloads_build = build_config.build_type == constants.PAYLOADS_TYPE

    if options.channels and not is_payloads_build:
      cros_build_lib.Die('--channel must only be used with a payload config,'
                         ' not target (%s).' % arg)

    if not options.channels and is_payloads_build:
      cros_build_lib.Die('payload configs (%s) require --channel to do anything'
                         ' useful.' % arg)

    # The --version option is not compatible with an external target unless the
    # --buildbot option is specified.  More correctly, only "paladin versions"
    # will work with external targets, and those are only used with --buildbot.
    # If --buildbot is specified, then user should know what they are doing and
    # only specify a version that will work.  See crbug.com/311648.
    if (options.force_version and
        not (options.buildbot or build_config.internal)):
      cros_build_lib.Die('Cannot specify --version without --buildbot for an'
                         ' external target (%s).' % arg)

  if invalid_targets:
    cros_build_lib.Die('One or more invalid configuration targets specified. '
                       'You can check the available configs by running '
                       '`cbuildbot --list --all`')


def ParseCommandLine(parser, argv):
  """Completely parse the commandline arguments"""
  (options, args) = parser.parse_args(argv)

  # Strip out null arguments.
  # TODO(rcui): Remove when buildbot is fixed
  args = [arg for arg in args if arg]

  if options.deprecated_use_buildbucket:
    logging.warning('--use-buildbucket is deprecated, and ignored.')

  if options.output_api_version:
    print(constants.REEXEC_API_VERSION)
    sys.exit(0)

  _FinishParsing(options, args)
  return options, args


_ENVIRONMENT_PROD = 'prod'
_ENVIRONMENT_DEBUG = 'debug'
_ENVIRONMENT_STANDALONE = 'standalone'


def _GetRunEnvironment(options, build_config):
  """Determine whether this is a prod/debug/standalone run."""
  # TODO(akeshet): This is a temporary workaround to make sure that the cidb
  # is not used on waterfalls that the db schema does not support (in particular
  # the chromeos.chrome waterfall).
  # See crbug.com/406940
  wfall = os.environ.get('BUILDBOT_MASTERNAME', '')
  if not wfall in waterfall.CIDB_KNOWN_WATERFALLS:
    return _ENVIRONMENT_STANDALONE

  # TODO(akeshet): Clean up this code once we have better defined flags to
  # specify on-or-off waterfall and on-or-off production runs of cbuildbot.
  # See crbug.com/331417

  # --buildbot runs should use the production services, unless the --debug flag
  # is also present.
  if options.buildbot:
    if options.debug:
      return _ENVIRONMENT_DEBUG
    else:
      return _ENVIRONMENT_PROD

  # --remote-trybot runs should use the debug services, with the exception of
  # pre-cq builds, which should use the production services.
  if options.remote_trybot:
    if build_config['pre_cq']:
      return _ENVIRONMENT_PROD
    else:
      return _ENVIRONMENT_DEBUG

  # If neither --buildbot nor --remote-trybot flag was used, don't use external
  # services.
  return _ENVIRONMENT_STANDALONE


def _SetupConnections(options, build_config):
  """Set up CIDB connections using the appropriate Setup call.

  Args:
    options: Command line options structure.
    build_config: Config object for this build.
  """
  # Outline:
  # 1) Based on options and build_config, decide whether we are a production
  # run, debug run, or standalone run.
  # 2) Set up cidb instance accordingly.
  # 3) Update topology info from cidb, so that any other service set up can use
  # topology.
  # 4) Set up any other services.
  run_type = _GetRunEnvironment(options, build_config)

  if run_type == _ENVIRONMENT_PROD:
    cidb.CIDBConnectionFactory.SetupProdCidb()
    context = ts_mon_config.SetupTsMonGlobalState(
        'cbuildbot', indirect=True, task_num=options.ts_mon_task_num)
  elif run_type == _ENVIRONMENT_DEBUG:
    cidb.CIDBConnectionFactory.SetupDebugCidb()
    context = ts_mon_config.TrivialContextManager()
  else:
    cidb.CIDBConnectionFactory.SetupNoCidb()
    context = ts_mon_config.TrivialContextManager()

  db = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
  topology.FetchTopologyFromCIDB(db)

  return context


# TODO(build): This function is too damn long.
def main(argv):
  # Turn on strict sudo checks.
  cros_build_lib.STRICT_SUDO = True

  # Set umask to 022 so files created by buildbot are readable.
  os.umask(0o22)

  parser = _CreateParser()
  options, args = ParseCommandLine(parser, argv)

  if options.config_repo:
    cros_build_lib.Die('Deprecated usage. Ping crbug.com/735696 you need it.')

  # Fetch our site_config now, because we need it to do anything else.
  site_config = config_lib.GetConfig()

  if options.list:
    _PrintValidConfigs(site_config, options.print_all)
    sys.exit(0)

  _PostParseCheck(parser, options, args, site_config)

  cros_build_lib.AssertOutsideChroot()

  if options.enable_buildbot_tags:
    logging.EnableBuildbotMarkers()
  if options.remote:
    logging.getLogger().setLevel(logging.WARNING)

    # Verify configs are valid.
    # If hwtest flag is enabled, post a warning that HWTest step may fail if the
    # specified board is not a released platform or it is a generic overlay.
    for bot in args:
      build_config = site_config[bot]
      if options.hwtest:
        logging.warning(
            'If %s is not a released platform or it is a generic overlay, '
            'the HWTest step will most likely not run; please ask the lab '
            'team for help if this is unexpected.' % build_config['boards'])

    # Verify gerrit patches are valid.
    print('Verifying patches...')
    patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
        gerrit_patches=options.gerrit_patches,
        local_patches=options.local_patches,
        sourceroot=options.sourceroot,
        remote_patches=options.remote_patches)

    # --debug need to be explicitly passed through for remote invocations.
    if options.buildbot and '--debug' not in options.pass_through_args:
      _ConfirmRemoteBuildbotRun()

    print('Submitting tryjob...')
    with _SetupConnections(options, build_config):
      description = options.remote_description
      if description is None:
        description = remote_try.DefaultDescription(
            options.branch,
            options.gerrit_patches+options.local_patches)

      tryjob = remote_try.RemoteTryJob(args, patch_pool.local_patches,
                                       options.pass_through_args,
                                       options.cache_dir,
                                       description,
                                       options.committer_email,
                                       options.use_buildbucket,
                                       options.slaves)
      tryjob.Submit(testjob=options.test_tryjob, dryrun=False)
    print('Tryjob submitted!')
    print(('Go to %s to view the status of your job.'
           % tryjob.GetTrybotWaterfallLink()))
    sys.exit(0)

  elif (not options.buildbot and not options.remote_trybot
        and not options.resume and not options.local):
    cros_build_lib.Die('Please use --remote or --local to run trybots')

  elif options.buildbot and not options.debug:
    if not cros_build_lib.HostIsCIBuilder():
      # Cannot run --buildbot if both --debug and --remote aren't specified.
      cros_build_lib.Die('This host isn\'t a continuous-integration builder.')

  # Only one config arg is allowed in this mode, which was confirmed earlier.
  bot_id = args[-1]
  build_config = site_config[bot_id]

  # TODO: Re-enable this block when reference_repo support handles this
  #       properly. (see chromium:330775)
  # if options.reference_repo is None:
  #   repo_path = os.path.join(options.sourceroot, '.repo')
  #   # If we're being run from a repo checkout, reuse the repo's git pool to
  #   # cut down on sync time.
  #   if os.path.exists(repo_path):
  #     options.reference_repo = options.sourceroot

  if options.reference_repo:
    if not os.path.exists(options.reference_repo):
      parser.error('Reference path %s does not exist'
                   % (options.reference_repo,))
    elif not os.path.exists(os.path.join(options.reference_repo, '.repo')):
      parser.error('Reference path %s does not look to be the base of a '
                   'repo checkout; no .repo exists in the root.'
                   % (options.reference_repo,))

  if (options.buildbot or options.remote_trybot) and not options.resume:
    if not options.cgroups:
      parser.error('Options --buildbot/--remote-trybot and --nocgroups cannot '
                   'be used together.  Cgroup support is required for '
                   'buildbot/remote-trybot mode.')
    if not cgroups.Cgroup.IsSupported():
      parser.error('Option --buildbot/--remote-trybot was given, but this '
                   'system does not support cgroups.  Failing.')

    missing = osutils.FindMissingBinaries(_BUILDBOT_REQUIRED_BINARIES)
    if missing:
      parser.error('Option --buildbot/--remote-trybot requires the following '
                   "binaries which couldn't be found in $PATH: %s"
                   % (', '.join(missing)))

  if options.reference_repo:
    options.reference_repo = os.path.abspath(options.reference_repo)

  if not options.buildroot:
    if options.buildbot:
      parser.error('Please specify a buildroot with the --buildroot option.')

    options.buildroot = _DetermineDefaultBuildRoot(options.sourceroot,
                                                   build_config['internal'])
    # We use a marker file in the buildroot to indicate the user has
    # consented to using this directory.
    if not os.path.exists(repository.GetTrybotMarkerPath(options.buildroot)):
      _ConfirmBuildRoot(options.buildroot)

  # Sanity check of buildroot- specifically that it's not pointing into the
  # midst of an existing repo since git-repo doesn't support nesting.
  if (not repository.IsARepoRoot(options.buildroot) and
      git.FindRepoDir(options.buildroot)):
    parser.error('Configured buildroot %s points into a repository checkout, '
                 'rather than the root of it.  This is not supported.'
                 % options.buildroot)

  if not options.log_dir:
    options.log_dir = os.path.join(options.buildroot, _DEFAULT_LOG_DIR)

  log_file = None
  if options.tee:
    log_file = os.path.join(options.log_dir, _BUILDBOT_LOG_FILE)
    osutils.SafeMakedirs(options.log_dir)
    _BackupPreviousLog(log_file)

  with cros_build_lib.ContextManagerStack() as stack:
    options.preserve_paths = set()
    if log_file is not None:
      # We don't want the critical section to try to clean up the tee process,
      # so we run Tee (forked off) outside of it. This prevents a deadlock
      # because the Tee process only exits when its pipe is closed, and the
      # critical section accidentally holds on to that file handle.
      stack.Add(tee.Tee, log_file)
      options.preserve_paths.add(_DEFAULT_LOG_DIR)

    critical_section = stack.Add(cleanup.EnforcedCleanupSection)
    stack.Add(sudo.SudoKeepAlive)

    if not options.resume:
      # If we're in resume mode, use our parents tempdir rather than
      # nesting another layer.
      stack.Add(osutils.TempDir, prefix='cbuildbot-tmp', set_global=True)
      logging.debug('Cbuildbot tempdir is %r.', os.environ.get('TMP'))

    if options.cgroups:
      stack.Add(cgroups.SimpleContainChildren, 'cbuildbot')

    # Mark everything between EnforcedCleanupSection and here as having to
    # be rolled back via the contextmanager cleanup handlers.  This
    # ensures that sudo bits cannot outlive cbuildbot, that anything
    # cgroups would kill gets killed, etc.
    stack.Add(critical_section.ForkWatchdog)

    if not options.buildbot:
      build_config = config_lib.OverrideConfigForTrybot(
          build_config, options)

    if options.mock_tree_status is not None:
      stack.Add(mock.patch.object, tree_status, '_GetStatus',
                return_value=options.mock_tree_status)

    if options.mock_slave_status is not None:
      with open(options.mock_slave_status, 'r') as f:
        mock_statuses = pickle.load(f)
        for key, value in mock_statuses.iteritems():
          mock_statuses[key] = builder_status_lib.BuilderStatus(**value)
      stack.Add(mock.patch.object,
                completion_stages.MasterSlaveSyncCompletionStage,
                '_FetchSlaveStatuses',
                return_value=mock_statuses)

    stack.Add(_SetupConnections, options, build_config)
    retry_stats.SetupStats()

    timeout_display_message = None
    # For master-slave builds: Update slave's timeout using master's published
    # deadline.
    if options.buildbot and options.master_build_id is not None:
      slave_timeout = None
      if cidb.CIDBConnectionFactory.IsCIDBSetup():
        cidb_handle = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
        if cidb_handle:
          slave_timeout = cidb_handle.GetTimeToDeadline(options.master_build_id)

      if slave_timeout is not None:
        # We artificially set a minimum slave_timeout because '0' is handled
        # specially, and because we don't want to timeout while trying to set
        # things up.
        slave_timeout = max(slave_timeout, 20)
        if options.timeout == 0 or slave_timeout < options.timeout:
          logging.info('Updating slave build timeout to %d seconds enforced '
                       'by the master', slave_timeout)
          options.timeout = slave_timeout
          timeout_display_message = (
              'This build has reached the timeout deadline set by the master. '
              'Either this stage or a previous one took too long (see stage '
              'timing historical summary in ReportStage) or the build failed '
              'to start on time.')
      else:
        logging.warning('Could not get master deadline for master-slave build. '
                        'Can not set slave timeout.')

    if options.timeout > 0:
      stack.Add(timeout_util.FatalTimeout, options.timeout,
                timeout_display_message)
    try:
      _RunBuildStagesWrapper(options, site_config, build_config)
    except failures_lib.ExitEarlyException as ex:
      # This build finished successfully. Do not re-raise ExitEarlyException.
      logging.info('One stage exited early: %s', ex)
