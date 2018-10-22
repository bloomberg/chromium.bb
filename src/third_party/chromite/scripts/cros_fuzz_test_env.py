# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set up the environment for locally testing fuzz targets."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils


def SetUpOrRemoveDevices(sysroot_path, remove):
  """Set up or remove devices in |sysroot_path|/dev.

  If |remove| is True, then dev/null, dev/random, and dev/urandom are removed if
  they exist.
  If |remove| is not True, then dev/null, dev/random, and dev/urandom are
  created, if they already exist then they are recreated.
  """
  dev_path = os.path.join(sysroot_path, 'dev')
  if not remove and not os.path.exists(dev_path):
    cros_build_lib.SudoRunCommand(['mkdir', dev_path])

  def get_path(device):
    return os.path.join(sysroot_path, 'dev', device)

  DEVICE_TO_CMD = {
      'null': ['mknod', '-m', '666', get_path('null'), 'c', '1', '3'],
      'random': ['mknod', '-m', '444', get_path('random'), 'c', '1', '8'],
      'urandom': ['mknod', '-m', '444', get_path('urandom'), 'c', '1', '9'],
  }

  for device in DEVICE_TO_CMD:
    device_path = get_path(device)
    # Remove devices in case they were added to dev, but are not what we expect.
    if os.path.exists(device_path):
      # Use -r since dev/null is sometimes a directory.
      cros_build_lib.SudoRunCommand(['rm', '-r', device_path])

    if not remove:
      cros_build_lib.SudoRunCommand(DEVICE_TO_CMD[device])


def MountOrUnmountProc(sysroot_path, unmount):
  """Mount or unmount |sysroot_path|/proc.

  Mounts |sysroot_path|/proc if |unmount| is False and it isn't already mounted.
  If it is already mounted and |unmount| is True then it is unmounted.
  """
  proc_path = os.path.join(sysroot_path, 'proc')
  if unmount:
    if osutils.IsMounted(proc_path):
      osutils.UmountDir(proc_path, cleanup=False)
    return

  if osutils.IsMounted(proc_path):
    # Don't remount /proc if it is already mounted.
    return

  if not os.path.exists(proc_path):
    cros_build_lib.SudoRunCommand(['mkdir', proc_path])

  cmd = ['mount', '-t', 'proc', 'none', proc_path]
  cros_build_lib.SudoRunCommand(cmd)


def GetParser():
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board',
                      required=True,
                      help='Board on which to run test.')
  parser.add_argument('--cleanup',
                      action='store_true',
                      help='Use this option after the testing is finished.'
                      ' It will undo the setup commands.')

  return parser


def main(argv):
  """Parse arguments and handle setting up or cleanup of fuzzing environment.

  Parse arguments, copies necessary files, calls MountOrUnmountProc, and calls
  SetUpOrRemoveDevices.
  """

  cros_build_lib.AssertOutsideChroot()

  parser = GetParser()
  options = parser.parse_args(argv)

  chromeos_root = constants.SOURCE_ROOT
  chroot_path = os.path.join(chromeos_root, 'chroot')
  sysroot_path = os.path.join(chroot_path, 'build', options.board)

  MountOrUnmountProc(sysroot_path, options.cleanup)
  SetUpOrRemoveDevices(sysroot_path, options.cleanup)

  # Do not copy the files if we are cleaning up.
  if not options.cleanup:
    src = os.path.join(chroot_path, 'usr', 'bin', 'asan_symbolize.py')
    dst = os.path.join(sysroot_path, 'usr', 'bin')
    cros_build_lib.SudoRunCommand(['cp', src, dst])

    # Create a directory for installing llvm-symbolizer and all of
    # its dependencies in the sysroot.
    install_path = os.path.join('usr', 'libexec', 'llvm-symbolizer')
    install_dir = os.path.join(sysroot_path, install_path)
    cmd = ['mkdir', '-p', install_dir]
    cros_build_lib.SudoRunCommand(cmd)

    # Now install llvm-symbolizer, with all its dependencies, in the
    # sysroot.
    llvm_symbolizer = os.path.join('usr', 'bin', 'llvm-symbolizer')

    lddtree_script = os.path.join(chromeos_root, 'chromite', 'bin', 'lddtree')
    cmd = [lddtree_script, '-v', '--generate-wrappers', '--root', chroot_path,
           '--copy-to-tree', install_dir, os.path.join('/', llvm_symbolizer)]
    cros_build_lib.SudoRunCommand(cmd)

    # Create a symlink to llvm-symbolizer in the sysroot.
    rel_path = os.path.relpath(install_dir, dst)
    link_path = os.path.join(rel_path, llvm_symbolizer)
    dest = os.path.join(sysroot_path, llvm_symbolizer)
    if not os.path.exists(dest):
      cmd = ['ln', '-s', link_path, '-t', dst]
      cros_build_lib.SudoRunCommand(cmd)
