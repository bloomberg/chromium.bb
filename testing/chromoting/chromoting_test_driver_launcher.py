# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility script to run chromoting test driver tests on the Chromoting bot."""

import argparse

from chromoting_test_utilities import InitialiseTestMachineForLinux
from chromoting_test_utilities import PrintHostLogContents
from chromoting_test_utilities import PROD_DIR_ID
from chromoting_test_utilities import RunCommandInSubProcess
from chromoting_test_utilities import TestCaseSetup
from chromoting_test_utilities import TestMachineCleanup

TEST_ENVIRONMENT_TEAR_DOWN_INDICATOR = 'Global test environment tear-down'
FAILED_INDICATOR = '[  FAILED  ]'


def LaunchCTDCommand(args, command):
  """Launches the specified chromoting test driver command.

  Args:
    args: Command line args, used for test-case startup tasks.
    command: Chromoting Test Driver command line.
  Returns:
    "command" if there was a test environment failure, otherwise a string of the
    names of failed tests.
  """

  TestCaseSetup(args)
  results = RunCommandInSubProcess(command)

  tear_down_index = results.find(TEST_ENVIRONMENT_TEAR_DOWN_INDICATOR)
  if tear_down_index == -1:
    # The test environment did not tear down. Something went horribly wrong.
    return '[Command failed]: ' + command

  end_results_list = results[tear_down_index:].split('\n')
  failed_tests_list = []
  for result in end_results_list:
    if result.startswith(FAILED_INDICATOR):
      failed_tests_list.append(result)

  if failed_tests_list:
    test_result = '[Command]: ' + command
    # Note: Skipping the first one is intentional.
    for i in range(1, len(failed_tests_list)):
      test_result += '    ' + failed_tests_list[i]
    return test_result

  # All tests passed!
  return ''


def main(args):
  InitialiseTestMachineForLinux(args.cfg_file)

  failed_tests = ''

  with open(args.commands_file) as f:
    for line in f:
      # Replace the PROD_DIR value in the command-line with
      # the passed in value.
      line = line.replace(PROD_DIR_ID, args.prod_dir)
      # Launch specified command line for test.
      failed_tests += LaunchCTDCommand(args, line)

  # All tests completed. Include host-logs in the test results.
  PrintHostLogContents()

  if failed_tests:
    print '++++++++++FAILED TESTS++++++++++'
    print failed_tests.rstrip('\n')
    print '++++++++++++++++++++++++++++++++'
    raise Exception('At least one test failed.')

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--commands_file',
                      help='path to file listing commands to be launched.')
  parser.add_argument('-p', '--prod_dir',
                      help='path to folder having product and test binaries.')
  parser.add_argument('-c', '--cfg_file',
                      help='path to test host config file.')
  parser.add_argument('--me2me_manifest_file',
                      help='path to me2me host manifest file.')
  parser.add_argument('--it2me_manifest_file',
                      help='path to it2me host manifest file.')
  parser.add_argument(
      '-u', '--user_profile_dir',
      help='path to user-profile-dir, used by connect-to-host tests.')
  command_line_args = parser.parse_args()
  try:
    main(command_line_args)
  finally:
    # Stop host and cleanup user-profile-dir.
    TestMachineCleanup(command_line_args.user_profile_dir)
