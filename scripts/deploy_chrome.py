#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Script that deploys a Chrome build to a device.

The script supports deploying Chrome from these sources:

1. A local build output directory, such as chromium/src/out/[Debug|Release].
2. A Chrome tarball uploaded by a trybot/official-builder to GoogleStorage.
3. A Chrome tarball existing locally.

The script copies the necessary contents of the source location (tarball or
build directory) and rsyncs the contents of the staging directory onto your
device's rootfs.
"""

import contextlib
import logging
import multiprocessing
import os
import optparse
import shlex
import time


from chromite.buildbot import constants
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.cros.commands import cros_chrome_sdk
from chromite.lib import chrome_util
from chromite.lib import cros_build_lib
from chromite.lib import commandline
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import remote_access as remote


_USAGE = "deploy_chrome [--]\n\n %s" % __doc__

KERNEL_A_PARTITION = 2
KERNEL_B_PARTITION = 4

KILL_PROC_MAX_WAIT = 10
POST_KILL_WAIT = 2

MOUNT_RW_COMMAND = 'mount -o remount,rw /'
LSOF_COMMAND = 'lsof %s/chrome'

_CHROME_DIR = '/opt/google/chrome'


def _UrlBaseName(url):
  """Return the last component of the URL."""
  return url.rstrip('/').rpartition('/')[-1]


class DeployFailure(results_lib.StepFailure):
  """Raised whenever the deploy fails."""


class DeployChrome(object):
  """Wraps the core deployment functionality."""
  def __init__(self, options, tempdir, staging_dir):
    """Initialize the class.

    Arguments:
      options: Optparse result structure.
      tempdir: Scratch space for the class.  Caller has responsibility to clean
        it up.
    """
    self.tempdir = tempdir
    self.options = options
    self.staging_dir = staging_dir
    self.host = remote.RemoteAccess(options.to, tempdir, port=options.port)
    self._rootfs_is_still_readonly = multiprocessing.Event()

  def _ChromeFileInUse(self):
    result = self.host.RemoteSh(LSOF_COMMAND % (self.options.target_dir,),
                                error_code_ok=True)
    return result.returncode == 0

  def _DisableRootfsVerification(self):
    if not self.options.force:
      logging.error('Detected that the device has rootfs verification enabled.')
      logging.info('This script can automatically remove the rootfs '
                   'verification, which requires that it reboot the device.')
      logging.info('Make sure the device is in developer mode!')
      logging.info('Skip this prompt by specifying --force.')
      if not cros_build_lib.BooleanPrompt('Remove roots verification?', False):
        # Since we stopped Chrome earlier, it's good form to start it up again.
        if self.options.startui:
          logging.info('Starting Chrome...')
          self.host.RemoteSh('start ui')
        raise DeployFailure('Need rootfs verification to be disabled. '
                            'Aborting.')

    logging.info('Removing rootfs verification from %s', self.options.to)
    # Running in VM's cause make_dev_ssd's firmware sanity checks to fail.
    # Use --force to bypass the checks.
    cmd = ('/usr/share/vboot/bin/make_dev_ssd.sh --partitions %d '
           '--remove_rootfs_verification --force')
    for partition in (KERNEL_A_PARTITION, KERNEL_B_PARTITION):
      self.host.RemoteSh(cmd % partition, error_code_ok=True)

    # A reboot in developer mode takes a while (and has delays), so the user
    # will have time to read and act on the USB boot instructions below.
    logging.info('Please remember to press Ctrl-U if you are booting from USB.')
    self.host.RemoteReboot()

    # Now that the machine has been rebooted, we need to kill Chrome again.
    self._KillProcsIfNeeded()

    # Make sure the rootfs is writable now.
    self._MountRootfsAsWritable(error_code_ok=False)

  def _CheckUiJobStarted(self):
    # status output is in the format:
    # <job_name> <status> ['process' <pid>].
    # <status> is in the format <goal>/<state>.
    try:
      result = self.host.RemoteSh('status ui')
    except cros_build_lib.RunCommandError as e:
      if 'Unknown job' in e.result.error:
        return False
      else:
        raise e

    return result.output.split()[1].split('/')[0] == 'start'

  def _KillProcsIfNeeded(self):
    if self._CheckUiJobStarted():
      logging.info('Shutting down Chrome...')
      self.host.RemoteSh('stop ui')

    # Developers sometimes run session_manager manually, in which case we'll
    # need to help shut the chrome processes down.
    try:
      with cros_build_lib.SubCommandTimeout(KILL_PROC_MAX_WAIT):
        while self._ChromeFileInUse():
          logging.warning('The chrome binary on the device is in use.')
          logging.warning('Killing chrome and session_manager processes...\n')

          self.host.RemoteSh("pkill 'chrome|session_manager'",
                             error_code_ok=True)
          # Wait for processes to actually terminate
          time.sleep(POST_KILL_WAIT)
          logging.info('Rechecking the chrome binary...')
    except cros_build_lib.TimeoutError:
      msg = ('Could not kill processes after %s seconds.  Please exit any '
             'running chrome processes and try again.' % KILL_PROC_MAX_WAIT)
      raise DeployFailure(msg)

  def _MountRootfsAsWritable(self, error_code_ok=True):
    """Mount the rootfs as writable.

    If the command fails, and error_code_ok is True, then this function sets
    self._rootfs_is_still_readonly.

    Arguments:
      error_code_ok: See remote.RemoteAccess.RemoteSh for details.
    """
    result = self.host.RemoteSh(MOUNT_RW_COMMAND, error_code_ok=error_code_ok)
    if result.returncode:
      self._rootfs_is_still_readonly.set()

  def _Deploy(self):
    logging.info('Copying Chrome to %s on device...', self.options.target_dir)
    # Show the output (status) for this command.
    self.host.Rsync('%s/' % os.path.abspath(self.staging_dir),
                    self.options.target_dir,
                    inplace=True, debug_level=logging.INFO,
                    verbose=self.options.verbose)
    if self.options.startui:
      logging.info('Starting Chrome...')
      self.host.RemoteSh('start ui')

  def _CheckConnection(self):
    try:
      logging.info('Testing connection to the device...')
      self.host.RemoteSh('true')
    except cros_build_lib.RunCommandError as ex:
      logging.error('Error connecting to the test device.')
      raise DeployFailure(ex)

  def _PrepareStagingDir(self):
    _PrepareStagingDir(self.options, self.tempdir, self.staging_dir)

  def Perform(self):
    # If requested, just do the staging step.
    if self.options.staging_only:
      self._PrepareStagingDir()
      return 0

    # Run setup steps in parallel. If any step fails, RunParallelSteps will
    # stop printing output at that point, and halt any running steps.
    steps = [self._PrepareStagingDir, self._CheckConnection,
             self._KillProcsIfNeeded, self._MountRootfsAsWritable]
    parallel.RunParallelSteps(steps, halt_on_error=True)

    # If we failed to mark the rootfs as writable, try disabling rootfs
    # verification.
    if self._rootfs_is_still_readonly.is_set():
      self._DisableRootfsVerification()

    # Actually deploy Chrome to the device.
    self._Deploy()


def ValidateGypDefines(_option, _opt, value):
  """Convert GYP_DEFINES-formatted string to dictionary."""
  return chrome_util.ProcessGypDefines(value)


class CustomOption(commandline.Option):
  """Subclass Option class to implement path evaluation."""
  TYPES = commandline.Option.TYPES + ('gyp_defines',)
  TYPE_CHECKER = commandline.Option.TYPE_CHECKER.copy()
  TYPE_CHECKER['gyp_defines'] = ValidateGypDefines


def _CreateParser():
  """Create our custom parser."""
  parser = commandline.OptionParser(usage=_USAGE, option_class=CustomOption,
                                    caching=True)

  # TODO(rcui): Have this use the UI-V2 format of having source and target
  # device be specified as positional arguments.
  parser.add_option('--force', action='store_true', default=False,
                    help='Skip all prompts (i.e., for disabling of rootfs '
                         'verification).  This may result in the target '
                         'machine being rebooted.')
  sdk_board_env = os.environ.get(cros_chrome_sdk.SDKFetcher.SDK_BOARD_ENV)
  parser.add_option('--board', default=sdk_board_env,
                    help="The board the Chrome build is targeted for.  When in "
                         "a 'cros chrome-sdk' shell, defaults to the SDK "
                         "board.")
  parser.add_option('--build-dir', type='path',
                    help='The directory with Chrome build artifacts to deploy '
                         'from.  Typically of format <chrome_root>/out/Debug. '
                         'When this option is used, the GYP_DEFINES '
                         'environment variable must be set.')
  parser.add_option('--target-dir', type='path',
                    help='Target directory on device to deploy Chrome into.',
                    default=_CHROME_DIR)
  parser.add_option('-g', '--gs-path', type='gs_path',
                    help='GS path that contains the chrome to deploy.')
  parser.add_option('--nostartui', action='store_false', dest='startui',
                    default=True,
                    help="Don't restart the ui daemon after deployment.")
  parser.add_option('--nostrip', action='store_false', dest='dostrip',
                    default=True,
                    help="Don't strip binaries during deployment.  Warning: "
                         "the resulting binaries will be very large!")
  parser.add_option('-p', '--port', type=int, default=remote.DEFAULT_SSH_PORT,
                    help='Port of the target device to connect to.')
  parser.add_option('-t', '--to',
                    help='The IP address of the CrOS device to deploy to.')
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help='Show more debug output.')

  group = optparse.OptionGroup(parser, 'Advanced Options')
  group.add_option('-l', '--local-pkg-path', type='path',
                   help='Path to local chrome prebuilt package to deploy.')
  group.add_option('--sloppy', action='store_true', default=False,
                   help='Ignore when mandatory artifacts are missing.')
  group.add_option('--staging-flags', default=None, type='gyp_defines',
                   help='Extra flags to control staging.  Valid flags are - %s'
                        % ', '.join(chrome_util.STAGING_FLAGS))
  group.add_option('--strict', action='store_true', default=False,
                   help='Stage artifacts based on the GYP_DEFINES environment '
                        'variable and --staging-flags, if set. Enforce that '
                        'all optional artifacts are deployed.')
  group.add_option('--strip-flags', default=None,
                   help="Flags to call the 'strip' binutil tool with.  "
                        "Overrides the default arguments.")

  parser.add_option_group(group)

  # GYP_DEFINES that Chrome was built with.  Influences which files are staged
  # when --build-dir is set.  Defaults to reading from the GYP_DEFINES
  # enviroment variable.
  parser.add_option('--gyp-defines', default=None, type='gyp_defines',
                    help=optparse.SUPPRESS_HELP)
  # Path of an empty directory to stage chrome artifacts to.  Defaults to a
  # temporary directory that is removed when the script finishes. If the path
  # is specified, then it will not be removed.
  parser.add_option('--staging-dir', type='path', default=None,
                    help=optparse.SUPPRESS_HELP)
  # Only prepare the staging directory, and skip deploying to the device.
  parser.add_option('--staging-only', action='store_true', default=False,
                    help=optparse.SUPPRESS_HELP)
  # Path to a binutil 'strip' tool to strip binaries with.  The passed-in path
  # is used as-is, and not normalized.  Used by the Chrome ebuild to skip
  # fetching the SDK toolchain.
  parser.add_option('--strip-bin', default=None, help=optparse.SUPPRESS_HELP)
  return parser


def _ParseCommandLine(argv):
  """Parse args, and run environment-independent checks."""
  parser = _CreateParser()
  (options, args) = parser.parse_args(argv)

  if not any([options.gs_path, options.local_pkg_path, options.build_dir]):
    parser.error('Need to specify either --gs-path, --local-pkg-path, or '
                 '--build-dir')
  if options.build_dir and any([options.gs_path, options.local_pkg_path]):
    parser.error('Cannot specify both --build_dir and '
                 '--gs-path/--local-pkg-patch')
  if options.build_dir and not options.board:
    parser.error('--board is required when --build-dir is specified.')
  if options.gs_path and options.local_pkg_path:
    parser.error('Cannot specify both --gs-path and --local-pkg-path')
  if not (options.staging_only or options.to):
    parser.error('Need to specify --to')
  if (options.strict or options.staging_flags) and not options.build_dir:
    parser.error('--strict and --staging-flags require --build-dir to be '
                 'set.')
  if options.staging_flags and not options.strict:
    parser.error('--staging-flags requires --strict to be set.')
  if options.sloppy and options.strict:
    parser.error('Cannot specify both --strict and --sloppy.')
  return options, args


def _PostParseCheck(options, _args):
  """Perform some usage validation (after we've parsed the arguments

  Args:
    options/args: The options/args object returned by optparse
  """
  if options.local_pkg_path and not os.path.isfile(options.local_pkg_path):
    cros_build_lib.Die('%s is not a file.', options.local_pkg_path)

  if not options.gyp_defines:
    gyp_env = os.getenv('GYP_DEFINES', None)
    if gyp_env is not None:
      options.gyp_defines = chrome_util.ProcessGypDefines(gyp_env)
      logging.debug('GYP_DEFINES taken from environment: %s',
                   options.gyp_defines)

  if options.strict and not options.gyp_defines:
    cros_build_lib.Die('When --strict is set, the GYP_DEFINES environment '
                         'variable must be set.')

  if (options.gyp_defines and
      options.gyp_defines.get('component') == 'shared_library'):
    cros_build_lib.Warning(
        "Detected 'component=shared_library' in GYP_DEFINES. "
        "deploy_chrome currently doesn't work well with component build. "
        "See crosbug.com/196317.  Use at your own risk!")


def _FetchChromePackage(cache_dir, tempdir, gs_path):
  """Get the chrome prebuilt tarball from GS.

  Returns: Path to the fetched chrome tarball.
  """
  gs_ctx = gs.GSContext.Cached(cache_dir, init_boto=True)
  files = gs_ctx.LS(gs_path).output.splitlines()
  files = [found for found in files if
           _UrlBaseName(found).startswith('%s-' % constants.CHROME_PN)]
  if not files:
    raise Exception('No chrome package found at %s' % gs_path)
  elif len(files) > 1:
    # - Users should provide us with a direct link to either a stripped or
    #   unstripped chrome package.
    # - In the case of being provided with an archive directory, where both
    #   stripped and unstripped chrome available, use the stripped chrome
    #   package.
    # - Stripped chrome pkg is chromeos-chrome-<version>.tar.gz
    # - Unstripped chrome pkg is chromeos-chrome-<version>-unstripped.tar.gz.
    files = [f for f in files if not 'unstripped' in f]
    assert len(files) == 1
    logging.warning('Multiple chrome packages found.  Using %s', files[0])

  filename = _UrlBaseName(files[0])
  logging.info('Fetching %s...', filename)
  gs_ctx.Copy(files[0], tempdir, print_cmd=False)
  chrome_path = os.path.join(tempdir, filename)
  assert os.path.exists(chrome_path)
  return chrome_path


@contextlib.contextmanager
def _StripBinContext(options):
  if not options.dostrip:
    yield None
  elif options.strip_bin:
    yield options.strip_bin
  else:
    sdk = cros_chrome_sdk.SDKFetcher(options.cache_dir, options.board)
    components = (sdk.TARGET_TOOLCHAIN_KEY, constants.CHROME_ENV_TAR)
    with sdk.Prepare(components=components) as ctx:
      env_path = os.path.join(ctx.key_map[constants.CHROME_ENV_TAR].path,
                              constants.CHROME_ENV_FILE)
      strip_bin = osutils.SourceEnvironment(env_path, ['STRIP'])['STRIP']
      strip_bin = os.path.join(ctx.key_map[sdk.TARGET_TOOLCHAIN_KEY].path,
                               'bin', os.path.basename(strip_bin))
      yield strip_bin


def _PrepareStagingDir(options, tempdir, staging_dir):
  """Place the necessary files in the staging directory.

  The staging directory is the directory used to rsync the build artifacts over
  to the device.  Only the necessary Chrome build artifacts are put into the
  staging directory.
  """
  osutils.SafeMakedirs(staging_dir)
  os.chmod(staging_dir, 0755)
  if options.build_dir:
    with _StripBinContext(options) as strip_bin:
      strip_flags = (None if options.strip_flags is None else
                     shlex.split(options.strip_flags))
      chrome_util.StageChromeFromBuildDir(
          staging_dir, options.build_dir, strip_bin, strict=options.strict,
          sloppy=options.sloppy, gyp_defines=options.gyp_defines,
          staging_flags=options.staging_flags,
          strip_flags=strip_flags)
  else:
    pkg_path = options.local_pkg_path
    if options.gs_path:
      pkg_path = _FetchChromePackage(options.cache_dir, tempdir,
                                     options.gs_path)

    assert pkg_path
    logging.info('Extracting %s...', pkg_path)
    # Extract only the ./opt/google/chrome contents, directly into the staging
    # dir, collapsing the directory hierarchy.
    cros_build_lib.DebugRunCommand(
        ['tar', '--strip-components', '4', '--extract',
         '--preserve-permissions', '--file', pkg_path, '.%s' % _CHROME_DIR],
        cwd=staging_dir)

def main(argv):
  options, args = _ParseCommandLine(argv)
  _PostParseCheck(options, args)

  # Set cros_build_lib debug level to hide RunCommand spew.
  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)
  else:
    logging.getLogger().setLevel(logging.INFO)

  with osutils.TempDir(set_global=True) as tempdir:
    staging_dir = options.staging_dir
    if not staging_dir:
      staging_dir = os.path.join(tempdir, 'chrome')

    deploy = DeployChrome(options, tempdir, staging_dir)
    try:
      deploy.Perform()
    except results_lib.StepFailure as ex:
      raise SystemExit(str(ex).strip())
