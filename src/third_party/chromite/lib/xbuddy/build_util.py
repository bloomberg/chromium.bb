# -*- coding: utf-8 -*-
# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a build related helper class."""

from __future__ import print_function

import os
import sys


class BuildObject(object):
  """Common base class that defines key paths in the source tree.

  Classes that inherit from BuildObject can access scripts in the src/scripts
  directory, and have a handle to the static directory of the devserver.
  """
  def __init__(self, static_dir):
    self.devserver_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    self.static_dir = static_dir
    self.scripts_dir = os.path.join(self.GetSourceRoot(), 'src/scripts')
    self.images_dir = os.path.join(self.GetSourceRoot(), 'src/build/images')

  @staticmethod
  def GetSourceRoot():
    """Returns the path to the source root."""
    src_root = os.environ.get('CROS_WORKON_SRCROOT')
    if not src_root:
      src_root = os.path.join(os.path.dirname(__file__), '..', '..', '..')

    return os.path.realpath(src_root)

  def GetLatestImageLink(self, board):
    """Returns the latest image symlink."""
    return os.path.join(self.images_dir, board, 'latest')

  def GetDefaultBoardID(self):
    """Returns the default board id stored in .default_board.

    Default to x86-generic, if that isn't set.
    """
    board_file = '%s/.default_board' % (self.scripts_dir)
    try:
      with open(board_file) as bf:
        return bf.read().strip()
    except IOError:
      return 'x86-generic'
