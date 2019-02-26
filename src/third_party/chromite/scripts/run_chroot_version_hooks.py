# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update or initialize the chroot version.

The chroot version hooks provide manually created update files to handle
tasks that need to be manually run. See also `update_chroot` for updating
the toolchain and some other packages on the chroot. The `update_chroot` script
also calls this script. The chroot version and installed package versions are
not strongly correlated.
"""

from __future__ import print_function

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_sdk_lib


def GetParser():
  """Build the ArgumentParser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-i', '--init-latest',
                      action='store_true', default=False,
                      help='Create the version file with the latest version '
                           "if it doesn't exist.")

  return parser


def _ParseArgs(argv):
  """Parse arguments."""
  parser = GetParser()

  opts = parser.parse_args(argv)
  opts.Freeze()

  return opts


def main(argv):
  """Main function."""
  cros_build_lib.AssertInsideChroot()

  opts = _ParseArgs(argv)

  if opts.init_latest:
    cros_sdk_lib.InitLatestVersion()
  else:
    try:
      cros_sdk_lib.RunChrootVersionHooks()
    except cros_sdk_lib.Error as e:
      cros_build_lib.Die(e.message)
