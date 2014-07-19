# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import logging
import urlparse

from integration_tests import chrome_proxy_metrics as metrics
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
    self._metrics = metrics.ChromeProxyMetric()

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
    self._metrics = metrics.ChromeProxyMetric()
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
    super(ChromeProxyHeaders, self).__init__(restart_after_each_page=True)

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


_FAKE_PROXY_AUTH_VALUE = 'aabbccdd3b7579186c1b0620614fdb1f0000ffff'
_TEST_SERVER = 'chromeproxy-test.appspot.com'
_TEST_SERVER_DEFAULT_URL = 'http://' + _TEST_SERVER + '/default'


# We rely on the chromeproxy-test server to facilitate some of the tests.
# The test server code is at <TBD location> and runs at _TEST_SERVER
#
# The test server allow request to override response status, headers, and
# body through query parameters. See GetResponseOverrideURL.
def GetResponseOverrideURL(url, respStatus=0, respHeader="", respBody=""):
  """ Compose the request URL with query parameters to override
  the chromeproxy-test server response.
  """

  queries = []
  if respStatus > 0:
    queries.append('respStatus=%d' % respStatus)
  if respHeader:
    queries.append('respHeader=%s' % base64.b64encode(respHeader))
  if respBody:
    queries.append('respBody=%s' % base64.b64encode(respBody))
  if len(queries) == 0:
    return url
  "&".join(queries)
  # url has query already
  if urlparse.urlparse(url).query:
    return url + '&' + "&".join(queries)
  else:
    return url + '?' + "&".join(queries)


class ChromeProxyHTTPFallbackProbeURL(ChromeProxyValidation):
  """Correctness measurement for proxy fallback.

  In this test, the probe URL does not return 'OK'. Chrome is expected
  to use the fallback proxy.
  """

  def __init__(self):
    super(ChromeProxyHTTPFallbackProbeURL, self).__init__()

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPFallbackProbeURL,
          self).CustomizeBrowserOptions(options)
    # Use the test server probe URL which returns the response
    # body as specified by respBody.
    probe_url = GetResponseOverrideURL(
        _TEST_SERVER_DEFAULT_URL,
        respBody='not OK')
    options.AppendExtraBrowserArgs(
        '--data-reduction-proxy-probe-url=%s' % probe_url)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHTTPFallback(tab, results)


# Depends on the fix of http://crbug.com/330342.
class ChromeProxyHTTPFallbackViaHeader(ChromeProxyValidation):
  """Correctness measurement for proxy fallback.

  In this test, the configured proxy is the chromeproxy-test server which
  will send back a response without the expected Via header. Chrome is
  expected to use the fallback proxy and add the configured proxy to the
  bad proxy list.
  """

  def __init__(self):
    super(ChromeProxyHTTPFallbackViaHeader, self).__init__()

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPFallbackViaHeader,
          self).CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--ignore-certificate-errors')
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-origin=http://%s' % _TEST_SERVER)
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-value=%s' % _FAKE_PROXY_AUTH_VALUE)

  def AddResults(self, tab, results):
    proxies = [
        _TEST_SERVER + ":80",
        self._metrics.effective_proxies['fallback'],
        self._metrics.effective_proxies['direct']]
    bad_proxies = [_TEST_SERVER + ":80"]
    self._metrics.AddResultsForHTTPFallback(tab, results, proxies, bad_proxies)


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
