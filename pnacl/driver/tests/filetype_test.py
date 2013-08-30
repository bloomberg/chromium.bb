#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from driver_env import env
import driver_test_utils
import driver_tools
import filetype


class TestFiletypeCache(driver_test_utils.DriverTesterCommon):
  def setUp(self):
    super(TestFiletypeCache, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)

  def getFakeLLAndBitcodeFile(self):
    with self.getTemp(suffix='.ll', close=False) as t:
      t.write('''
define i32 @main() {
  ret i32 0
}
''')
      t.close()
      with self.getTemp(suffix='.bc') as b:
        driver_tools.RunDriver('as', [t.name, '-o', b.name])
        return t, b

  def test_ll_bc_filetypes(self):
    if not driver_test_utils.CanRunHost():
      return
    ll, bc = self.getFakeLLAndBitcodeFile()
    self.assertTrue(filetype.FileType(ll.name) == 'll')
    self.assertTrue(filetype.FileType(bc.name) == 'po')
    self.assertFalse(filetype.IsLLVMBitcode(ll.name))
    self.assertTrue(filetype.IsLLVMBitcode(bc.name))

  def test_inplace_finalize(self):
    if not driver_test_utils.CanRunHost():
      return
    ll, bc = self.getFakeLLAndBitcodeFile()
    self.assertTrue(filetype.FileType(bc.name) == 'po')
    self.assertTrue(filetype.IsLLVMBitcode(bc.name))
    self.assertFalse(filetype.IsPNaClBitcode(bc.name))
    driver_tools.RunDriver('finalize', [bc.name])
    self.assertFalse(filetype.IsLLVMBitcode(bc.name))
    self.assertTrue(filetype.IsPNaClBitcode(bc.name))
    self.assertTrue(filetype.FileType(bc.name) == 'pexe')
