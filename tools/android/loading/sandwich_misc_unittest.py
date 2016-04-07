# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import urlparse

import sandwich_misc


LOADING_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(LOADING_DIR, 'testdata')


class SandwichMiscTest(unittest.TestCase):
  _TRACE_PATH = os.path.join(TEST_DATA_DIR, 'scanner_vs_parser.trace')

  def GetResourceUrl(self, path):
    return urlparse.urljoin('http://l/', path)

  def testRedirectedMainWhitelisting(self):
    urls_set_ref = set([self.GetResourceUrl('./')])
    urls_set = sandwich_misc.ExtractDiscoverableUrls(
        self._TRACE_PATH, sandwich_misc.REDIRECTED_MAIN_DISCOVERER)
    self.assertEquals(urls_set_ref, urls_set)

  def testParserDiscoverableWhitelisting(self):
    urls_set_ref = set([self.GetResourceUrl('./'),
                        self.GetResourceUrl('0.png'),
                        self.GetResourceUrl('1.png')])
    urls_set = sandwich_misc.ExtractDiscoverableUrls(
        self._TRACE_PATH, sandwich_misc.PARSER_DISCOVERER)
    self.assertEquals(urls_set_ref, urls_set)

  def testHTMLPreloadScannerWhitelisting(self):
    urls_set_ref = set([self.GetResourceUrl('./'),
                        self.GetResourceUrl('0.png')])
    urls_set = sandwich_misc.ExtractDiscoverableUrls(
        self._TRACE_PATH, sandwich_misc.HTML_PRELOAD_SCANNER_DISCOVERER)
    self.assertEquals(urls_set_ref, urls_set)


if __name__ == '__main__':
  unittest.main()
