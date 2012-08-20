#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that resets your Chrome GIT checkout."""

import functools
import logging
import optparse
import os
import time
import urlparse

from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import remote_access as remote
from chromite.lib import sudo


GS_HTTP = 'https://commondatastorage.googleapis.com'
GSUTIL_URL = '%s/chromeos-public/gsutil.tar.gz' % GS_HTTP
GS_RETRIES = 5
KERNEL_A_PARTITION = 2
KERNEL_B_PARTITION = 4

KILL_PROC_MAX_WAIT = 10
POST_KILL_WAIT = 2


# Convenience RunCommand methods
DebugRunCommand = functools.partial(
    cros_build_lib.RunCommand, debug_level=logging.DEBUG)

DebugRunCommandCaptureOutput = functools.partial(
    cros_build_lib.RunCommandCaptureOutput, debug_level=logging.DEBUG)

DebugSudoRunCommand = functools.partial(
    cros_build_lib.SudoRunCommand, debug_level=logging.DEBUG)


def _TestGSLs(gs_bin):
  """Quick test of gsutil functionality."""
  result = DebugRunCommandCaptureOutput([gs_bin, 'ls'], error_code_ok=True)
  return not result.returncode


def _SetupBotoConfig(gs_bin):
  """Make sure we can access protected bits in GS."""
  boto_path = os.path.expanduser('~/.boto')
  if os.path.isfile(boto_path) or _TestGSLs(gs_bin):
    return

  logging.info('Configuring gsutil. Please use your @google.com account.')
  try:
    cros_build_lib.RunCommand([gs_bin, 'config'], print_cmd=False)
  finally:
    if os.path.exists(boto_path) and not os.path.getsize(boto_path):
      os.remove(boto_path)


def _UrlBaseName(url):
  """Return the last component of the URL."""
  return url.rstrip('/').rpartition('/')[-1]


def _ExtractChrome(src, dest):
  osutils.SafeMakedirs(dest)
  # Preserve permissions (-p).  This is default when running tar with 'sudo'.
  DebugSudoRunCommand(['tar', '--checkpoint', '-xf', src],
                        cwd=dest)


