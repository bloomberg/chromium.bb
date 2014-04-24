#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Script that deploys a Chrome build to a device.

The script supports deploying Chrome from these sources:

1. A local build output directory, such as chromium/src/out/[Debug|Release].
2. A Chrome tarball uploaded by a trybot/official-builder to GoogleStorage.
3. A Chrome tarball existing locally.

The script copies the necessary contents of the source location (tarball or
build directory) and rsyncs the contents of the staging directory onto your
device's rootfs.
"""

import collections
import contextlib
import functools
import glob
import logging
import multiprocessing
import os
import optparse
import shlex
import shutil
import tarfile
import time
import zipfile


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
from chromite.lib import stats
from chromite.lib import timeout_util
from chromite.scripts import lddtree


_USAGE = "deploy_chrome [--]\n\n %s" % __doc__

KERNEL_A_PARTITION = 2
KERNEL_B_PARTITION = 4

KILL_PROC_MAX_WAIT = 10
POST_KILL_WAIT = 2

MOUNT_RW_COMMAND = 'mount -o remount,rw /'
LSOF_COMMAND = 'lsof %s/chrome'

MOUNT_RW_COMMAND_ANDROID = 'mount -o remount,rw /system'

_ANDROID_DIR = '/system/chrome'
_ANDROID_DIR_EXTRACT_PATH = 'system/chrome/*'

_CHROME_DIR = '/opt/google/chrome'
_CHROME_DIR_MOUNT = '/mnt/stateful_partition/deploy_rootfs/opt/google/chrome'

_BIND_TO_FINAL_DIR_CMD = 'mount --rbind %s %s'
_SET_MOUNT_FLAGS_CMD = 'mount -o remount,exec,suid %s'

DF_COMMAND = 'df -k %s'
DF_COMMAND_ANDROID = 'df %s'

def _UrlBaseName(url):
  """Return the last component of the URL."""
  return url.rstrip('/').rpartition('/')[-1]


class DeployFailure(results_lib.StepFailure):
  """Raised whenever the deploy fails."""


DeviceInfo = collections.namedtuple(
    'DeviceInfo', ['target_dir_size', 'target_fs_free'])


class DeployChrome(object):
  """Wraps the core deployment functionality."""
  def __init__(self, options, tempdir, staging_dir):
    """Initialize the class.

    Args:
      options: Optparse result structure.
      tempdir: Scratch space for the class.  Caller has responsibility to clean
        it up.
      staging_dir: Directory to stage the files to.
    """
    self.tempdir = tempdir
    self.options = options
    self.staging_dir = staging_dir
    self.host = remote.RemoteAccess(options.to, tempdir, port=options.port)
    self._rootfs_is_still_readonly = multiprocessing.Event()

    # Used to track whether deploying content_shell or chrome to a device.
    self.content_shell = False
    self.copy_paths = chrome_util.GetCopyPaths(False)
    self.chrome_dir = _CHROME_DIR

  def _GetRemoteMountFree(self, remote_dir):
    result = self.host.RemoteSh((DF_COMMAND if not self.content_shell
                                 else DF_COMMAND_ANDROID) % remote_dir,
                                capture_output=True)
    line = result.output.splitlines()[1]
    value = line.split()[3]
    multipliers = {
        'G': 1024 * 1024 * 1024,
        'M': 1024 * 1024,
        'K': 1024,
    }
    return int(value.rstrip('GMK')) * multipliers.get(value[-1], 1)

  def _GetRemoteDirSize(self, remote_dir):
    if self.content_shell:
      # Content Shell devices currently do not contain the du binary.
      logging.warning('Remote host does not contain du; cannot get remote '
                      'directory size to properly calculate available free '
                      'space.')
      return 0
    result = self.host.RemoteSh('du -ks %s' % remote_dir, capture_output=True)
    return int(result.output.split()[0])

  def _GetStagingDirSize(self):
    result = cros_build_lib.DebugRunCommand(['du', '-ks', self.staging_dir],
                                            redirect_stdout=True,
                                            capture_output=True)
    return int(result.output.split()[0])

  def _ChromeFileInUse(self):
    result = self.host.RemoteSh(LSOF_COMMAND % (self.options.target_dir,),
                                error_code_ok=True, capture_output=True)
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
      result = self.host.RemoteSh('status ui', capture_output=True)
    except cros_build_lib.RunCommandError as e:
      if 'Unknown job' in e.result.error:
        return False
      else:
        raise e

    return result.output.split()[1].split('/')[0] == 'start'

  def _KillProcsIfNeeded(self):
    if self.content_shell:
      logging.info('Shutting down content_shell...')
      self.host.RemoteSh('stop content_shell')
      return

    if self._CheckUiJobStarted():
      logging.info('Shutting down Chrome...')
      self.host.RemoteSh('stop ui')

    # Developers sometimes run session_manager manually, in which case we'll
    # need to help shut the chrome processes down.
    try:
      with timeout_util.Timeout(KILL_PROC_MAX_WAIT):
        while self._ChromeFileInUse():
          logging.warning('The chrome binary on the device is in use.')
          logging.warning('Killing chrome and session_manager processes...\n')

          self.host.RemoteSh("pkill 'chrome|session_manager'",
                             error_code_ok=True)
          # Wait for processes to actually terminate
          time.sleep(POST_KILL_WAIT)
          logging.info('Rechecking the chrome binary...')
    except timeout_util.TimeoutError:
      msg = ('Could not kill processes after %s seconds.  Please exit any '
             'running chrome processes and try again.' % KILL_PROC_MAX_WAIT)
      raise DeployFailure(msg)

  def _MountRootfsAsWritable(self, error_code_ok=True):
    """Mount the rootfs as writable.

    If the command fails, and error_code_ok is True, then this function sets
    self._rootfs_is_still_readonly.

    Args:
      error_code_ok: See remote.RemoteAccess.RemoteSh for details.
    """
    # TODO: Should migrate to use the remount functions in remote_access.
    result = self.host.RemoteSh(MOUNT_RW_COMMAND if not self.content_shell
                                else MOUNT_RW_COMMAND_ANDROID,
                                error_code_ok=error_code_ok,
                                capture_output=True)
    if result.returncode:
      self._rootfs_is_still_readonly.set()

  def _GetDeviceInfo(self):
    steps = [
        functools.partial(self._GetRemoteDirSize, self.options.target_dir),
        functools.partial(self._GetRemoteMountFree, self.options.target_dir)
    ]
    return_values = parallel.RunParallelSteps(steps, return_values=True)
    return DeviceInfo(*return_values)

  def _CheckDeviceFreeSpace(self, device_info):
    """See if target device has enough space for Chrome.

    Args:
      device_info: A DeviceInfo named tuple.
    """
    effective_free = device_info.target_dir_size + device_info.target_fs_free
    staging_size = self._GetStagingDirSize()
    # For content shell deployments, which do not contain the du binary,
    # do not raise DeployFailure since can't get exact free space available
    # for staging files.
    if effective_free < staging_size:
      if self.content_shell:
        logging.warning('Not enough free space on the device.  If overwriting '
                        'files, deployment may still succeed.  Required: %s '
                        'MiB, actual: %s MiB.', staging_size / 1024,
                        effective_free / 1024)
      else:
        raise DeployFailure(
            'Not enough free space on the device.  Required: %s MiB, '
            'actual: %s MiB.' % (staging_size / 1024, effective_free / 1024))
    if device_info.target_fs_free < (100 * 1024):
      logging.warning('The device has less than 100MB free.  deploy_chrome may '
                      'hang during the transfer.')

  def _Deploy(self):
    logging.info('Copying Chrome to %s on device...', self.options.target_dir)
    # Show the output (status) for this command.
    dest_path = _CHROME_DIR
    if self.content_shell:
      try:
        self.host.Scp('%s/*' % os.path.abspath(self.staging_dir),
                      '%s/' % self.options.target_dir,
                      debug_level=logging.INFO,
                      verbose=self.options.verbose)
      except cros_build_lib.RunCommandError as ex:
        if ex.result.returncode != 1:
          logging.error('Scp failure [%s]', ex.result.returncode)
          raise DeployFailure(ex)
        else:
          # TODO(stevefung): Update Dropbear SSHD on device.
          # http://crbug.com/329656
          logging.info('Potential conflict with DropBear SSHD return status')

      dest_path = _ANDROID_DIR
    else:
      self.host.Rsync('%s/' % os.path.abspath(self.staging_dir),
                      self.options.target_dir,
                      inplace=True, debug_level=logging.INFO,
                      verbose=self.options.verbose)

    for p in self.copy_paths:
      if p.owner:
        self.host.RemoteSh('chown %s %s/%s' % (p.owner, dest_path,
                                               p.src if not p.dest else p.dest))
      if p.mode:
        # Set mode if necessary.
        self.host.RemoteSh('chmod %o %s/%s' % (p.mode, dest_path,
                                               p.src if not p.dest else p.dest))


    if self.options.startui:
      logging.info('Starting UI...')
      if self.content_shell:
        self.host.RemoteSh('start content_shell')
      else:
        self.host.RemoteSh('start ui')

  def _CheckConnection(self):
    try:
      logging.info('Testing connection to the device...')
      if self.content_shell:
        # true command over ssh returns error code 255, so as workaround
        #   use `sleep 0` as no-op.
        self.host.RemoteSh('sleep 0')
      else:
        self.host.RemoteSh('true')
    except cros_build_lib.RunCommandError as ex:
      logging.error('Error connecting to the test device.')
      raise DeployFailure(ex)

  def _CheckDeployType(self):
    if self.options.build_dir:
      if os.path.exists(os.path.join(self.options.build_dir, 'system.unand')):
        # Unand Content shell deployment.
        self.content_shell = True
        self.options.build_dir = os.path.join(self.options.build_dir,
                                              'system.unand/chrome/')
        self.options.dostrip = False
        self.options.target_dir = _ANDROID_DIR
        self.copy_paths = chrome_util.GetCopyPaths(True)
      elif os.path.exists(os.path.join(self.options.build_dir,
                                       'content_shell')) and not os.path.exists(
          os.path.join(self.options.build_dir, 'chrome')):
        # Content shell deployment
        self.content_shell = True
        self.copy_paths = chrome_util.GetCopyPaths(True)
    elif self.options.local_pkg_path or self.options.gs_path:
      # Package deployment.
      pkg_path = self.options.local_pkg_path
      if self.options.gs_path:
        pkg_path = _FetchChromePackage(self.options.cache_dir, self.tempdir,
                                       self.options.gs_path)

      assert pkg_path
      logging.info('Checking %s for content_shell...', pkg_path)
      if pkg_path[-4:] == '.zip':
        zip_pkg = zipfile.ZipFile(pkg_path)
        if any('eureka_shell' in name for name in zip_pkg.namelist()):
          self.content_shell = True
        zip_pkg.close()
      else:
        tar = tarfile.open(pkg_path)
        if any('eureka_shell' in member.name for member in tar.getmembers()):
          self.content_shell = True
        tar.close()

      if self.content_shell:
        self.options.dostrip = False
        self.options.target_dir = _ANDROID_DIR
        self.copy_paths = chrome_util.GetCopyPaths(True)
        self.chrome_dir = _ANDROID_DIR

  def _PrepareStagingDir(self):
    _PrepareStagingDir(self.options, self.tempdir, self.staging_dir,
                       self.copy_paths, self.chrome_dir)

  def _MountTarget(self):
    logging.info('Mounting Chrome...')

    # Create directory if does not exist
    self.host.RemoteSh('mkdir -p --mode 0775 %s' % (self.options.mount_dir,))
    self.host.RemoteSh(_BIND_TO_FINAL_DIR_CMD % (self.options.target_dir,
                                                 self.options.mount_dir))
    # Chrome needs partition to have exec and suid flags set
    self.host.RemoteSh(_SET_MOUNT_FLAGS_CMD % (self.options.mount_dir,))

  def Perform(self):
    self._CheckDeployType()

    # If requested, just do the staging step.
    if self.options.staging_only:
      self._PrepareStagingDir()
      return 0

    # Run setup steps in parallel. If any step fails, RunParallelSteps will
    # stop printing output at that point, and halt any running steps.
    steps = [self._GetDeviceInfo, self._CheckConnection,
             self._KillProcsIfNeeded, self._MountRootfsAsWritable,
             self._PrepareStagingDir]
    ret = parallel.RunParallelSteps(steps, halt_on_error=True,
                                    return_values=True)
    self._CheckDeviceFreeSpace(ret[0])

    # If we failed to mark the rootfs as writable, try disabling rootfs
    # verification.
    if self._rootfs_is_still_readonly.is_set():
      self._DisableRootfsVerification()

    if self.options.mount_dir is not None:
      self._MountTarget()

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
                    default=None)
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
  parser.add_option('--mount-dir', type='path', default=None,
                    help='Deploy Chrome in target directory and bind it'
                         'to directory specified by this flag.')
  parser.add_option('--mount', action='store_true', default=False,
                    help='Deploy Chrome to default target directory and bind it'
                         'to default mount directory.')

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

  group = optparse.OptionGroup(parser, 'Metadata Overrides (Advanced)',
                               description='Provide all of these overrides '
                               'in order to remove dependencies on '
                               'metadata.json existence.')
  group.add_option('--target-tc', action='store', default=None,
                   help='Override target toolchain name, e.g. '
                   'x86_64-cros-linux-gnu')
  group.add_option('--toolchain-url', action='store', default=None,
                   help='Override toolchain url format pattern, e.g. '
                   '2014/04/%%(target)s-2014.04.23.220740.tar.xz')
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

  if options.mount or options.mount_dir:
    if not options.target_dir:
      options.target_dir = _CHROME_DIR_MOUNT
  else:
    if not options.target_dir:
      options.target_dir = _CHROME_DIR

  if options.mount and not options.mount_dir:
    options.mount_dir = _CHROME_DIR

  return options, args


def _PostParseCheck(options, _args):
  """Perform some usage validation (after we've parsed the arguments).

  Args:
    options: The options object returned by optparse.
    _args: The args object returned by optparse.
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

  if options.build_dir:
    chrome_path = os.path.join(options.build_dir, 'chrome')
    if os.path.isfile(chrome_path):
      deps = lddtree.ParseELF(chrome_path)
      if 'libbase.so' in deps['libs']:
        cros_build_lib.Warning(
            'Detected a component build of Chrome.  component build is '
            'not working properly for Chrome OS.  See crbug.com/196317.  '
            'Use at your own risk!')


def _FetchChromePackage(cache_dir, tempdir, gs_path):
  """Get the chrome prebuilt tarball from GS.

  Returns:
    Path to the fetched chrome tarball.
  """
  gs_ctx = gs.GSContext(cache_dir=cache_dir, init_boto=True)
  files = gs_ctx.LS(gs_path)
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
    with sdk.Prepare(components=components, target_tc=options.target_tc,
                     toolchain_url=options.toolchain_url) as ctx:
      env_path = os.path.join(ctx.key_map[constants.CHROME_ENV_TAR].path,
                              constants.CHROME_ENV_FILE)
      strip_bin = osutils.SourceEnvironment(env_path, ['STRIP'])['STRIP']
      strip_bin = os.path.join(ctx.key_map[sdk.TARGET_TOOLCHAIN_KEY].path,
                               'bin', os.path.basename(strip_bin))
      yield strip_bin


def _PrepareStagingDir(options, tempdir, staging_dir, copy_paths=None,
                       chrome_dir=_CHROME_DIR):
  """Place the necessary files in the staging directory.

  The staging directory is the directory used to rsync the build artifacts over
  to the device.  Only the necessary Chrome build artifacts are put into the
  staging directory.
  """
  osutils.SafeMakedirs(staging_dir)
  os.chmod(staging_dir, 0o755)
  if options.build_dir:
    with _StripBinContext(options) as strip_bin:
      strip_flags = (None if options.strip_flags is None else
                     shlex.split(options.strip_flags))
      chrome_util.StageChromeFromBuildDir(
          staging_dir, options.build_dir, strip_bin, strict=options.strict,
          sloppy=options.sloppy, gyp_defines=options.gyp_defines,
          staging_flags=options.staging_flags,
          strip_flags=strip_flags, copy_paths=copy_paths)
  else:
    pkg_path = options.local_pkg_path
    if options.gs_path:
      pkg_path = _FetchChromePackage(options.cache_dir, tempdir,
                                     options.gs_path)

    assert pkg_path
    logging.info('Extracting %s...', pkg_path)
    # Extract only the ./opt/google/chrome contents, directly into the staging
    # dir, collapsing the directory hierarchy.
    if pkg_path[-4:] == '.zip':
      cros_build_lib.DebugRunCommand(
          ['unzip', '-X', pkg_path, _ANDROID_DIR_EXTRACT_PATH, '-d',
           staging_dir])
      for filename in glob.glob(os.path.join(staging_dir, 'system/chrome/*')):
        shutil.move(filename, staging_dir)
      osutils.RmDir(os.path.join(staging_dir, 'system'), ignore_missing=True)
    else:
      cros_build_lib.DebugRunCommand(
          ['tar', '--strip-components', '4', '--extract',
           '--preserve-permissions', '--file', pkg_path, '.%s' % chrome_dir],
          cwd=staging_dir)


def main(argv):
  options, args = _ParseCommandLine(argv)
  _PostParseCheck(options, args)

  # Set cros_build_lib debug level to hide RunCommand spew.
  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)
  else:
    logging.getLogger().setLevel(logging.INFO)

  with stats.UploadContext() as queue:
    cmd_stats = stats.Stats.SafeInit(cmd_line=argv, cmd_base='deploy_chrome')
    if cmd_stats:
      queue.put([cmd_stats, stats.StatsUploader.URL, 1])

    with osutils.TempDir(set_global=True) as tempdir:
      staging_dir = options.staging_dir
      if not staging_dir:
        staging_dir = os.path.join(tempdir, 'chrome')

      deploy = DeployChrome(options, tempdir, staging_dir)
      try:
        deploy.Perform()
      except results_lib.StepFailure as ex:
        raise SystemExit(str(ex).strip())
