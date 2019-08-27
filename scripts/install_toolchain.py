# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Installs cross toolchain libraries into a sysroot.

This script is not meant to be used by developers directly.
Run at your own risk.
"""

from __future__ import print_function

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import sysroot_lib
from chromite.lib import toolchain


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--noconfigure', dest='configure', default=True,
                      action='store_false',
                      help='Disable updating config files in <sysroot>/etc '
                           'after installation.')
  parser.add_argument('--force', default=False, action='store_true',
                      help='Install toolchain even if already up to date.')
  parser.add_argument('--toolchain',
                      help='Toolchain. For example: i686-pc-linux-gnu, '
                           'armv7a-softfloat-linux-gnueabi.')
  parser.add_argument('--sysroot', type='path', required=True,
                      help='The sysroot to install the toolchain for.')

  return parser


def _GetToolchain(toolchain_name, sysroot):
  """Get the toolchain value or exit with an error message."""
  if toolchain_name:
    # Use the CLI argument when provided.
    return toolchain_name

  # Fetch the value from the sysroot.
  toolchain_name = sysroot.GetStandardField(sysroot_lib.STANDARD_FIELD_CHOST)
  if not toolchain_name:
    cros_build_lib.Die('No toolchain specified in the sysroot or command line.')

  return toolchain_name


def _ParseArgs(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  # Expand the sysroot path to a sysroot object.
  opts.sysroot = sysroot_lib.Sysroot(opts.sysroot)
  # Make sure the toolchain value reflects the toolchain we will be using.
  opts.toolchain = _GetToolchain(opts.toolchain, opts.sysroot)

  opts.Freeze()
  return opts


def main(argv):
  cros_build_lib.AssertInsideChroot()

  opts = _ParseArgs(argv)
  try:
    toolchain.InstallToolchain(sysroot=opts.sysroot,
                               toolchain=opts.toolchain,
                               force=opts.force, configure=opts.configure)
  except (toolchain.Error, cros_build_lib.RunCommandError, ValueError) as e:
    cros_build_lib.Die(e)
