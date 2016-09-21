# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for VM Management."""

from __future__ import print_function

import os
import time

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access


class VMError(Exception):
  """Exception for VM failures."""

  def __init__(self, message):
    super(VMError, self).__init__()
    logging.error(message)


class VM(object):
  """Class for managing a VM."""

  SSH_PORT = 9222
  VM_DIR = '/var/run/cros_vm'


  def __init__(self, kvm_path=None, image_path=None, ssh_port=SSH_PORT,
               dry_run=False):
    """Initialize VM.

    Args:
      kvm_path: path to kvm binary.
      image_path: path of vm image.
      ssh_port: ssh port to use.
      dry_run: disable VM commands.
    """

    self.kvm_path = kvm_path
    self.image_path = image_path
    self.ssh_port = ssh_port
    self.dry_run = dry_run

    self.pidfile = os.path.join(self.VM_DIR, 'kvm.pid')
    self.kvm_monitor = os.path.join(self.VM_DIR, 'kvm.monitor')
    self.kvm_pipe_in = '%s.in' % self.kvm_monitor  # to KVM
    self.kvm_pipe_out = '%s.out' % self.kvm_monitor  # from KVM
    self.kvm_serial = '%s.serial' % self.kvm_monitor

    # TODO(achuith): support nographics, snapshot, mem_path, usb_passthrough,
    # moblab, etc.

  @staticmethod
  def _CleanupFiles(recreate):
    """Cleanup VM_DIR.

    Args:
      recreate: recreate VM_DIR.
    """
    cros_build_lib.SudoRunCommand(['rm', '-rf', VM.VM_DIR])
    if recreate:
      cros_build_lib.SudoRunCommand(['mkdir', VM.VM_DIR])
      cros_build_lib.SudoRunCommand(['chmod', '777', VM.VM_DIR])

  @staticmethod
  def _FindKVMBinary():
    """Returns path to KVM binary.

    Returns:
      KVM binary path.
    """

    for exe in ['kvm', 'qemu-kvm', 'qemu-system-x86_64']:
      try:
        return cros_build_lib.RunCommand(['which', exe],
                                         redirect_stdout=True).output.rstrip()
      except cros_build_lib.RunCommandError:
        raise VMError('KVM path not found.')

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

    if not self.kvm_path:
      self.kvm_path = self._FindKVMBinary()
    logging.debug('kvm path=%s', self.kvm_path)

    if not self.image_path:
      self.image_path = os.environ.get('VM_IMAGE_PATH', '')
    logging.debug('vm image path=%s', self.image_path)
    if not self.image_path or not os.path.exists(self.image_path):
      raise VMError('VM image path %s does not exist.' % self.image_path)

    self._CleanupFiles(recreate=True)
    open(self.kvm_serial, 'w')
    for pipe in [self.kvm_pipe_in, self.kvm_pipe_out]:
      os.mkfifo(pipe, 0600)

    args = [self.kvm_path, '-m', '2G', '-smp', '4', '-vga', 'cirrus',
            '-pidfile', self.pidfile,
            '-chardev', 'pipe,id=control_pipe,path=%s' % self.kvm_monitor,
            '-serial', 'file:%s' % self.kvm_serial,
            '-mon', 'chardev=control_pipe', '-daemonize',
            '-net', 'nic,model=virtio',
            '-net', 'user,hostfwd=tcp::%d-:22' % self.ssh_port,
            '-drive', 'file=%s,index=0,media=disk,cache=unsafe'
            % self.image_path]
    logging.info(' '.join(args))
    logging.info('Pid file: %s', self.pidfile)
    if not self.dry_run:
      cros_build_lib.SudoRunCommand(args)

  def _GetVMPid(self):
    """Get the pid of the VM.

    Returns:
      pid of the VM.
    """
    if not os.path.exists(self.VM_DIR):
      logging.info('No VM running.')
      return 0

    if not os.path.exists(self.pidfile):
      raise VMError('%s does not exist.' % self.pidfile)

    pid = cros_build_lib.SudoRunCommand(['cat', self.pidfile],
                                        redirect_stdout=True).output.rstrip()
    if not pid.isdigit():
      raise VMError('%s in %s is not a pid.' % (pid, self.pidfile))
    return pid

  def IsRunning(self):
    """Returns True if there's a running VM.

    Returns:
      True if there's a running VM.
    """
    try:
      pid = self._GetVMPid()
    except VMError:
      return False
    return bool(pid)

  def Stop(self):
    """Stop the VM."""

    pid = self._GetVMPid()
    if not pid:
      return

    logging.info('Killing %s.', pid)
    if not self.dry_run:
      cros_build_lib.SudoRunCommand(['kill', '-9', pid])
    self._CleanupFiles(recreate=False)

  def WaitForBoot(self, timeout=10, poll_interval=0.1):
    """Wait for the VM to boot up.

    If there is no VM running, start one.

    Args:
      timeout: maxiumum time to wait before raising an exception.
      poll_interval: interval between checks.
    """
    if not os.path.exists(self.VM_DIR):
      self.Start()

    start_time = time.time()
    while time.time() - start_time < timeout:
      result = self.RemoteCommand(cmd=['echo'])
      if result.returncode == 255:
        time.sleep(poll_interval)
        continue
      elif result.returncode == 0:
        return
      else:
        break
    raise VMError('WaitForBoot failed')

  def RemoteCommand(self, cmd):
    """Run a remote command in the VM.

    Args:
      cmd: command to run, of list type.
      verbose: verbose logging output.
    """
    if not isinstance(cmd, list):
      raise VMError('cmd must be a list.')

    args = ['ssh', '-o', 'UserKnownHostsFile=/dev/null',
            '-o', 'StrictHostKeyChecking=no',
            '-i', remote_access.TEST_PRIVATE_KEY,
            '-p', str(self.ssh_port), 'root@localhost']
    args.extend(cmd)

    if not self.dry_run:
      return cros_build_lib.RunCommand(args, redirect_stdout=True,
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
  parser.add_argument('--start', action='store_true',
                      help='Start the VM.')
  parser.add_argument('--stop', action='store_true',
                      help='Stop the VM.')
  parser.add_argument('--cmd', help='Run this command in the VM.')
  parser.add_argument('--image-path', type='path',
                      help='Path to VM image to launch with --start.')
  parser.add_argument('--kvm-path', type='path',
                      help='Path of kvm binary to launch with --start.')
  parser.add_argument('--ssh-port', type=int, default=VM.SSH_PORT,
                      help='ssh port to communicate with VM.')
  parser.add_argument('--dry-run', action='store_true',
                      help='dry run for debugging.')
  return parser.parse_args(argv)


def main(argv):
  args = ParseCommandLine(argv)
  vm = VM(kvm_path=args.kvm_path, image_path=args.image_path,
          ssh_port=args.ssh_port, dry_run=args.dry_run)
  vm.PerformAction(start=args.start, stop=args.stop, cmd=args.cmd)
