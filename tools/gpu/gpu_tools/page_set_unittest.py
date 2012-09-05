# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import tempfile
import unittest

import page_set

set1="""
{"description": "hello",
 "pages": [
   {"url": "http://www.foo.com/"}
 ]
}
"""

class TestPageSet(unittest.TestCase):
  def testSet1(self):
    with tempfile.NamedTemporaryFile() as f:
      f.write(set1)
      f.flush()
      ps = page_set.PageSet()
      ps.LoadFromFile(f.name)

    self.assertEquals("hello", ps.description)
    self.assertEquals(1, len(ps.pages))
    self.assertEquals("http://www.foo.com/", ps.pages[0].url)
