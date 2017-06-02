# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for VM Management."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
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

  def __init__(self, image_path=None, qemu_path=None, enable_kvm=True,
               display=True, ssh_port=SSH_PORT, dry_run=False):
    """Initialize VM.

    Args:
      image_path: path of vm image.
      qemu_path: path to qemu binary.
      enable_kvm: enable kvm (kernel support for virtualization).
      display: display video output.
      ssh_port: ssh port to use.
      dry_run: disable VM commands.
    """

    self.qemu_path = qemu_path
    self.enable_kvm = enable_kvm
    # Software emulation doesn't need sudo access.
    self.use_sudo = enable_kvm
    self.display = display
    self.image_path = image_path
    self.ssh_port = ssh_port
    self.dry_run = dry_run

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
                                             port=ssh_port)

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
    self._RunCommand(['rm', '-rf', self.vm_dir])
    if recreate:
      self._RunCommand(['mkdir', self.vm_dir])
      self._RunCommand(['chmod', '777', self.vm_dir])

  def PerformAction(self, start=False, stop=False, cmd=None):
    """Performs an action, one of start, stop, or run a command in the VM.

    Args:
      start: start the VM.
      stop: stop the VM.
      cmd: list or scalar command to run in the VM.

    Returns:
      cmd output.
    """

    if not start and not stop and not cmd:
      raise VMError('Must specify one of start, stop, or cmd.')
    if start:
      self.Start()
    if stop:
      self.Stop()
    if cmd:
      return self.RemoteCommand(cmd.split())

  def Start(self):
    """Start the VM."""

    self.Stop()

    logging.debug('Start VM')
    if not self.qemu_path:
      self.qemu_path = osutils.Which('qemu-system-x86_64')
    if not self.qemu_path:
      raise VMError('qemu not found.')
    logging.debug('qemu path=%s', self.qemu_path)

    if not self.image_path:
      self.image_path = os.environ.get('VM_IMAGE_PATH', '')
    logging.debug('vm image path=%s', self.image_path)
    if not self.image_path or not os.path.exists(self.image_path):
      raise VMError('VM image path %s does not exist.' % self.image_path)

    self._CleanupFiles(recreate=True)
    open(self.kvm_serial, 'w')
    for pipe in [self.kvm_pipe_in, self.kvm_pipe_out]:
      os.mkfifo(pipe, 0600)

    args = [self.qemu_path, '-m', '2G', '-smp', '4', '-vga', 'cirrus',
            '-daemonize',
            '-pidfile', self.pidfile,
            '-chardev', 'pipe,id=control_pipe,path=%s' % self.kvm_monitor,
            '-serial', 'file:%s' % self.kvm_serial,
            '-mon', 'chardev=control_pipe',
            '-net', 'nic,model=virtio',
            '-net', 'user,hostfwd=tcp::%d-:22' % self.ssh_port,
            '-drive', 'file=%s,index=0,media=disk,cache=unsafe'
            % self.image_path]
    if self.enable_kvm:
      args.append('-enable-kvm')
    if not self.display:
      args.extend(['-display', 'none'])
    logging.info(' '.join(args))
    logging.info('Pid file: %s', self.pidfile)
    if not self.dry_run:
      self._RunCommand(args)

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

    pid = self._RunCommand(['cat', self.pidfile],
                           redirect_stdout=True).output.rstrip()
    if not pid.isdigit():
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
    res = self._RunCommand(['kill', '-0', str(pid)], error_code_ok=True)
    return res.returncode == 0

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
            exc_retry=_TooFewPidsException,
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
          exc_retry=remote_access.SSHConnectionError,
          max_retry=10,
          functor=lambda: self.RemoteCommand(cmd=['echo']),
          sleep=5)
    except remote_access.SSHConnectionError:
      raise VMError('WaitForBoot timed out trying to connect to VM.')

    if result.returncode != 0:
      raise VMError('WaitForBoot failed: %s.' % result.error)

    self._WaitForProcs()

  def RemoteCommand(self, cmd):
    """Run a remote command in the VM.

    Args:
      cmd: command to run, of list type.
    """
    if not isinstance(cmd, list):
      raise VMError('cmd must be a list.')

    if not self.dry_run:
      return self.remote.RunCommand(cmd, debug_level=logging.INFO,
                                    combine_stdout_stderr=True,
                                    log_output=True,
                                    error_code_ok=True)

def ParseCommandLine(argv):
  """Parse the command line.

  Args:
    argv: Command arguments.

  Returns:
    List of parsed args.
  """
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--start', action='store_true', default=False,
                      help='Start the VM.')
  parser.add_argument('--stop', action='store_true', default=False,
                      help='Stop the VM.')
  parser.add_argument('--cmd', help='Run this command in the VM.')
  parser.add_argument('--image-path', type='path',
                      help='Path to VM image to launch with --start.')
  parser.add_argument('--qemu-path', type='path',
                      help='Path of qemu binary to launch with --start.')
  parser.add_argument('--disable-kvm', dest='enable_kvm',
                      action='store_false', default=True,
                      help='Disable KVM, use software emulation.')
  parser.add_argument('--no-display', dest='display',
                      action='store_false', default=True,
                      help='Do not display video output.')
  parser.add_argument('--ssh-port', type=int, default=VM.SSH_PORT,
                      help='ssh port to communicate with VM.')
  parser.add_argument('--dry-run', action='store_true', default=False,
                      help='dry run for debugging.')
  return parser.parse_args(argv)


def main(argv):
  args = ParseCommandLine(argv)
  vm = VM(image_path=args.image_path, qemu_path=args.qemu_path,
          enable_kvm=args.enable_kvm, display=args.display,
          ssh_port=args.ssh_port, dry_run=args.dry_run)
  vm.PerformAction(start=args.start, stop=args.stop, cmd=args.cmd)
