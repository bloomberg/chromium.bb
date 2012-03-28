#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cpp_util
import unittest

class CppUtilTest(unittest.TestCase):
  def testClassname(self):
    self.assertEquals('Permissions', cpp_util.Classname('permissions'))
    self.assertEquals('UpdateAllTheThings',
        cpp_util.Classname('updateAllTheThings'))
    self.assertEquals('Aa_Bb_Cc', cpp_util.Classname('aa.bb.cc'))

if __name__ == '__main__':
  unittest.main()
