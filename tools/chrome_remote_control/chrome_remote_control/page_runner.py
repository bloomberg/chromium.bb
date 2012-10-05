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

class PageState(object):
  def __init__(self):
    self.did_login = False

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
    archive_path = os.path.abspath(os.path.join(self.page_set.base_dir,
                                                self.page_set.archive_path))
    if browser.Browser.CanUseReplayServer(archive_path, options.record):
      extra_browser_args = replay_server.CHROME_FLAGS
    else:
      extra_browser_args = []
      from chrome_remote_control import browser_options
      if not browser_options.options_for_unittests:
        logging.warning('\n' + 80 * '*' + """\n
        The page set archive %s does not exist,
        benchmarking against live sites!
        Results won't be repeatable or comparable. To correct this, check out
        the archives from src-internal or create a new archive using --record.
        \n""" + 80 * '*' + '\n', os.path.relpath(archive_path))

    with possible_browser.Create(extra_browser_args) as b:
      if self.page_set.credentials_path:
        credentials_path = os.path.join(self.page_set.base_dir,
                                        self.page_set.credentials_path)
        b.credentials.credentials_path = credentials_path
      with b.CreateReplayServer(archive_path, options.record):
        with b.ConnectToNthTab(0) as tab:
          test.SetUpBrowser(b)
          for page in self.page_set:
            self._RunPage(options, page, tab, test, results)

  def _RunPage(self, options, page, tab, test, results):
    logging.info('Running %s' % page.url)

    page_state = PageState()
    try:
      self.PreparePage(page, tab, page_state, results)
    except Exception, ex:
      logging.error('Unexpected failure while running %s: %s',
                    page.url, traceback.format_exc())
      self.CleanUpPage(page, tab, page_state)
      raise

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
      self.CleanUpPage(page, tab, page_state)

  def Close(self):
    if self._server:
      self._server.Close()
      self._server = None

  def PreparePage(self, page, tab, page_state, results):
    parsed_url = urlparse.urlparse(page.url)
    if parsed_url[0] == 'file':
      path = os.path.join(self.page_set.base_dir,
                          parsed_url.netloc) # pylint: disable=E1101
      dirname, filename = os.path.split(path)
      if self._server and self._server.path != dirname:
        self._server.Close()
        self._server = None
      if not self._server:
        self._server = tab.browser.CreateTemporaryHTTPServer(dirname)
      page.url = self._server.UrlOf(filename)

    if page.credentials:
      page_state.did_login = tab.browser.credentials.LoginNeeded(
        tab, page.credentials)
      if not page_state.did_login:
        msg = 'Could not login to %s on %s' % (page.credentials, page.url)
        logging.info(msg)
        results.AddFailure(page, msg, "")

    tab.page.Navigate(page.url)
    # TODO(dtu): Detect HTTP redirects.
    if page.wait_time_after_navigate:
      # Wait for unpredictable redirects.
      time.sleep(page.wait_time_after_navigate)
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

  def CleanUpPage(self, page, tab, page_state): # pylint: disable=R0201
    if page.credentials and page_state.did_login:
      tab.browser.credentials.LoginNoLongerNeeded(tab, page.credentials)
