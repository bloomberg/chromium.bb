# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import time
import traceback
import urlparse

from chrome_remote_control import browser
from chrome_remote_control import page_test
from chrome_remote_control import replay_server
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
    archive_path = os.path.abspath(os.path.join(os.path.dirname(
        self.page_set.file_path), self.page_set.archive_path))
    if browser.Browser.CanUseReplayServer(archive_path, options.record):
      extra_browser_args = replay_server.CHROME_FLAGS
    else:
      extra_browser_args = []
      logging.warning('\n' + 80 * '*' + """\n
      Page set archive does not exist, benchmarking against live sites!
      Results won't be repeatable or comparable. To correct this, check out the
      archives from src-internal or create a new archive using --record.
      \n""" + 80 * '*' + '\n')

    with possible_browser.Create(extra_browser_args) as b:
      with b.CreateReplayServer(archive_path, options.record):
        with b.ConnectToNthTab(0) as tab:
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
                          parsed_url.netloc) # pylint: disable=E1101
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
