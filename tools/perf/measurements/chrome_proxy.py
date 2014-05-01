# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from metrics import chrome_proxy
from metrics import loading
from telemetry.core import util
from telemetry.page import page_measurement


class ChromeProxyLatency(page_measurement.PageMeasurement):
  """Chrome proxy latency measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyLatency, self).__init__(*args, **kwargs)

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)

  def MeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    loading.LoadingMetric().AddResults(tab, results)


class ChromeProxyDataSaving(page_measurement.PageMeasurement):
  """Chrome proxy data daving measurement."""
  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSaving, self).__init__(*args, **kwargs)
    self._metrics = chrome_proxy.ChromeProxyMetric()

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)
    self._metrics.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    self._metrics.Stop(page, tab)
    self._metrics.AddResultsForDataSaving(tab, results)


class ChromeProxyValidation(page_measurement.PageMeasurement):
  """Base class for all chrome proxy correctness measurements."""

  def __init__(self, restart_after_each_page=False):
    super(ChromeProxyValidation, self).__init__(
        needs_browser_restart_after_each_page=restart_after_each_page)
    self._metrics = chrome_proxy.ChromeProxyMetric()
    self._page = None
    # Whether a timeout exception is expected during the test.
    self._expect_timeout = False

  def CustomizeBrowserOptions(self, options):
    # Enable the chrome proxy (data reduction proxy).
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)
    assert self._metrics
    self._metrics.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    self._page = page
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    assert self._metrics
    self._metrics.Stop(page, tab)
    self.AddResults(tab, results)

  def AddResults(self, tab, results):
    raise NotImplementedError

  def StopBrowserAfterPage(self, browser, page):  # pylint: disable=W0613
    if hasattr(page, 'restart_after') and page.restart_after:
      return True
    return False

  def RunNavigateSteps(self, page, tab):
    # The redirect from safebrowsing causes a timeout. Ignore that.
    try:
      super(ChromeProxyValidation, self).RunNavigateSteps(page, tab)
    except util.TimeoutException, e:
      if self._expect_timeout:
        logging.warning('Navigation timeout on page %s',
                        page.name if page.name else page.url)
      else:
        raise e


class ChromeProxyHeaders(ChromeProxyValidation):
  """Correctness measurement for response headers."""

  def __init__(self):
    super(ChromeProxyHeaders, self).__init__()

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHeaderValidation(tab, results)


class ChromeProxyBypass(ChromeProxyValidation):
  """Correctness measurement for bypass responses."""

  def __init__(self):
    super(ChromeProxyBypass, self).__init__(restart_after_each_page=True)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForBypass(tab, results)


class ChromeProxySafebrowsing(ChromeProxyValidation):
  """Correctness measurement for safebrowsing."""

  def __init__(self):
    super(ChromeProxySafebrowsing, self).__init__()

  def WillNavigateToPage(self, page, tab):
    super(ChromeProxySafebrowsing, self).WillNavigateToPage(page, tab)
    self._expect_timeout = True

  def AddResults(self, tab, results):
    self._metrics.AddResultsForSafebrowsing(tab, results)


class ChromeProxySmoke(ChromeProxyValidation):
  """Smoke measurement for basic chrome proxy correctness."""

  def __init__(self):
    super(ChromeProxySmoke, self).__init__()

  def WillNavigateToPage(self, page, tab):
    super(ChromeProxySmoke, self).WillNavigateToPage(page, tab)
    if page.name == 'safebrowsing':
      self._expect_timeout = True

  def AddResults(self, tab, results):
    # Map a page name to its AddResults func.
    page_to_metrics = {
        'header validation': [self._metrics.AddResultsForHeaderValidation],
        'compression: image': [
            self._metrics.AddResultsForHeaderValidation,
            self._metrics.AddResultsForDataSaving,
            ],
        'compression: javascript': [
            self._metrics.AddResultsForHeaderValidation,
            self._metrics.AddResultsForDataSaving,
            ],
        'compression: css': [
            self._metrics.AddResultsForHeaderValidation,
            self._metrics.AddResultsForDataSaving,
            ],
        'bypass': [self._metrics.AddResultsForBypass],
        'safebrowsing': [self._metrics.AddResultsForSafebrowsing],
        }
    if not self._page.name in page_to_metrics:
      raise page_measurement.MeasurementFailure(
          'Invalid page name (%s) in smoke. Page name must be one of:\n%s' % (
          self._page.name, page_to_metrics.keys()))
    for add_result in page_to_metrics[self._page.name]:
      add_result(tab, results)
