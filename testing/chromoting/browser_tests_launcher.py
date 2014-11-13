# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utility script to launch browser-tests on the Chromoting bot."""
import argparse
import subprocess

PROD_DIR_ID = '$(PROD_DIR)'


def LaunchCommand(command):

  cmd_line = [command]
  try:
    p = subprocess.Popen(cmd_line, stdout=subprocess.PIPE, shell=True)
    results, err = p.communicate()
    if 'SUCCESS: all tests passed.' not in results:
      raise Exception('Test failed\n%s\n%s' % (results, err))
  except subprocess.CalledProcessError, e:
    raise Exception('Exception %s running command %s' % (e, command))
  else:
    print results


def main():

  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--file',
                      help='path to file listing commands to be launched.')
  parser.add_argument('-p', '--prod_dir',
                      help='path to folder having product and test binaries.')

  args = parser.parse_args()

  with open(args.file) as f:
    for line in f:
      # Replace the PROD_DIR value in the command-line with
      # the passed in value.
      line = line.replace(PROD_DIR_ID, args.prod_dir)
      LaunchCommand(line)

if __name__ == '__main__':
  main()
