# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set up the environment for locally testing fuzz targets."""

from __future__ import print_function

import argparse
import glob
import os
import sys

from chromite.lib import cros_build_lib

in_chroot = cros_build_lib.IsInsideChroot()

def IsMounted(path):
  """Given a path, check to see if it is already mounted."""

  cmd = ['mount']
  result = cros_build_lib.RunCommand(cmd, capture_output=True)
  lines = result.output.strip().split('\n')

  for l in lines:
    if l.find(path) != -1:
      return True

  return False

def RunMountCommands(sysroot_path, unmount):
  """Mount/Unmount the proc & dev paths.

  Checks to see if path is mounted befor attempting to
  mount or unmount it.
  """

  for point in ['proc', 'dev']:
    mount_path = os.path.join(sysroot_path, point)
    if unmount:
      if IsMounted(mount_path):
        command = ['umount', mount_path]
        cros_build_lib.SudoRunCommand(command)
    else:
      if not IsMounted(mount_path):
        if point == 'proc':
          command = ['mount', '-t', 'proc', 'none', mount_path]
        else:
          command = ['mount', '-o', 'bind', '/dev', mount_path]
        cros_build_lib.SudoRunCommand(command)

def main(argv):
  """Parse arguments, calls RunMountCommands & copies files."""

  if in_chroot:
    print('This script must be run from outside the chroot.')
    print('Please exit the chroot and then try re-running this script.')
    return 1

  parser = argparse.ArgumentParser()

  parser.add_argument('--chromeos_root', default=None,
                      required=True,
                      help='Path to Chrome OS checkout (not including '
                      '"chroot").')
  parser.add_argument('--board', default=None,
                      required=True,
                      help='Board on which to run test.')
  parser.add_argument('--cleanup', default=False,
                      action='store_true',
                      help='Use this option after the testing is finished.'
                      ' It will undo the mount commands.')

  options = parser.parse_args(argv)

  chromeos_root = os.path.expanduser(options.chromeos_root)
  chroot_path = os.path.join(chromeos_root, 'chroot')
  sysroot_path = os.path.join(chroot_path, 'build', options.board)

  RunMountCommands(sysroot_path, options.cleanup)

  # Do not copy the files if we are cleaning up.
  if not options.cleanup:
    dst = os.path.join(sysroot_path, 'usr', 'lib64', '.')
    src_pattern = os.path.join(chroot_path, 'usr', 'lib64', 'libLLVM*.so')
    files = glob.glob(src_pattern)
    for src in files:
      cros_build_lib.SudoRunCommand(['cp', src, dst])

    src = os.path.join(chroot_path, 'usr', 'bin', 'asan_symbolize.py')
    dst = os.path.join(sysroot_path, 'usr', 'bin', '.')
    cros_build_lib.SudoRunCommand(['cp', src, dst])

    src = os.path.join(chroot_path, 'usr', 'bin', 'llvm-symbolizer')
    dst = os.path.join(sysroot_path, 'usr', 'bin', '.')
    cros_build_lib.SudoRunCommand(['cp', src, dst])

  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
