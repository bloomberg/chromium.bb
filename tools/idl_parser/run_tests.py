#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import sys
import unittest

if __name__ == '__main__':
  testlist = glob.glob('*_test.py')
  for testname in testlist:
    unittest.main(verbosity=2, module=testname[:-3])