# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Calculate what overlays are needed for a particular board."""

import optparse
import os

from chromite.lib import cros_build_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import portage_utilities


def _ParseArguments(argv):
  parser = optparse.OptionParser(usage='USAGE: %prog [options]')

  parser.add_option('--board', default=None, help='Board name')
  parser.add_option('--board_overlay', default=None,
                    help='Location of the board overlay. Used by ./setup_board '
                         'to allow developers to add custom overlays.')
  parser.add_option('--primary_only', default=False, action='store_true',
                    help='Only return the path to the primary overlay. This '
                         'only makes sense when --board is specified.')

  flags, remaining_arguments = parser.parse_args(argv)

  if flags.primary_only and flags.board is None:
    parser.error('--board is required when --primary_only is supplied.')

  if remaining_arguments:
    parser.print_help()
    parser.error('Invalid arguments')

  return flags


def main(argv):
  flags = _ParseArguments(argv)
  args = (constants.BOTH_OVERLAYS, flags.board)

  # Verify that a primary overlay exists.
  try:
    primary_overlay = portage_utilities.FindPrimaryOverlay(*args)
  except portage_utilities.MissingOverlayException as ex:
    cros_build_lib.Die(str(ex))

  # Get the overlays to print.
  if flags.primary_only:
    overlays = [primary_overlay]
  else:
    overlays = portage_utilities.FindOverlays(*args)

  # Exclude any overlays in src/third_party, for backwards compatibility with
  # scripts that expected these to not be listed.
  ignore_prefix = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party')
  overlays = [o for o in overlays if not o.startswith(ignore_prefix)]

  if flags.board_overlay and os.path.isdir(flags.board_overlay):
    overlays.append(os.path.abspath(flags.board_overlay))

  print '\n'.join(overlays)
