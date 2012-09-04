# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import re
import time
import traceback

import chrome_remote_control

def UrlTest(browser, test_method, page_set):
  skipped_pages = []
  with browser.ConnectToNthTab(0) as tab:
    for page in page_set:
      try:
        logging.info('Loading %s...', page.url)
        tab.page.Navigate(page.url)
        # TODO(dtu): Detect HTTP redirects.
        time.sleep(2)  # Wait for unpredictable redirects.
        tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
        logging.info('Loaded page: %s', page.url)
      except chrome_remote_control.TimeoutException:
        logging.warning('Timed out while loading: %s', page.url)
        continue

      try:
        logging.info('Running test on %s...', page.url)
        result = test_method(page, tab)
        logging.info('Ran test:', page.url)
      except:
        logging.error('Failed on URL: %s', page.url)
        traceback.print_exc()
        skipped_pages.append(page)
        continue

      if not result:
        logging.warning('No result for URL: %s', page.url)
        skipped_pages.append(page)
        continue

      yield result

  if len(skipped_pages) > 0:
    logging.warning('Skipped pages: %s', skipped_page)
