# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collection of tools used in scripts while we migrate to bricks."""

from __future__ import print_function

from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib


def ParseArgs(argv):
  """Parse arguments.

  Args:
    argv: array of arguments passed to the script.
  """
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('brick')
  parser.add_argument(
      '--friendly-name', action='store_true', dest='friendlyname',
      help='Returns the friendly name for a given brick. This name is used in '
      'the sysroot path and as "board name" in our legacy tools.')
  options = parser.parse_args(argv)
  options.Freeze()
  return options


def main(argv):
  opts = ParseArgs(argv)

  try:
    brick = brick_lib.Brick(opts.brick, allow_legacy=False)
  except brick_lib.BrickNotFound:
    cros_build_lib.Die('Brick %s not found.' % opts.brick)

  if opts.friendlyname:
    print(brick.FriendlyName())
