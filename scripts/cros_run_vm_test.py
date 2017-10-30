# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for running catapult smoke tests in a VM."""

from __future__ import print_function

import datetime
import os
import re

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.scripts import cros_vm


class VMTest(object):
  """Class for running VM tests."""

  CATAPULT_RUN_TESTS = \
      '/usr/local/telemetry/src/third_party/catapult/telemetry/bin/run_tests'
  SANITY_TEST = '/usr/local/autotest/bin/vm_sanity.py'

  def __init__(self, catapult_tests, guest, start_vm, vm_args):
    self.start_time = datetime.datetime.utcnow()
    self.test_pattern = catapult_tests
    self.guest = guest
    self.start_vm = start_vm

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
    cros_build_lib.RunCommand(['ninja', '-C', build_dir, 'chrome',
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

  def _RunCatapultTests(self, test_pattern, guest):
    """Run catapult tests matching a pattern.

    Args:
      test_pattern: Run tests matching this pattern.
      guest: Run in guest mode.

    Returns:
      cros_build_lib.CommandResult object.
    """

    browser = 'system-guest' if guest else 'system'
    return self._vm.RemoteCommand(['python', self.CATAPULT_RUN_TESTS,
                                   '--browser=%s' % browser,
                                   test_pattern])

  def RunTests(self):
    """Run all eligible tests.

    If a test_pattern for catapult tests has been specified, run those tests.
    Otherwise, run vm_sanity.

    Returns:
      cros_build_lib.CommandResult object.
    """

    if self.test_pattern:
      return self._RunCatapultTests(self.test_pattern, self.guest)

    return self._vm.RemoteCommand([self.SANITY_TEST])

  def ProcessResult(self, result, output):
    """Process the CommandResult object from RunCmd/RunTests.

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
  parser.add_argument('--catapult-tests',
                      help='Catapult test pattern to run, passed to run_tests.')
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

  opts, vm_args = parser.parse_known_args(argv)
  if opts.build or opts.deploy:
    if not opts.build_dir:
      parser.error('Must specifiy --build-dir with --build or --deploy.')
    if not os.path.isdir(opts.build_dir):
      parser.error('%s is not a directory.' % opts.build_dir)
  return opts, vm_args


def main(argv):
  opts, vm_args = ParseCommandLine(argv)
  vm_test = VMTest(catapult_tests=opts.catapult_tests,
                   guest=opts.guest,
                   start_vm=opts.start_vm, vm_args=vm_args)

  vm_test.StartVM()

  if opts.build:
    vm_test.Build(opts.build_dir)

  if opts.build or opts.deploy:
    vm_test.Deploy(opts.build_dir)

  result = vm_test.RunTests()
  vm_test.ProcessResult(result, opts.output)

  vm_test.StopVM()
  return result.returncode