class DeployChrome(object):
  """Wraps the core deployment functionality."""
  def __init__(self, options, tempdir):
    self.tempdir = tempdir
    self.options = options
    self.chrome_dir = os.path.join(tempdir, 'chrome')
    self.host = remote.RemoteAccess(options.to, tempdir, port=options.port)
    self.start_ui_needed = False

  def _FetchChrome(self):
    """Get the chrome prebuilt tarball from GS.

    Returns: Path to the fetched chrome tarball.
    """
    logging.info('Fetching gsutil.')
    gsutil_tar = os.path.join(self.tempdir, 'gsutil.tar.gz')
    cros_build_lib.RunCurl([GSUTIL_URL, '-o', gsutil_tar],
                           debug_level=logging.DEBUG)
    DebugRunCommand(['tar', '-xzf', gsutil_tar], cwd=self.tempdir)
    gs_bin = os.path.join(self.tempdir, 'gsutil', 'gsutil')
    _SetupBotoConfig(gs_bin)
    cmd = [gs_bin, 'ls', self.options.gs_path]
    files = DebugRunCommandCaptureOutput(cmd).output.splitlines()
    files = [found for found in files if
             _UrlBaseName(found).startswith('chromeos-chrome-')]
    if not files:
      raise Exception('No chrome package found at %s' % self.options.gs_path)
    elif len(files) > 1:
      # - Users should provide us with a direct link to either a stripped or
      #   unstripped chrome package.
      # - In the case of being provided with an archive directory, where both
      #   stripped and unstripped chrome available, use the stripped chrome
      #   package (comes on top after sort).
      # - Stripped chrome pkg is chromeos-chrome-<version>.tar.gz
      # - Unstripped chrome pkg is chromeos-chrome-<version>-unstripped.tar.gz.
      files.sort()
      cros_build_lib.logger.warning('Multiple chrome packages found.  Using %s',
                                    files[0])

    filename = _UrlBaseName(files[0])
    logging.info('Fetching %s.', filename)
    cros_build_lib.RunCommand([gs_bin, 'cp', files[0], self.tempdir],
                              print_cmd=False)
    chrome_path = os.path.join(self.tempdir, filename)
    assert os.path.exists(chrome_path)
    return chrome_path

  def _ChromeFileInUse(self):
    result = self.host.RemoteSh('lsof /opt/google/chrome/chrome',
                                error_code_ok=True)
    return result.returncode == 0

  def _DisableRootfsVerification(self):
    if not self.options.force:
      logging.error('Detected that the device has rootfs verification enabled.')
      logging.info('This script can automatically remove the rootfs '
                   'verification, which requires that it reboot the device.')
      logging.info('Make sure the device is in developer mode!')
      logging.info('Skip this prompt by specifying --force.')
      result = cros_build_lib.YesNoPrompt(
          'no', prompt='Remove roots verification?')
      if result == 'no':
        cros_build_lib.Die('Need rootfs verification to be disabled. '
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

  def _CheckRootfsWriteable(self):
    # /proc/mounts is in the format:
    # <device> <dir> <type> <options>
    result = self.host.RemoteSh('cat /proc/mounts')
    for line in result.output.splitlines():
      components = line.split()
      if components[0] == '/dev/root' and components[1] == '/':
        return 'rw' in components[3].split(',')
    else:
      raise Exception('Internal error - rootfs mount not found!')

  def _CheckUiJobStarted(self):
    # status output is in the format:
    # <job_name> <status> ['process' <pid>].
    # <status> is in the format <goal>/<state>.
    result = self.host.RemoteSh('status ui')
    return result.output.split()[1].split('/')[0] == 'start'

  def _KillProcsIfNeeded(self):
    if self._CheckUiJobStarted():
      logging.info('Shutting down Chrome.')
      self.start_ui_needed = True
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
      cros_build_lib.Die('Could not kill processes after %s seconds.  Please '
                         'exit any running chrome processes and try again.')

  def _PrepareTarget(self):
    # Mount root partition as read/write
    if not self._CheckRootfsWriteable():
      logging.info('Mounting rootfs as writeable...')
      result = self.host.RemoteSh('mount -o remount,rw /', error_code_ok=True)
      if result.returncode:
        self._DisableRootfsVerification()
        logging.info('Trying again to mount rootfs as writeable...')
        self.host.RemoteSh('mount -o remount,rw /')

      if not self._CheckRootfsWriteable():
        cros_build_lib.Die('Root partition still read-only')

    # This is needed because we're doing an 'rsync --inplace' of Chrome, but
    # makes sense to have even when going the sshfs route.
    self._KillProcsIfNeeded()

  def _Deploy(self):
    logging.info('Copying Chrome to device.')
    # Show the output (status) for this command.
    self.host.Rsync('%s/' % os.path.abspath(self.chrome_dir), '/', inplace=True,
                    debug_level=logging.INFO)
    if self.start_ui_needed:
      self.host.RemoteSh('start ui')

  def Perform(self):
    try:
      logging.info('Testing connection to the device.')
      self.host.RemoteSh('true')
    except cros_build_lib.RunCommandError:
      logging.error('Error connecting to the test device.')
      raise

    pkg_path = self.options.local_path
    if self.options.gs_path:
      pkg_path = self._FetchChrome()

    logging.info('Extracting %s.', pkg_path)
    _ExtractChrome(pkg_path, self.chrome_dir)

    self._PrepareTarget()
    self._Deploy()


def check_gs_path(option, opt, value):
  """Convert passed-in path to gs:// path."""
  parsed = urlparse.urlparse(value.rstrip('/ '))
  # pylint: disable=E1101
  path = parsed.path.lstrip('/')
  if parsed.hostname.startswith('sandbox.google.com'):
    # Sandbox paths are 'storage/<bucket>/<path_to_object>', so strip out the
    # first component.
    storage, _, path = path.partition('/')
    assert storage == 'storage', 'GS URL %s not in expected format.' % value

  return 'gs://%s' % path


def check_path(option, opt, value):
  """Expand the local path"""
  return osutils.ExpandPath(value)


class CustomOption(optparse.Option):
  """Subclass Option class to implement path evaluation."""
  TYPES = optparse.Option.TYPES + ('path', 'gs_path')
  TYPE_CHECKER = optparse.Option.TYPE_CHECKER.copy()
  TYPE_CHECKER['path'] = check_path
  TYPE_CHECKER['gs_path'] = check_gs_path


def _ParseCommandLine(argv):
  """Create the parser, parse args, and run environment-independent checks."""
  usage = 'usage: %prog [--] [command]'
  parser = optparse.OptionParser(usage=usage, option_class=CustomOption)

  parser.add_option('--force', action='store_true', default=False,
                    help=('Skip all prompts (i.e., for disabling of rootfs '
                          'verification).  This may result in the target '
                          'machine being rebooted.'))
  parser.add_option('-g', '--gs-path', type='gs_path',
                    help=('GS path that contains the chrome to deploy.'))
  parser.add_option('-l', '--local-path', type='path',
                    help='path to local chrome prebuilt package to deploy.')
  parser.add_option('-p', '--port', type=int, default=remote.DEFAULT_SSH_PORT,
                    help=('Port of the target device to connect to.'))
  parser.add_option('-t', '--to',
                    help=('The IP address of the CrOS device to deploy to.'))
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help=('Show more debug output.'))

  (options, args) = parser.parse_args(argv)

  if not options.gs_path and not options.local_path:
    parser.error('Need to specify either --gs-path or --local-path')
  if options.gs_path and options.local_path:
    parser.error('Cannot specify both --gs-path and --local-path')
  if not options.to:
    parser.error('Need to specify --to')

  return options, args


def _PostParseCheck(options, args):
  """Perform some usage validation (after we've parsed the arguments

  Args:
    options/args: The options/args object returned by optparse
  """
  if options.local_path and not os.path.isfile(options.local_path):
    cros_build_lib.Die('%s is not a file.', options.local_path)


def main(argv):
  options, args = _ParseCommandLine(argv)
  _PostParseCheck(options, args)

  # Set cros_build_lib debug level to hide RunCommand spew.
  if options.verbose:
    cros_build_lib.logger.setLevel(logging.DEBUG)
  else:
    cros_build_lib.logger.setLevel(logging.INFO)

  with sudo.SudoKeepAlive(ttyless_sudo=False):
    with osutils.TempDirContextManager(sudo_rm=True) as tempdir:
      deploy = DeployChrome(options, tempdir)
      deploy.Perform()
