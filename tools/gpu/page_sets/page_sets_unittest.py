# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

import gpu_tools.page_set
import page_sets

class PageSetsUnittest(unittest.TestCase):
  """Verfies that all the pagesets in this directory are syntactically valid."""

  @staticmethod
  def testPageSetsParseCorrectly():
    filenames = page_sets.GetAllPageSetFilenames()
    for filename in filenames:
      ps = gpu_tools.page_set.PageSet()
      ps.LoadFromFile(filename)
