#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests of the pnacl driver.

This tests various driver_tools / driver_log utility functions.
"""

from driver_env import env
import driver_log
import driver_temps
import driver_tools
import pathtools

import os
import tempfile
import unittest


class TestTempNamegenFileWipe(unittest.TestCase):

  def setUp(self):
    self.tempfiles = []

  def tearDown(self):
    # Just in case the test fails, try to wipe the temp files
    # ourselves.
    for t in self.tempfiles:
      if os.path.exists(t.name):
        os.remove(t.name)

  def nameGenTemps(self):
    temp_out = pathtools.normalize(tempfile.NamedTemporaryFile().name)
    temp_in1 = pathtools.normalize(tempfile.NamedTemporaryFile().name)
    temp_in2 = pathtools.normalize(tempfile.NamedTemporaryFile().name)
    namegen = driver_tools.TempNameGen([temp_in1, temp_in2],
                                       temp_out)
    t_gen_out = namegen.TempNameForOutput('pexe')
    t_gen_in = namegen.TempNameForInput(temp_in1, 'bc')

    # Touch/create them (append to not truncate).
    fp = driver_log.DriverOpen(t_gen_out, 'a')
    self.assertTrue(os.path.exists(t_gen_out))
    self.tempfiles.append(fp)
    fp.close()

    fp2 = driver_log.DriverOpen(t_gen_in, 'a')
    self.assertTrue(os.path.exists(t_gen_in))
    self.tempfiles.append(fp2)
    fp2.close()
    return t_gen_out, t_gen_in


  def test_NamegenGetsWiped(self):
    """Test that driver-generated temp files can get wiped
    (no path canonicalization problems, etc.).
    This assumes that the default option is to not "-save-temps".
    """
    t_gen_out, t_gen_in = self.nameGenTemps()
    # Now wipe!
    driver_temps.TempFiles.wipe()
    # They are gone!
    self.assertFalse(os.path.exists(t_gen_out))
    self.assertFalse(os.path.exists(t_gen_in))


  def test_SaveTempsNotWiped(self):
    """Test that driver-generated temp files don't get wiped w/ "-save-temps".
    """
    env.push()
    env.set('SAVE_TEMPS', '1')
    t_gen_out, t_gen_in = self.nameGenTemps()
    # Now wipe!
    driver_temps.TempFiles.wipe()
    env.pop()
    # They are *not* gone.
    self.assertTrue(os.path.exists(t_gen_out))
    self.assertTrue(os.path.exists(t_gen_in))
    # However, tearDown() should clean them up since we registered them
    # with the test harness's own tempfiles list.


if __name__ == '__main__':
  unittest.main()
