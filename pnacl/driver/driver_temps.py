#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

"""Manages files that'll get deleted when the driver exits unless SAVE_TEMPS.
"""

import os

import pathtools
from driver_env import env
from driver_log import Log, AtDriverExit

class TempFileHandler(object):
  def __init__(self):
    AtDriverExit(self.wipe)
    self.files = []

  def add(self, path):
    if env.getbool('SAVE_TEMPS'):
      return
    path = pathtools.abspath(path)
    self.files.append(path)

  def wipe(self):
    for path in self.files:
      try:
        sys_path = pathtools.tosys(path)
        # If exiting early, the file may not have been created yet.
        if os.path.exists(sys_path):
          os.remove(sys_path)
      except OSError as err:
        Log.Fatal("TempFileHandler: Unable to wipe file %s w/ error %s",
                  pathtools.touser(path),
                  err.strerror)
    self.files = []

TempFiles = TempFileHandler()
