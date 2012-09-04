# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import csv
import os
import unittest
import StringIO

import file_server
import page_set
import page_scroller
import tempfile

import chrome_remote_control.browser_options

class ScrollTestUnitTest(unittest.TestCase):

  def testBasicFunctionality(self):
    scrollable_page_path = os.path.join(
        os.path.dirname(__file__),
        '..', 'unittest_data', 'scrollable_page.html')

    result_count = 0
    output = StringIO.StringIO()
    with file_server.FileServer(scrollable_page_path) as server:
      url = '%s/%s' % (server.url, os.path.basename(scrollable_page_path))

      ps = page_set.PageSet()
      ps.pages.append(page_set.Page(url))
      options = chrome_remote_control.browser_options.options_for_unittests
      page_scroller.Run(output, options, ps)

    output.seek(0)
    raw_rows = list(csv.reader(output))

    header = raw_rows[0]
    results = raw_rows[1:]
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0][0], url, 'Did not navigate to right URL.')
