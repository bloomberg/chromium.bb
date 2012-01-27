# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cpp_util
import unittest

class CppUtilTest(unittest.TestCase):
  def testCppName(self):
    self.assertEquals('Permissions', cpp_util.CppName('permissions'))
    self.assertEquals('UpdateAllTheThings',
        cpp_util.CppName('updateAllTheThings'))
    self.assertEquals('Aa_Bb_Cc', cpp_util.CppName('aa.bb.cc'))

if __name__ == '__main__':
  unittest.main()
