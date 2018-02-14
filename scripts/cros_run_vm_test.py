# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for running catapult smoke tests in a VM."""

from __future__ import print_function

import argparse
import datetime
import os
import re

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.scripts import cros_vm


class VMTest(object):
  """Class for running VM tests."""

  def __init__(self, opts, vm_args):
    self.start_time = datetime.datetime.utcnow()
    self.catapult_tests = opts.catapult_tests
    self.guest = opts.guest
    self.autotest = opts.autotest
    self.board = opts.board
    self.results_dir = opts.results_dir
    self.start_vm = opts.start_vm

    if self.board:
      vm_args += ['--board', self.board]
    self._vm = cros_vm.VM(vm_args)

  def __del__(self):
    self.StopVM()

    logging.info('Time elapsed %s.',
                 datetime.datetime.utcnow() - self.start_time)


  def StartVM(self):
    """Start a VM if necessary.

    If --start-vm is specified, we launch a new VM, otherwise we use an
    existing VM.
    """
    if not self._vm.IsRunning():
      self.start_vm = True

    if self.start_vm:
      self._vm.Start()
    self._vm.WaitForBoot()

  def StopVM(self):
    """Stop the VM if necessary.

    If --start-vm was specified, we launched this VM, so we now stop it.
    """
    if self._vm and self.start_vm:
      self._vm.Stop()

  def Build(self, build_dir):
    """Build chrome.

    Args:
      build_dir: Build directory.
    """
    cros_build_lib.RunCommand(['autoninja', '-C', build_dir, 'chrome',
                               'chrome_sandbox', 'nacl_helper'],
                              log_output=True)

  def Deploy(self, build_dir):
    """Deploy chrome.

    Args:
      build_dir: Build directory.
    """
    cros_build_lib.RunCommand(['deploy_chrome', '--force',
                               '--build-dir', build_dir,
                               '--process-timeout', '180',
                               '--to', 'localhost',
                               '--port', str(self._vm.ssh_port)],
                              log_output=True)
    self._vm.WaitForBoot()

  def _RunCatapultTests(self):
    """Run catapult tests matching a pattern using run_tests.

    Returns:
      cros_build_lib.CommandResult object.
    """

    browser = 'system-guest' if self.guest else 'system'
    return self._vm.RemoteCommand([
        'python',
        '/usr/local/telemetry/src/third_party/catapult/telemetry/bin/run_tests',
        '--browser=%s' % browser,
    ] + self.catapult_tests)

  def _RunAutotest(self):
    """Run an autotest using test_that.

    Returns:
      cros_build_lib.CommandResult object.
    """
    cmd = ['test_that']
    if self.board:
      cmd += ['--board', self.board]
    if self.results_dir:
      cmd += ['--results_dir', self.results_dir]
    cmd += [
        '--no-quickmerge',
        '--ssh_options', '-F /dev/null -i /dev/null',
        'localhost:%d' % self._vm.ssh_port,
    ] + self.autotest
    return cros_build_lib.RunCommand(cmd, log_output=True)

  def RunTests(self):
    """Run catapult tests, autotest, or the default, vm_sanity.

    Returns:
      cros_build_lib.CommandResult object.
    """

    if self.catapult_tests:
      return self._RunCatapultTests()

    if self.autotest:
      return self._RunAutotest()

    return self._vm.RemoteCommand(['/usr/local/autotest/bin/vm_sanity.py'])

  def ProcessResult(self, result, output):
    """Process the CommandResult object from RunVMCmd/RunTests.

    Args:
      result: cros_build_lib.CommandResult object.
      output: output file to write to.
    """
    result_str = 'succeeded' if result.returncode == 0 else 'failed'
    logging.info('Tests %s.', result_str)
    if not output:
      return

    # Skip SSH warning.
    suppress_list = [
        r'Warning: Permanently added .* to the list of known hosts']
    with open(output, 'w') as f:
      lines = result.output.splitlines(True)
      for line in lines:
        for suppress in suppress_list:
          if not re.search(suppress, line):
            f.write(line)

  def RunVMCmd(self, cmd, files, cwd):
    """Run cmd in the VM.

    Copy src files to /usr/local/vm_test/, change working directory to
    |cwd|, run the command |cmd|, and cleanup.

    Args:
      cmd: Command to run in the VM.
      files: List of files to scp over to the VM.
      cwd: Working directory in the VM for the cmd.

    Returns:
      cros_build_lib.CommandResult object.
    """
    # Some sanity checks.
    for f in files:
      if os.path.isabs(f):
        raise ValueError('%s should be a relative path' % f)
      if not os.path.exists(f):
        raise ValueError('%s does not exist' % f)

    DEST_BASE = '/usr/local/vm_test'
    # Copy files, preserving the directory structure.
    for f in files:
      dirname = os.path.join(DEST_BASE, os.path.dirname(f))
      self._vm.RemoteCommand(['mkdir', '-p', dirname])
      self._vm.remote.CopyToDevice(src=f, dest=dirname, mode='scp',
                                   debug_level=logging.INFO)

    # Run the remote command with cwd.
    cwd = os.path.join(DEST_BASE, cwd) if cwd else DEST_BASE
    cmd = '"cd %s; %s"' % (cwd, ' '.join(cmd))
    result = self._vm.RemoteCommand(cmd, shell=True)

    # Cleanup.
    self._vm.RemoteCommand(['rm', '-rf', DEST_BASE])
    return result

