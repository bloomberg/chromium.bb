# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import re
import traceback

import browser
import chrome_remote_control

def UrlTest(test_method, url_list):
  skipped_urls = []
  with browser.StartBrowser() as b:
    with b.ConnectToNthTab(0) as tab:
      for url in url_list:
        url = url.strip()
        if not url:
          continue
        if not re.match('(.+)://', url):
          url = 'http://%s' % url

        try:
          logging.info('Loading %s...', url)
          tab.LoadUrl(url)
          logging.info('Loaded page: %s', url)
        except chrome_remote_control.TimeoutException:
          logging.warning('Timed out while loading: %s', url)
          continue

        try:
          logging.info('Running test on %s...', url)
          result = test_method(tab)
          logging.info('Ran test:', url)
        except:
          logging.error('Failed on URL: %s', url)
          traceback.print_exc()
          skipped_urls.append(url)
          continue

        if not result:
          logging.warning('No result for URL: %s', url)
          skipped_urls.append(url)
          continue

        yield result

  if len(skipped_urls) > 0:
    logging.warning('Skipped URLs: %s', skipped_urls)
