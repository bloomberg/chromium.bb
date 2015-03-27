# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collection of tools to create sysroots."""


from __future__ import print_function

import os
import sys

from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib


def ParseArgs(argv):
  """Parse arguments.

  Args:
    argv: array of arguments passed to the script.
  """
  parser = commandline.ArgumentParser(description=__doc__)
  subparser = parser.add_subparsers()
  wrapper = subparser.add_parser('create-wrappers')
  wrapper.add_argument('--sysroot', help='Path to the sysroot.', required=True)
  wrapper.add_argument('--friendlyname', help='Name to append to the commands.')
  wrapper.add_argument('--toolchain', help='Toolchain used in this sysroot.')
  wrapper.set_defaults(command='create-wrappers')

  config = subparser.add_parser('generate-config')
  target = config.add_mutually_exclusive_group(required=True)
  target.add_argument('--board', help='Board to generate the config for.')
  target.add_argument('--brick', help='Brick to generate the config for.')
  config.add_argument('--out-file', dest='out_file',
                      help='File to write into. If not specified, the '
                      'configuration will be printed to stdout.')
  config.add_argument('--sysroot', help='Path to the sysroot.', required=True)
  config.set_defaults(command='generate-config')

  options = parser.parse_args(argv)
  options.Freeze()
  return options


def main(argv):
  opts = ParseArgs(argv)
  if not cros_build_lib.IsInsideChroot():
    raise commandline.ChrootRequiredError()

  if os.geteuid() != 0:
    cros_build_lib.SudoRunCommand(sys.argv, print_cmd=False)
    return

  if opts.command == 'create-wrappers':
    sysroot_lib.CreateAllWrappers(opts.sysroot, opts.toolchain,
                                  opts.friendlyname)
  elif opts.command == 'generate-config':
    if opts.brick:
      config = sysroot_lib.GenerateBrickConfig(
          opts.sysroot, brick=brick_lib.Brick(opts.brick))
    else:
      config = sysroot_lib.GenerateBoardConfig(opts.sysroot, board=opts.board)

    if opts.out_file:
      osutils.WriteFile(opts.out_file, config, makedirs=True)
    else:
      print(config)
