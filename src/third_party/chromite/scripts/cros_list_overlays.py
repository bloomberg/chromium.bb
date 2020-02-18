# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Calculate what overlays are needed for a particular board."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import portage_util


def _ParseArguments(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--board', default=None, help='Board name')
  parser.add_argument('-a', '--all', default=False, action='store_true',
                      help='Show all overlays (even common ones).')

  opts = parser.parse_args(argv)
  opts.Freeze()

  return opts


def main(argv):
  opts = _ParseArguments(argv)

  overlays = portage_util.FindOverlays(constants.BOTH_OVERLAYS, opts.board)

  # Exclude any overlays in src/third_party, for backwards compatibility with
  # scripts that expected these to not be listed.
  if not opts.all:
    ignore_prefix = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party')
    overlays = [o for o in overlays if not o.startswith(ignore_prefix)]

  print('\n'.join(overlays))
