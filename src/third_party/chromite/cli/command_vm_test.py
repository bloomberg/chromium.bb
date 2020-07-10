# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for integration VM tests for CLI commands.

This module contains the basic functionalities for setting up a VM and testing
the CLI commands.
"""

from __future__ import print_function

from chromite.cli import deploy
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access
from chromite.lib import vm
from chromite.utils import outcap


class Error(Exception):
  """Base exception for CLI command VM tests."""


class SetupError(Error):
  """Raised when error occurs during test environment setup."""


class TestError(Error):
  """Raised when a command test has failed."""


class CommandError(Error):
  """Raised when error occurs during a command test."""


def _PrintCommandLog(command, content):
  """Print out the log |content| for |command|."""
  if content:
    logging.info('\n----------- Start of %s log -----------\n%s\n'
                 '-----------  End of %s log  -----------',
                 command, content.rstrip(), command)


def TestCommandDecorator(command_name):
  """Decorator that runs the command test function."""

  def Decorator(test_function):
    """Inner decorator that actually wraps the function."""

    def Wrapper(command_test):
      """Wrapper for the test function."""
      command = cros_build_lib.CmdToStr(command_test.BuildCommand(command_name))
      logging.info('Running test for %s.', command)
      try:
        test_function(command_test)
        logging.info('Test for %s passed.', command)
      except CommandError as e:
        _PrintCommandLog(command, str(e))
        raise TestError('Test for %s failed.' % command)

    return Wrapper

  return Decorator


class CommandVMTest(object):
  """Base class for CLI command VM tests.

  This class provides the abstract interface for testing CLI commands on a VM.
  The sub-class must define the BuildCommand method in order to be usable. And
  the test functions must use the TestCommandDecorator decorator.
  """

  def __init__(self, board, image_path):
    """Initializes CommandVMTest.

    Args:
      board: Board for the VM to run tests.
      image_path: Path to the image for the VM to run tests.
    """
    self.board = board
    self.image_path = image_path
    self.port = None
    self.device_addr = None

  def BuildCommand(self, command, device=None, pos_args=None, opt_args=None):
    """Builds a CLI command.

    Args:
      command: The sub-command to build on (e.g. 'flash', 'deploy').
      device: The device's address for the command.
      pos_args: A list of positional arguments for the command.
      opt_args: A list of optional arguments for the command.
    """
    raise NotImplementedError()

  def SetUp(self):
    """Creates and starts the VM instance for testing."""
    self.port = remote_access.GetUnusedPort()
    self.device_addr = 'ssh://%s:%d' % (remote_access.LOCALHOST, self.port)
    vm_path = vm.CreateVMImage(image=self.image_path, board=self.board,
                               updatable=True)
    vm_cmd = ['./cros_vm', '--ssh-port=%d' % self.port, '--copy-on-write',
              '--image-path=%s' % vm_path, '--start']
    cros_build_lib.run(vm_cmd, cwd=constants.CHROMITE_BIN_DIR)

  def TearDown(self):
    """Stops the VM instance after testing."""
    if not self.port:
      return
    cros_build_lib.run(['./cros_vm', '--stop', '--ssh-port=%d' % self.port],
                       cwd=constants.CHROMITE_BIN_DIR,
                       error_code_ok=True)

  @TestCommandDecorator('shell')
  def TestShell(self):
    """Tests the shell command."""
    # The path and content of a temporary file for testing shell command.
    path = '/tmp/shell-test'
    content = 'shell command test file'

    cmd = self.BuildCommand('shell', device=self.device_addr,
                            opt_args=['--no-known-hosts'])

    logging.info('Test to use shell command to write a file to the VM device.')
    write_cmd = cmd + ['--', 'echo "%s" > %s' % (content, path)]
    result = cros_build_lib.run(write_cmd, capture_output=True,
                                error_code_ok=True)
    if result.returncode:
      logging.error('Failed to write the file to the VM device.')
      raise CommandError(result.error)

    logging.info('Test to use shell command to read a file on the VM device.')
    read_cmd = cmd + ['--', 'cat %s' % path]
    result = cros_build_lib.run(read_cmd, capture_output=True,
                                error_code_ok=True)
    if result.returncode or result.output.rstrip() != content:
      logging.error('Failed to read the file on the VM device.')
      raise CommandError(result.error)

    logging.info('Test to use shell command to remove a file on the VM device.')
    remove_cmd = cmd + ['--', 'rm %s' % path]
    result = cros_build_lib.run(remove_cmd, capture_output=True,
                                error_code_ok=True)
    if result.returncode:
      logging.error('Failed to remove the file on the VM device.')
      raise CommandError(result.error)

  @TestCommandDecorator('debug')
  def TestDebug(self):
    """Tests the debug command."""
    logging.info('Test to start and debug a new process on the VM device.')
    exe_path = '/bin/bash'
    start_cmd = self.BuildCommand('debug', device=self.device_addr,
                                  opt_args=['--exe', exe_path])
    result = cros_build_lib.run(start_cmd, capture_output=True,
                                error_code_ok=True, input='\n')
    if result.returncode:
      logging.error('Failed to start and debug a new process on the VM device.')
      raise CommandError(result.error)

    logging.info('Test to attach a running process on the VM device.')
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.LOCALHOST, port=self.port) as device:
      exe = 'update_engine'
      pids = device.GetRunningPids(exe, full_path=False)
      if not pids:
        logging.error('Failed to find any running process to debug.')
        raise CommandError()
      pid = pids[0]
      attach_cmd = self.BuildCommand('debug', device=self.device_addr,
                                     opt_args=['--pid', str(pid)])
      result = cros_build_lib.run(attach_cmd, capture_output=True,
                                  error_code_ok=True, input='\n')
      if result.returncode:
        logging.error('Failed to attach a running process on the VM device.')
        raise CommandError(result.error)

  @TestCommandDecorator('flash')
  def TestFlash(self):
    """Tests the flash command."""
    # We explicitly disable reboot after the update because VMs sometimes do
    # not come back after reboot. The flash command does not need to verify
    # the integrity of the updated image. We have AU tests for that.
    cmd = self.BuildCommand('flash', device=self.device_addr,
                            pos_args=['latest'],
                            opt_args=['--no-wipe', '--no-reboot'])

    logging.info('Test to flash the VM device with the latest image.')
    result = cros_build_lib.run(cmd, capture_output=True, error_code_ok=True)
    if result.returncode:
      logging.error('Failed to flash the VM device.')
      raise CommandError(result.error)

  @TestCommandDecorator('deploy')
  def TestDeploy(self):
    """Tests the deploy command."""
    packages = ['dev-python/cherrypy', 'app-portage/portage-utils']
    # Set the installation root to /usr/local so that the command does not
    # attempt to remount rootfs (which leads to VM reboot).
    cmd = self.BuildCommand('deploy', device=self.device_addr,
                            pos_args=packages, opt_args=['--log-level=info',
                                                         '--root=/usr/local'])

    logging.info('Test to uninstall packages on the VM device.')
    with outcap.OutputCapturer() as output:
      result = cros_build_lib.run(cmd + ['--unmerge'], error_code_ok=True)

    if result.returncode:
      logging.error('Failed to uninstall packages on the VM device.')
      raise CommandError(result.error)

    captured_output = output.GetStdout() + output.GetStderr()
    for event in deploy.BrilloDeployOperation.UNMERGE_EVENTS:
      if event not in captured_output:
        logging.error('Strings used by deploy.BrilloDeployOperation to update '
                      'the progress bar have been changed. Please update the '
                      'strings in UNMERGE_EVENTS')
        raise CommandError()

    logging.info('Test to install packages on the VM device.')
    with outcap.OutputCapturer() as output:
      result = cros_build_lib.run(cmd, error_code_ok=True)

    if result.returncode:
      logging.error('Failed to install packages on the VM device.')
      raise CommandError(result.error)

    captured_output = output.GetStdout() + output.GetStderr()
    for event in deploy.BrilloDeployOperation.MERGE_EVENTS:
      if event not in captured_output:
        logging.error('Strings used by deploy.BrilloDeployOperation to update '
                      'the progress bar have been changed. Please update the '
                      'strings in MERGE_EVENTS')
        raise CommandError()

    # Verify that the packages are installed.
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.LOCALHOST, port=self.port) as device:
      try:
        device.RunCommand(['python', '-c', '"import cherrypy"'])
        device.RunCommand(['qmerge', '-h'])
      except cros_build_lib.RunCommandError as e:
        logging.error('Unable to verify packages installed on VM: %s', e)
        raise CommandError()

  def RunTests(self):
    """Calls the test functions."""
    self.TestShell()
    # TestDebug broken (crbug.com/863122)
    self.TestFlash()
    self.TestDeploy()

  def Run(self):
    """Runs the tests."""
    try:
      self.SetUp()
      self.RunTests()
      logging.info('All tests completed successfully.')
    finally:
      self.TearDown()
