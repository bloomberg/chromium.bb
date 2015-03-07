# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Calculate what overlays are needed for a particular board."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import portage_util


def _ParseArguments(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board', default=None, help='Board name')
  parser.add_argument('--board_overlay', default=None,
                      help='Location of the board overlay. Used by '
                           './setup_board to allow developers to add custom '
                           'overlays.')
  parser.add_argument('--primary_only', default=False, action='store_true',
                      help='Only return the path to the primary overlay. This '
                           'only makes sense when --board is specified.')
  parser.add_argument('-a', '--all', default=False, action='store_true',
                      help='Show all overlays (even common ones).')
  parser.add_argument('--brick', help='Main brick to use')

  opts = parser.parse_args(argv)
  opts.Freeze()

  if opts.primary_only and opts.board is None:
    parser.error('--board is required when --primary_only is supplied.')

  if opts.brick:
    if opts.board:
      parser.error('--board and --brick are incompatible.')

    if opts.all:
      parser.error('Cannot list all overlays with --brick')

  return opts


def main(argv):
  opts = _ParseArguments(argv)
  args = (constants.BOTH_OVERLAYS, opts.board)

  if opts.brick:
    main_brick = brick_lib.Brick(opts.brick)
    overlays = [b.OverlayDir() for b in main_brick.BrickStack()]
  else:
    # Verify that a primary overlay exists.
    try:
      primary_overlay = portage_util.FindPrimaryOverlay(*args)
    except portage_util.MissingOverlayException as ex:
      cros_build_lib.Die(str(ex))

    # Get the overlays to print.
    if opts.primary_only:
      overlays = [primary_overlay]
    else:
      overlays = portage_util.FindOverlays(*args)

  # Exclude any overlays in src/third_party, for backwards compatibility with
  # scripts that expected these to not be listed.
  if not opts.all:
    ignore_prefix = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party')
    overlays = [o for o in overlays if not o.startswith(ignore_prefix)]

  if opts.board_overlay and os.path.isdir(opts.board_overlay):
    overlays.append(os.path.abspath(opts.board_overlay))

  print('\n'.join(overlays))
