# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import time
import urlparse

class PageRunner(object):
  def __init__(self, page_set):
    self.page_set = page_set
    self._server = None

  def __del__(self):
    self.Close()

  def Close(self):
    if self._server:
      self._server.Close()
      self._server = None

  def PreparePage(self, page, tab):
    parsed_url = urlparse.urlparse(page.url)
    if parsed_url[0] == 'file':
      path = os.path.join(self.page_set.file_path, parsed_url[2])
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