def ParseCommandLine(argv):
  """Parse the command line.

  Args:
    argv: Command arguments.

  Returns:
    List of parsed options for VMTest, and args to pass to VM.
  """

  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--start-vm', action='store_true', default=False,
                      help='Start a new VM before running tests.')
  parser.add_argument('--catapult-tests', nargs='+',
                      help='Catapult test pattern to run, passed to run_tests.')
  parser.add_argument('--autotest', nargs='+',
                      help='Autotest test pattern to run, passed to test_that.')
  parser.add_argument('--board', help='Board to use.')
  parser.add_argument('--results-dir', help='Autotest results directory.')
  parser.add_argument('--output', type='path', help='Save output to file.')
  parser.add_argument('--guest', action='store_true', default=False,
                      help='Run tests in incognito mode.')
  parser.add_argument('--build-dir', type='path',
                      help='Directory for building and deploying chrome.')
  parser.add_argument('--build', action='store_true', default=False,
                      help='Before running tests, build chrome using ninja, '
                      '--build-dir must be specified.')
  parser.add_argument('--deploy', action='store_true', default=False,
                      help='Before running tests, deploy chrome to the VM, '
                      '--build-dir must be specified.')
  parser.add_argument('--cwd', help='Change working directory.')
  parser.add_argument('--files', default=[], action='append',
                      help='Files to scp to the VM.')
  parser.add_argument('--files-from', type='path',
                      help='File with list of files to copy to the VM.')
  parser.add_argument('--cmd', action='store_true', default=False,
                      help='Run a command in the VM.')
  parser.add_argument('args', nargs=argparse.REMAINDER,
                      help='Command to run in the VM.')

  opts, vm_args = parser.parse_known_args(argv)

  if opts.build or opts.deploy:
    if not opts.build_dir:
      parser.error('Must specifiy --build-dir with --build or --deploy.')
    if not os.path.isdir(opts.build_dir):
      parser.error('%s is not a directory.' % opts.build_dir)

  # Ensure command is provided. For eg, to copy out to the VM and run
  # out/unittest:
  # cros_run_vm_test --files out --cwd out --cmd -- ./unittest
  if opts.cmd and len(opts.args) < 2:
    parser.error('Must specify test command to run.')
  # Verify additional args.
  if opts.args:
    if not opts.cmd:
      parser.error('Additional args may only be specified with --cmd: %s'
                   % opts.args)
    if opts.args[0] != '--':
      parser.error('Additional args must start with \'--\': %s' % opts.args)

  return opts, vm_args


def FileList(files, files_from):
  """Get list of files from command line args --files and --files-from.

  Args:
    files: files specified directly on the command line.
    files_from: files specified in a file.

  Returns:
    Contents of files_from if it exists, otherwise files.
  """
  if files_from and os.path.isfile(files_from):
    with open(files_from) as f:
      files = [line.rstrip() for line in f]
  return files

def main(argv):
  opts, vm_args = ParseCommandLine(argv)
  vm_test = VMTest(opts, vm_args)
  vm_test.StartVM()

  if opts.build:
    vm_test.Build(opts.build_dir)

  if opts.build or opts.deploy:
    vm_test.Deploy(opts.build_dir)

  if opts.cmd:
    files = FileList(opts.files, opts.files_from)
    result = vm_test.RunVMCmd(opts.args[1:], files, opts.cwd)
  else:
    result = vm_test.RunTests()
  vm_test.ProcessResult(result, opts.output)

  vm_test.StopVM()
  return result.returncode
