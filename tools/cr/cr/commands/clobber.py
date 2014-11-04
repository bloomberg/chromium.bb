# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the clobber command."""

import os
import shutil

import cr


class ClobberCommand(cr.Command):
  """The implementation of the clobber command.

  The clobber command removes all generated files from the output directory.
  """

  def __init__(self):
    super(ClobberCommand, self).__init__()
    self.help = 'Clobber the current output directory'
    self.description = ("""
        This deletes all generated files from the output directory.
        """)

  def Run(self):
    self.Clobber()

  @classmethod
  def Clobber(cls):
    """Performs the clobber."""
    build_dir = cr.context.Get('CR_BUILD_DIR')
    verbose = cr.context.verbose >= 3 or cr.context.dry_run
    delete = not cr.context.dry_run
    print 'Clobbering...'
    for f in os.listdir(build_dir):
      if f == cr.base.client.CLIENT_CONFIG_PATH:
        continue
      path = os.path.join(build_dir, f)
      if os.path.isfile(path):
        if verbose:
          print 'Delete file %s' % path
        if delete:
          os.unlink(path)
      elif os.path.isdir(path):
        if verbose:
          print 'Delete folder %s' % path
        if delete:
          shutil.rmtree(path)
    print 'Done'
