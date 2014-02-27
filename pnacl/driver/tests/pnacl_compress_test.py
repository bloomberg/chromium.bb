#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests pnacl-compress.

This tests that pnacl-compress fixpoints, and is immutable after that.
"""

from driver_env import env
import driver_test_utils
import driver_tools
import pathtools

class TestPnaclCompress(driver_test_utils.DriverTesterCommon):
  def setUp(self):
    super(TestPnaclCompress, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)
    self.platform = driver_test_utils.GetPlatformToTest()

  def getFakePexe(self):
    with self.getTemp(suffix='.ll', close=False) as t:
      with self.getTemp(suffix='.pexe') as p:
        t.write('''
define i32 @main() {
  ret i32 0
}
''')
        t.close()
        driver_tools.RunDriver('as', [t.name, '-o', p.name])
        driver_tools.RunDriver('finalize', [p.name])
        return p

  def test_multiple_compresses(self):
    pexe = self.getFakePexe()
    init_size = pathtools.getsize(pexe.name)
    driver_tools.RunDriver('compress', [pexe.name])
    shrunk_size = pathtools.getsize(pexe.name)
    self.assertTrue(init_size >= shrunk_size)
    driver_tools.RunDriver('compress', [pexe.name])
    self.assertTrue(pathtools.getsize(pexe.name) == shrunk_size)
