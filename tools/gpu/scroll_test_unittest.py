#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

import pylib.file_server
import pylib.scroll
import pylib.url_test

class ScrollTestUnitTest(unittest.TestCase):

  def runTest(self):
    test_file_path = os.path.join(os.path.dirname(__file__),
                                  'data', 'scrollable_page.html')

    result_count = 0
    with pylib.file_server.FileServer(test_file_path) as root_url:
      url = '%s/%s' % (root_url, os.path.basename(test_file_path))
      for result in pylib.url_test.UrlTest(pylib.scroll.Scroll, [url]):
        result_count += 1

    self.assertEqual(result_count, 1, 'Wrong number of test results.')
    self.assertEqual(result.GetUrl(), url, 'Did not navigate to right URL.')
    self.assertTrue(result.GetFirstPaintTime() < 1, 'The page load was slow.')
    self.assertTrue(result.GetFps(0) > 55, 'Scroll seemed kind of slow.')

if __name__ == '__main__':
  unittest.main()
