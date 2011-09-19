#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.data_pack'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))
import unittest

from grit.format import data_pack

class FormatDataPackUnittest(unittest.TestCase):
  def testWriteDataPack(self):
    expected = (
        '\x04\x00\x00\x00'                  # header(version
        '\x04\x00\x00\x00'                  #        no. entries,
        '\x01'                              #        encoding)
        '\x01\x00\x27\x00\x00\x00'          # index entry 1
        '\x04\x00\x27\x00\x00\x00'          # index entry 4
        '\x06\x00\x33\x00\x00\x00'          # index entry 6
        '\x0a\x00\x3f\x00\x00\x00'          # index entry 10
        '\x00\x00\x3f\x00\x00\x00'          # extra entry for the size of last
        'this is id 4this is id 6')         # data
    input = { 1: "", 4: "this is id 4", 6: "this is id 6", 10: "" }
    output = data_pack.DataPack.WriteDataPackToString(input, data_pack.UTF8)
    self.failUnless(output == expected)


if __name__ == '__main__':
  unittest.main()
