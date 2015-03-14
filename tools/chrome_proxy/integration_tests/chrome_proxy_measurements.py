# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import logging
import urlparse

from integration_tests import chrome_proxy_metrics as metrics
from metrics import loading
from telemetry.core import exceptions
from telemetry.page import page_test


class ChromeProxyLatency(page_test.PageTest):
  """Chrome proxy latency measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyLatency, self).__init__(*args, **kwargs)
    self._metrics = metrics.ChromeProxyMetric()

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    self._metrics.AddResultsForLatency(tab, results)


class ChromeProxyDataSaving(page_test.PageTest):
  """Chrome proxy data saving measurement."""
  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSaving, self).__init__(*args, **kwargs)
    self._metrics = metrics.ChromeProxyMetric()

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)
    self._metrics.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    self._metrics.Stop(page, tab)
    self._metrics.AddResultsForDataSaving(tab, results)


class ChromeProxyValidation(page_test.PageTest):
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

  def ValidateAndMeasurePage(self, page, tab, results):
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
      if self._expect_timeout:
        raise metrics.ChromeProxyMetricException, (
            'Timeout was expected, but did not occur')
    except exceptions.TimeoutException as e:
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


class ChromeProxyCorsBypass(ChromeProxyValidation):
  """Correctness measurement for bypass responses for CORS requests."""

  def __init__(self):
    super(ChromeProxyCorsBypass, self).__init__(restart_after_each_page=True)

  def ValidateAndMeasurePage(self, page, tab, results):
    # The test page sets window.xhrRequestCompleted to true when the XHR fetch
    # finishes.
    tab.WaitForJavaScriptExpression('window.xhrRequestCompleted', 300)
    super(ChromeProxyCorsBypass,
          self).ValidateAndMeasurePage(page, tab, results)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForCorsBypass(tab, results)


class ChromeProxyBlockOnce(ChromeProxyValidation):
  """Correctness measurement for block-once responses."""

  def __init__(self):
    super(ChromeProxyBlockOnce, self).__init__(restart_after_each_page=True)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForBlockOnce(tab, results)


class ChromeProxySafebrowsingOn(ChromeProxyValidation):
  """Correctness measurement for safebrowsing."""

  def __init__(self):
    super(ChromeProxySafebrowsingOn, self).__init__()

  def WillNavigateToPage(self, page, tab):
    super(ChromeProxySafebrowsingOn, self).WillNavigateToPage(page, tab)
    self._expect_timeout = True

  def AddResults(self, tab, results):
    self._metrics.AddResultsForSafebrowsingOn(tab, results)

class ChromeProxySafebrowsingOff(ChromeProxyValidation):
  """Correctness measurement for safebrowsing."""

  def __init__(self):
    super(ChromeProxySafebrowsingOff, self).__init__()

  def AddResults(self, tab, results):
    self._metrics.AddResultsForSafebrowsingOff(tab, results)

_FAKE_PROXY_AUTH_VALUE = 'aabbccdd3b7579186c1b0620614fdb1f0000ffff'
_TEST_SERVER = 'chromeproxy-test.appspot.com'
_TEST_SERVER_DEFAULT_URL = 'http://' + _TEST_SERVER + '/default'


# We rely on the chromeproxy-test server to facilitate some of the tests.
# The test server code is at <TBD location> and runs at _TEST_SERVER
#
# The test server allow request to override response status, headers, and
# body through query parameters. See GetResponseOverrideURL.
def GetResponseOverrideURL(url=_TEST_SERVER_DEFAULT_URL, respStatus=0,
                           respHeader="", respBody=""):
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
    super(ChromeProxyHTTPFallbackProbeURL, self).__init__(
        restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPFallbackProbeURL,
          self).CustomizeBrowserOptions(options)
    # Set the secure proxy check URL to the google.com favicon, which will be
    # interpreted as a secure proxy check failure since the response body is not
    # "OK". The google.com favicon is used because it will load reliably fast,
    # and there have been problems with chromeproxy-test.appspot.com being slow
    # and causing tests to flake.
    options.AppendExtraBrowserArgs(
        '--data-reduction-proxy-secure-proxy-check-url='
        'http://www.google.com/favicon.ico')

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHTTPFallback(tab, results)


class ChromeProxyHTTPFallbackViaHeader(ChromeProxyValidation):
  """Correctness measurement for proxy fallback.

  In this test, the configured proxy is the chromeproxy-test server which
  will send back a response without the expected Via header. Chrome is
  expected to use the fallback proxy and add the configured proxy to the
  bad proxy list.
  """

  def __init__(self):
    super(ChromeProxyHTTPFallbackViaHeader, self).__init__(
        restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPFallbackViaHeader,
          self).CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--ignore-certificate-errors')
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-origin=http://%s' % _TEST_SERVER)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHTTPFallback(tab, results)


class ChromeProxyClientVersion(ChromeProxyValidation):
  """Correctness measurement for version directives in Chrome-Proxy header.

  The test verifies that the version information provided in the Chrome-Proxy
  request header overrides any version, if specified, that is provided in the
  user agent string.
  """

  def __init__(self):
    super(ChromeProxyClientVersion, self).__init__()

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyClientVersion,
          self).CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--user-agent="Chrome/32.0.1700.99"')

  def AddResults(self, tab, results):
    self._metrics.AddResultsForClientVersion(tab, results)


class ChromeProxyClientType(ChromeProxyValidation):
  """Correctness measurement for Chrome-Proxy header client type directives."""

  def __init__(self):
    super(ChromeProxyClientType, self).__init__(restart_after_each_page=True)
    self._chrome_proxy_client_type = None

  def AddResults(self, tab, results):
    # Get the Chrome-Proxy client type from the first page in the page set, so
    # that the client type value can be used to determine which of the later
    # pages in the page set should be bypassed.
    if not self._chrome_proxy_client_type:
      client_type = self._metrics.GetClientTypeFromRequests(tab)
      if client_type:
        self._chrome_proxy_client_type = client_type

    self._metrics.AddResultsForClientType(tab,
                                          results,
                                          self._chrome_proxy_client_type,
                                          self._page.bypass_for_client_type)


class ChromeProxyLoFi(ChromeProxyValidation):
  """Correctness measurement for Lo-Fi in Chrome-Proxy header."""

  def __init__(self):
    super(ChromeProxyLoFi, self).__init__(restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyLoFi, self).CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-data-reduction-proxy-lo-fi')

  def AddResults(self, tab, results):
    self._metrics.AddResultsForLoFi(tab, results)


class ChromeProxyHTTPToDirectFallback(ChromeProxyValidation):
  """Correctness measurement for HTTP proxy fallback to direct."""

  def __init__(self):
    super(ChromeProxyHTTPToDirectFallback, self).__init__(
        restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPToDirectFallback,
          self).CustomizeBrowserOptions(options)
    # Set the primary proxy to something that will fail to be resolved so that
    # this test will run using the HTTP fallback proxy.
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-origin=http://nonexistent.googlezip.net')

  def WillNavigateToPage(self, page, tab):
    super(ChromeProxyHTTPToDirectFallback, self).WillNavigateToPage(page, tab)
    # Attempt to load a page through the nonexistent primary proxy in order to
    # cause a proxy fallback, and have this test run starting from the HTTP
    # fallback proxy.
    tab.Navigate(_TEST_SERVER_DEFAULT_URL)
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHTTPToDirectFallback(tab, results, _TEST_SERVER)


class ChromeProxyReenableAfterBypass(ChromeProxyValidation):
  """Correctness measurement for re-enabling proxies after bypasses.

  This test loads a page that causes all data reduction proxies to be bypassed
  for 1 to 5 minutes, then waits 5 minutes and verifies that the proxy is no
  longer bypassed.
  """

  def __init__(self):
    super(ChromeProxyReenableAfterBypass, self).__init__(
        restart_after_each_page=True)

  def AddResults(self, tab, results):
    self._metrics.AddResultsForReenableAfterBypass(
        tab, results, self._page.bypass_seconds_min,
        self._page.bypass_seconds_max)


class ChromeProxySmoke(ChromeProxyValidation):
  """Smoke measurement for basic chrome proxy correctness."""

  def __init__(self):
    super(ChromeProxySmoke, self).__init__(restart_after_each_page=True)

  def WillNavigateToPage(self, page, tab):
    super(ChromeProxySmoke, self).WillNavigateToPage(page, tab)

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
        }
    if not self._page.name in page_to_metrics:
      raise page_test.MeasurementFailure(
          'Invalid page name (%s) in smoke. Page name must be one of:\n%s' % (
          self._page.name, page_to_metrics.keys()))
    for add_result in page_to_metrics[self._page.name]:
      add_result(tab, results)
