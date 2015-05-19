# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from common import chrome_proxy_metrics as metrics
from telemetry.core import exceptions
from telemetry.page import page_test

class ChromeProxyValidation(page_test.PageTest):
  """Base class for all chrome proxy correctness measurements."""

  # Value of the extra via header. |None| if no extra via header is expected.
  extra_via_header = None

  def __init__(self, restart_after_each_page=False, metrics=None):
    super(ChromeProxyValidation, self).__init__(
        needs_browser_restart_after_each_page=restart_after_each_page)
    self._metrics = metrics
    self._page = None

  def CustomizeBrowserOptions(self, options):
    # Enable the chrome proxy (data reduction proxy).
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)
    assert self._metrics
    self._metrics.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._page = page
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    assert self._metrics
    self._metrics.Stop(page, tab)
    if ChromeProxyValidation.extra_via_header:
      self._metrics.AddResultsForExtraViaHeader(
          tab, results, ChromeProxyValidation.extra_via_header)
    self.AddResults(tab, results)

  def AddResults(self, tab, results):
    raise NotImplementedError

  def StopBrowserAfterPage(self, browser, page):  # pylint: disable=W0613
    if hasattr(page, 'restart_after') and page.restart_after:
      return True
    return False
