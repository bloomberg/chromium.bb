# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for VM Management."""

from __future__ import print_function

import argparse
import distutils.version
import os
import re

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import cache
from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import remote_access
from chromite.lib import retry_util


class VMError(Exception):
  """Exception for VM failures."""

  def __init__(self, message):
    super(VMError, self).__init__()
    logging.error(message)


class VM(object):
  """Class for managing a VM."""

  SSH_PORT = 9222

  def __init__(self, argv):
    """Initialize VM.

    Args:
      argv: command line args.
    """
    opts = self._ParseArgs(argv)
    opts.Freeze()

    self.qemu_path = opts.qemu_path
    self.qemu_bios_path = opts.qemu_bios_path
    self.enable_kvm = opts.enable_kvm
    # We don't need sudo access for software emulation or if /dev/kvm is
    # writeable.
    self.use_sudo = self.enable_kvm and not os.access('/dev/kvm', os.W_OK)
    self.display = opts.display
    self.image_path = opts.image_path
    self.board = opts.board
    self.ssh_port = opts.ssh_port
    self.dry_run = opts.dry_run

    self.start = opts.start
    self.stop = opts.stop
    self.cmd = opts.args[1:] if opts.cmd else None

    self.vm_dir = os.path.join(osutils.GetGlobalTempDir(), 'cros_vm')
    if os.path.exists(self.vm_dir):
      # For security, ensure that vm_dir is not a symlink, and is owned by us or
      # by root.
      assert not os.path.islink(self.vm_dir), \
          'VM state dir is misconfigured; please recreate: %s' % self.vm_dir
      st_uid = os.stat(self.vm_dir).st_uid
      assert st_uid == 0 or st_uid == os.getuid(), \
          'VM state dir is misconfigured; please recreate: %s' % self.vm_dir

    self.pidfile = os.path.join(self.vm_dir, 'kvm.pid')
    self.kvm_monitor = os.path.join(self.vm_dir, 'kvm.monitor')
    self.kvm_pipe_in = '%s.in' % self.kvm_monitor  # to KVM
    self.kvm_pipe_out = '%s.out' % self.kvm_monitor  # from KVM
    self.kvm_serial = '%s.serial' % self.kvm_monitor

    self.remote = remote_access.RemoteDevice(remote_access.LOCALHOST,
                                             port=self.ssh_port)

    # TODO(achuith): support nographics, snapshot, mem_path, usb_passthrough,
    # moblab, etc.

  def _RunCommand(self, *args, **kwargs):
    """Use SudoRunCommand or RunCommand as necessary."""
    if self.use_sudo:
      return cros_build_lib.SudoRunCommand(*args, **kwargs)
    else:
      return cros_build_lib.RunCommand(*args, **kwargs)

  def _CleanupFiles(self, recreate):
    """Cleanup vm_dir.

    Args:
      recreate: recreate vm_dir.
    """
    osutils.RmDir(self.vm_dir, ignore_missing=True, sudo=self.use_sudo)
    if recreate:
      osutils.SafeMakedirs(self.vm_dir)

  @cros_build_lib.MemoizedSingleCall
  def QemuVersion(self):
    """Determine QEMU version."""
    version_str = self._RunCommand([self.qemu_path, '--version'],
                                   capture_output=True).output
    # version string looks like one of these:
    # QEMU emulator version 2.0.0 (Debian 2.0.0+dfsg-2ubuntu1.36), Copyright (c)
    # 2003-2008 Fabrice Bellard
    #
    # QEMU emulator version 2.6.0, Copyright (c) 2003-2008 Fabrice Bellard
    #
    # qemu-x86_64 version 2.10.1
    # Copyright (c) 2003-2017 Fabrice Bellard and the QEMU Project developers
    m = re.search(r"version ([0-9.]+)", version_str)
    if not m:
      raise VMError('Unable to determine QEMU version from:\n%s.' % version_str)
    return m.group(1)

  def _CheckQemuMinVersion(self):
    """Ensure minimum QEMU version."""
    min_qemu_version = '2.6.0'
    logging.info('QEMU version %s', self.QemuVersion())
    LooseVersion = distutils.version.LooseVersion
    if LooseVersion(self.QemuVersion()) < LooseVersion(min_qemu_version):
      raise VMError('QEMU %s is the minimum supported version. You have %s.'
                    % (min_qemu_version, self.QemuVersion()))

  def _SetQemuPath(self):
    """Find a suitable Qemu executable."""
    if not self.qemu_path:
      self.qemu_path = osutils.Which('qemu-system-x86_64')
    if not self.qemu_path:
      raise VMError('Qemu not found.')
    logging.debug('Qemu path: %s', self.qemu_path)
    self._CheckQemuMinVersion()

  def _GetBuiltVMImagePath(self):
    """Get path of a locally built VM image."""
    if not cros_build_lib.IsInsideChroot():
      return None
    return os.path.join(constants.SOURCE_ROOT, 'src/build/images',
                        cros_build_lib.GetBoard(self.board),
                        'latest', constants.VM_IMAGE_BIN)

  def _GetDownloadedVMImagePath(self):
    """Get path of a downloaded VM image."""
    tarball_cache = cache.TarballCache(os.path.join(
        path_util.GetCacheDir(),
        cros_chrome_sdk.COMMAND_NAME,
        cros_chrome_sdk.SDKFetcher.TARBALL_CACHE))
    lkgm = cros_chrome_sdk.SDKFetcher.GetChromeLKGM()
    if not lkgm:
      return None
    cache_key = (self.board, lkgm, constants.VM_IMAGE_TAR)
    with tarball_cache.Lookup(cache_key) as ref:
      if ref.Exists():
        return os.path.join(ref.path, constants.VM_IMAGE_BIN)
    return None

  def _SetVMImagePath(self):
    """Detect VM image path in SDK and chroot."""
    if not self.image_path:
      self.image_path = (self._GetDownloadedVMImagePath() or
                         self._GetBuiltVMImagePath())
    if not self.image_path:
      raise VMError('No VM image found. Use cros chrome-sdk --download-vm.')
    if not os.path.isfile(self.image_path):
      raise VMError('VM image does not exist: %s' % self.image_path)
    logging.debug('VM image path: %s', self.image_path)

  def Run(self):
    """Performs an action, one of start, stop, or run a command in the VM.

    Returns:
      cmd output.
    """

    if not self.start and not self.stop and not self.cmd:
      raise VMError('Must specify one of start, stop, or cmd.')
    if self.start:
      self.Start()
    if self.cmd:
      return self.RemoteCommand(self.cmd)
    if self.stop:
      self.Stop()

  def Start(self):
    """Start the VM."""

    self.Stop()

    logging.debug('Start VM')
    self._SetQemuPath()
    self._SetVMImagePath()

    self._CleanupFiles(recreate=True)
    # Make sure we can read these files later on by creating them as ourselves.
    osutils.Touch(self.kvm_serial)
    for pipe in [self.kvm_pipe_in, self.kvm_pipe_out]:
      os.mkfifo(pipe, 0600)
    osutils.Touch(self.pidfile)

    qemu_args = [self.qemu_path]
    if self.qemu_bios_path:
      if not os.path.isdir(self.qemu_bios_path):
        raise VMError('Invalid QEMU bios path: %s' % self.qemu_bios_path)
      qemu_args += ['-L', self.qemu_bios_path]

    qemu_args += [
        '-m', '2G', '-smp', '4', '-vga', 'virtio', '-daemonize',
        '-usbdevice', 'tablet',
        '-pidfile', self.pidfile,
        '-chardev', 'pipe,id=control_pipe,path=%s' % self.kvm_monitor,
        '-serial', 'file:%s' % self.kvm_serial,
        '-mon', 'chardev=control_pipe',
        # Qemu-vlans are used by qemu to separate out network traffic on the
        # slirp network bridge. qemu forwards traffic on a slirp vlan to all
        # ports conected on that vlan. By default, slirp ports are on vlan
        # 0. We explicitly set a vlan here so that another qemu VM using
        # slirp doesn't conflict with our network traffic.
        '-net', 'nic,model=virtio,vlan=%d' % self.ssh_port,
        '-net', 'user,hostfwd=tcp:127.0.0.1:%d-:22,vlan=%d'
        % (self.ssh_port, self.ssh_port),
        '-drive', 'file=%s,index=0,media=disk,cache=unsafe,format=raw'
        % self.image_path,
    ]
    if self.enable_kvm:
      qemu_args.append('-enable-kvm')
    if not self.display:
      qemu_args.extend(['-display', 'none'])
    logging.info(cros_build_lib.CmdToStr(qemu_args))
    logging.info('Pid file: %s', self.pidfile)
    if not self.dry_run:
      self._RunCommand(qemu_args)

  def _GetVMPid(self):
    """Get the pid of the VM.

    Returns:
      pid of the VM.
    """
    if not os.path.exists(self.vm_dir):
      logging.debug('%s not present.', self.vm_dir)
      return 0

    if not os.path.exists(self.pidfile):
      logging.info('%s does not exist.', self.pidfile)
      return 0

    pid = osutils.ReadFile(self.pidfile).rstrip()
    if not pid.isdigit():
      # Ignore blank/empty files.
      if pid:
        logging.error('%s in %s is not a pid.', pid, self.pidfile)
      return 0

    return int(pid)

  def IsRunning(self):
    """Returns True if there's a running VM.

    Returns:
      True if there's a running VM.
    """
    pid = self._GetVMPid()
    if not pid:
      return False

    # Make sure the process actually exists.
    return os.path.isdir('/proc/%i' % pid)

  def Stop(self):
    """Stop the VM."""
    logging.debug('Stop VM')

    pid = self._GetVMPid()
    if pid:
      logging.info('Killing %d.', pid)
      if not self.dry_run:
        self._RunCommand(['kill', '-9', str(pid)], error_code_ok=True)

    self._CleanupFiles(recreate=False)

  def _WaitForProcs(self):
    """Wait for expected processes to launch."""
    class _TooFewPidsException(Exception):
      """Exception for _GetRunningPids to throw."""

    def _GetRunningPids(exe, numpids):
      pids = self.remote.GetRunningPids(exe, full_path=False)
      logging.info('%s pids: %s', exe, repr(pids))
      if len(pids) < numpids:
        raise _TooFewPidsException()

    def _WaitForProc(exe, numpids):
      try:
        retry_util.RetryException(
            exception=_TooFewPidsException,
            max_retry=20,
            functor=lambda: _GetRunningPids(exe, numpids),
            sleep=2)
      except _TooFewPidsException:
        raise VMError('_WaitForProcs failed: timed out while waiting for '
                      '%d %s processes to start.' % (numpids, exe))

    # We could also wait for session_manager, nacl_helper, etc, but chrome is
    # the long pole. We expect the parent, 2 zygotes, gpu-process, renderer.
    # This could potentially break with Mustash.
    _WaitForProc('chrome', 5)

  def WaitForBoot(self):
    """Wait for the VM to boot up.

    Wait for ssh connection to become active, and wait for all expected chrome
    processes to be launched.
    """
    if not os.path.exists(self.vm_dir):
      self.Start()

    try:
      result = retry_util.RetryException(
          exception=remote_access.SSHConnectionError,
          max_retry=10,
          functor=lambda: self.RemoteCommand(cmd=['echo']),
          sleep=5)
    except remote_access.SSHConnectionError:
      raise VMError('WaitForBoot timed out trying to connect to VM.')

    if result.returncode != 0:
      raise VMError('WaitForBoot failed: %s.' % result.error)

    # Chrome can take a while to start with software emulation.
    if not self.enable_kvm:
      self._WaitForProcs()

  def RemoteCommand(self, cmd, **kwargs):
    """Run a remote command in the VM.

    Args:
      cmd: command to run.
      kwargs: additional args (see documentation for RemoteDevice.RunCommand).
    """
    if not self.dry_run:
      return self.remote.RunCommand(cmd, debug_level=logging.INFO,
                                    combine_stdout_stderr=True,
                                    log_output=True,
                                    error_code_ok=True,
                                    **kwargs)

  @staticmethod
  def _ParseArgs(argv):
    """Parse a list of args.

    Args:
      argv: list of command line arguments.

    Returns:
      List of parsed opts.
    """
    parser = commandline.ArgumentParser(description=__doc__)
    parser.add_argument('--start', action='store_true', default=False,
                        help='Start the VM.')
    parser.add_argument('--stop', action='store_true', default=False,
                        help='Stop the VM.')
    parser.add_argument('--image-path', type='path',
                        help='Path to VM image to launch with --start.')
    parser.add_argument('--qemu-path', type='path',
                        help='Path of qemu binary to launch with --start.')
    parser.add_argument('--qemu-bios-path', type='path',
                        help='Path of directory with qemu bios files.')
    parser.add_argument('--disable-kvm', dest='enable_kvm',
                        action='store_false', default=True,
                        help='Disable KVM, use software emulation.')
    parser.add_argument('--no-display', dest='display',
                        action='store_false', default=True,
                        help='Do not display video output.')
    parser.add_argument('--ssh-port', type=int, default=VM.SSH_PORT,
                        help='ssh port to communicate with VM.')
    sdk_board_env = os.environ.get(cros_chrome_sdk.SDKFetcher.SDK_BOARD_ENV)
    parser.add_argument('--board', default=sdk_board_env, help='Board to use.')
    parser.add_argument('--dry-run', action='store_true', default=False,
                        help='dry run for debugging.')
    parser.add_argument('--cmd', action='store_true', default=False,
                        help='Run a command in the VM.')
    parser.add_argument('args', nargs=argparse.REMAINDER,
                        help='Command to run in the VM.')
    return parser.parse_args(argv)

def main(argv):
  vm = VM(argv)
  vm.Run()
