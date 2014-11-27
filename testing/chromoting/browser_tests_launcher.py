# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utility script to launch browser-tests on the Chromoting bot."""
import argparse
import hashlib
import os
from os.path import expanduser
import shutil
import socket
import subprocess

BROWSER_TEST_ID = 'browser_tests'
PROD_DIR_ID = '#PROD_DIR#'
HOST_HASH_VALUE = hashlib.md5(socket.gethostname()).hexdigest()
SUCCESS_INDICATOR = 'SUCCESS: all tests passed.'
NATIVE_MESSAGING_DIR = 'NativeMessagingHosts'
CRD_ID = 'chrome-remote-desktop'  # Used in a few file/folder names


def LaunchCommand(command):
  cmd_line = [command]
  try:
    p = subprocess.Popen(cmd_line, stdout=subprocess.PIPE, shell=True)
    results, err = p.communicate()
    # Check that the test passed.
    if SUCCESS_INDICATOR not in results:
      raise Exception(
          'Test failed. Command:%s\nResults:%s\nError:%s\n' %
          (command, results, err))
  except subprocess.CalledProcessError, e:
    raise Exception('Exception %s running command %s' % (e, command))
  else:
    print results


def InitialiseTestMachineForLinux(cfg_file, manifest_file, user_profile_dir):
  """Sets up a Linux machine for connect-to-host browser-tests.

  Copy over me2me host-config and manifest files to expected locations.
  By default, the Linux me2me host expects the host-config file to be under
  $HOME/.config/chrome-remote-desktop
  Its name is expected to have a hash that is specific to a machine.

  When a user launches the remoting web-app, the native-message host process is
  started. For this to work, the manifest file for me2me host is expected to be
  in a specific folder under the user-profile dir.

  This function performs both the above tasks.

  TODO(anandc):
  Once we have Linux machines in the swarming lab already installed with the
  me2me host, this function should also perform the step of starting the host.
  That is gated on this CL: https://chromereviews.googleplex.com/123957013/, and
  then having base images in the chrome-labs be updated with it.

  Args:
    cfg_file: location of test account's host-config file.
    manifest_file: location of me2me host manifest file.
    user_profile_dir: user-profile-dir to be used by the connect-to-host tests.
  """

  # First get home directory on current machine.
  home_dir = expanduser('~')
  default_config_file_location = os.path.join(home_dir, '.config', CRD_ID)
  if os.path.exists(default_config_file_location):
    shutil.rmtree(default_config_file_location)
  os.makedirs(default_config_file_location)

  # Copy over test host-config to expected location, with expected file-name.
  # The file-name should contain a hash-value that is machine-specific.
  default_config_file_name = 'host#%s.json' % HOST_HASH_VALUE
  config_file_src = os.path.join(os.getcwd(), cfg_file)
  shutil.copyfile(
      config_file_src,
      os.path.join(default_config_file_location, default_config_file_name))

  # Next, create a user-profile dir, and place the me2me manifest.json file in
  # the expected location for native-messating-host to work properly.
  native_messaging_folder = os.path.join(user_profile_dir, NATIVE_MESSAGING_DIR)

  if os.path.exists(native_messaging_folder):
    shutil.rmtree(native_messaging_folder)
  os.makedirs(native_messaging_folder)

  manifest_file_src = os.path.join(os.getcwd(), manifest_file)
  manifest_file_dest = (
      os.path.join(native_messaging_folder, os.path.basename(manifest_file)))
  shutil.copyfile(manifest_file_src, manifest_file_dest)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--commands_file',
                      help='path to file listing commands to be launched.')
  parser.add_argument('-p', '--prod_dir',
                      help='path to folder having product and test binaries.')
  parser.add_argument('-c', '--cfg_file',
                      help='path to test host config file.')
  parser.add_argument('-m', '--manifest_file',
                      help='path to me2me host manifest file.')
  parser.add_argument(
      '-u', '--user_profile_dir',
      help='path to user-profile-dir, used by connect-to-host tests.')

  args = parser.parse_args()

  InitialiseTestMachineForLinux(args.cfg_file, args.manifest_file,
                                args.user_profile_dir)

  with open(args.commands_file) as f:
    for line in f:
      # Replace the PROD_DIR value in the command-line with
      # the passed in value.
      line = line.replace(PROD_DIR_ID, args.prod_dir)
      LaunchCommand(line)

if __name__ == '__main__':
  main()
