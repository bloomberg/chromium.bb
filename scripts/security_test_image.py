# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script provides CLI access to run security tests on a Chrome OS images.

The entry point is available as image_lib.SecurityTest. Call that directly when
possible.

Note: You probably will need an internal checkout by default for these
      tests to be useful. You can provide your own baselines, but you
      can certainly provide your own set of configs.

Note: These tests will fail on dev images. They are designed to
      check release recovery images only.

Note: The --image argument can be a path or a basename. When a basename is
      provided, the --board argument is always used to build the path.
      Consequently, `./image_name.bin` and `image_name.bin` are treated
      very differently.
"""

from __future__ import print_function

import re

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import image_lib


def GetParser():
  """Build the Argument Parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board', help='The board to test an image for.')
  # Avoiding type='path' to allow the use of `./` to distinguish between a
  # local image (e.g. `./image_name.bin`) and a basename (`image_name.bin`) in
  # the board's build directory. The `./` would be normalized out of a
  # type='path' argument, making it look like it's a basename.
  parser.add_argument('--image',
                      help='Source release image to use (recovery_image.bin by '
                           'default). May be a path to an image or just the '
                           'basename of the image if a board is also provided.')
  parser.add_argument('--baselines', type='path',
                      help='Directory to load security baselines from (default '
                           'from cros-signing).')
  parser.add_argument('--vboot-hash',
                      help='The git rev of the vboot tree to checkout (default '
                           'to the signer hash).')

  return parser


def _ParseArgs(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  # Need the board if no image provided or only the basename is provided so
  # we can build out the full path to an image file.
  opts.board = opts.board or cros_build_lib.GetDefaultBoard()
  try:
    opts.image = image_lib.BuildImagePath(opts.board, opts.image)
  except image_lib.ImageDoesNotExistError as e:
    # Replace |arg| with --arg, otherwise messages still relevant.
    message = re.sub(r'\|(\w+)\|', r'--\1', str(e))
    parser.error(message)

  opts.Freeze()
  return opts


def main(argv):
  cros_build_lib.AssertInsideChroot()
  opts = _ParseArgs(argv)
  try:
    success = image_lib.SecurityTest(board=opts.board, image=opts.image,
                                     baselines=opts.baselines,
                                     vboot_hash=opts.vboot_hash)
  except image_lib.Error as e:
    cros_build_lib.Die(e)
  else:
    return 0 if success else 1
