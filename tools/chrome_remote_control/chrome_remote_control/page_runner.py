# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import time
import traceback
import urlparse

from chrome_remote_control import page_test
from chrome_remote_control import util

class PageRunner(object):
  """Runs a given test against a given test."""
  def __init__(self, page_set):
    self.page_set = page_set
    self._server = None

  def __del__(self):
    assert self._server == None

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def Run(self, options, possible_browser, test, results):
    with possible_browser.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        for page in self.page_set:
          self._RunPage(options, page, tab, test, results)

  def _RunPage(self, options, page, tab, test, results):
    logging.info('Running %s' % page.url)

    try:
      self.PreparePage(page, tab)
    except Exception, ex:
      logging.error('Unexpected failure while running %s: %s',
                    page.url, traceback.format_exc())
      raise
    finally:
      self.CleanUpPage()

    try:
      test.Run(options, page, tab, results)
    except page_test.Failure, ex:
      logging.info('%s: %s', ex, page.url)
      results.AddFailure(page, ex, traceback.format_exc())
      return
    except util.TimeoutException, ex:
      logging.warning('Timed out while running %s', page.url)
      results.AddFailure(page, ex, traceback.format_exc())
      return
    except Exception, ex:
      logging.error('Unexpected failure while running %s: %s',
                    page.url, traceback.format_exc())
      raise
    finally:
      self.CleanUpPage()

  def Close(self):
    if self._server:
      self._server.Close()
      self._server = None

  def PreparePage(self, page, tab):
    parsed_url = urlparse.urlparse(page.url)
    if parsed_url[0] == 'file':
      path = os.path.join(os.path.dirname(self.page_set.file_path),
                          parsed_url[1])
      dirname, filename = os.path.split(path)
      if self._server and self._server.path != dirname:
        self._server.Close()
        self._server = None
      if not self._server:
        self._server = tab.browser.CreateTemporaryHTTPServer(dirname)
      page.url = self._server.UrlOf(filename)

    tab.page.Navigate(page.url)
    # TODO(dtu): Detect HTTP redirects.
    if page.wait_time_after_navigate:
      # Wait for unpredictable redirects.
      time.sleep(page.wait_time_after_navigate)
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

  def CleanUpPage(self):
    pass
