#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import unittest
import tempfile

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# CIPD client version to use for self-update from an "old" checkout to the tip.
#
# This version is from Jan 2018.
OLD_VERSION = 'git_revision:a1f61935faa60feb73e37556fdf791262c2dedce'


class CipdBootstrapTest(unittest.TestCase):
  """Tests that CIPD client can bootstrap from scratch and self-update from some
  old version to a most recent one.

  WARNING: This integration test touches real network and real CIPD backend and
  downloads several megabytes of stuff.
  """

  def setUp(self):
    self.tempdir = tempfile.mkdtemp('depot_tools_cipd')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def stage_files(self, cipd_version=None):
    """Copies files needed for cipd bootstrap into the temp dir.

    Args:
      cipd_version: if not None, a value to put into cipd_client_version file.
    """
    for f in ('cipd', 'cipd.bat', 'cipd.ps1', 'cipd_client_version'):
      shutil.copy2(os.path.join(ROOT_DIR, f), os.path.join(self.tempdir, f))
    if cipd_version is not None:
      with open(os.path.join(self.tempdir, 'cipd_client_version'), 'wt') as f:
        f.write(cipd_version+'\n')

  def call_cipd_help(self):
    """Calls 'cipd help' bootstrapping the client in tempdir.

    Returns (exit code, merged stdout and stderr).
    """
    exe = 'cipd.bat' if sys.platform == 'win32' else 'cipd'
    p = subprocess.Popen(
        [os.path.join(self.tempdir, exe), 'help'],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out, _ = p.communicate()
    return p.returncode, out

  def test_new_bootstrap(self):
    """Bootstrapping the client from scratch."""
    self.stage_files()
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Bootstrap from scratch failed:\n%s' % out)

  def test_self_update(self):
    """Updating the existing client in-place."""
    self.stage_files(cipd_version=OLD_VERSION)
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Update to %s fails:\n%s' % (OLD_VERSION, out))
    self.stage_files()
    ret, out = self.call_cipd_help()
    if ret:
      self.fail('Update from %s to the tip fails:\n%s' % (OLD_VERSION, out))


if __name__ == '__main__':
  unittest.main()
