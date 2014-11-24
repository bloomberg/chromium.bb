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

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    loading.LoadingMetric().AddResults(tab, results)


class ChromeProxyDataSaving(page_test.PageTest):
  """Chrome proxy data daving measurement."""
  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSaving, self).__init__(*args, **kwargs)
    self._metrics = metrics.ChromeProxyMetric()

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
    except exceptions.DevtoolsTargetCrashException, e:
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
    super(ChromeProxyHTTPFallbackProbeURL, self).__init__()

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyHTTPFallbackProbeURL,
          self).CustomizeBrowserOptions(options)
    # Use the test server probe URL which returns the response
    # body as specified by respBody.
    probe_url = GetResponseOverrideURL(
        respBody='not OK')
    options.AppendExtraBrowserArgs(
        '--data-reduction-proxy-probe-url=%s' % probe_url)

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
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-value=%s' % _FAKE_PROXY_AUTH_VALUE)

  def AddResults(self, tab, results):
    proxies = [
        _TEST_SERVER + ":80",
        self._metrics.effective_proxies['fallback'],
        self._metrics.effective_proxies['direct']]
    bad_proxies = [_TEST_SERVER + ":80", metrics.PROXY_SETTING_HTTP]
    self._metrics.AddResultsForHTTPFallback(tab, results, proxies, bad_proxies)


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

    proxies = [
        'nonexistent.googlezip.net:80',
        self._metrics.effective_proxies['fallback'],
        self._metrics.effective_proxies['direct']]
    self._metrics.VerifyProxyInfo(tab, proxies, proxies[:1])

  def AddResults(self, tab, results):
    self._metrics.AddResultsForHTTPToDirectFallback(tab, results)


class ChromeProxyExplicitBypass(ChromeProxyValidation):
  """Correctness measurement for explicit proxy bypasses.

  In this test, the configured proxy is the chromeproxy-test server which
  will send back a response without the expected Via header. Chrome is
  expected to use the fallback proxy and add the configured proxy to the
  bad proxy list.
  """

  def __init__(self):
    super(ChromeProxyExplicitBypass, self).__init__(
        restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    super(ChromeProxyExplicitBypass,
          self).CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--ignore-certificate-errors')
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-origin=http://%s' % _TEST_SERVER)
    options.AppendExtraBrowserArgs(
        '--spdy-proxy-auth-value=%s' % _FAKE_PROXY_AUTH_VALUE)

  def AddResults(self, tab, results):
    bad_proxies = [{
        'proxy': _TEST_SERVER + ':80',
        'retry_seconds_low': self._page.bypass_seconds_low,
        'retry_seconds_high': self._page.bypass_seconds_high
    }]
    if self._page.num_bypassed_proxies == 2:
      bad_proxies.append({
          'proxy': self._metrics.effective_proxies['fallback'],
          'retry_seconds_low': self._page.bypass_seconds_low,
          'retry_seconds_high': self._page.bypass_seconds_high
      })
    else:
      # Even if the test page only causes the primary proxy to be bypassed,
      # Chrome will attempt to fetch the favicon for the test server through
      # the data reduction proxy, which will cause a "block=0" bypass.
      bad_proxies.append({'proxy': self._metrics.effective_proxies['fallback']})

    self._metrics.AddResultsForExplicitBypass(tab, results, bad_proxies)


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
      raise page_test.MeasurementFailure(
          'Invalid page name (%s) in smoke. Page name must be one of:\n%s' % (
          self._page.name, page_to_metrics.keys()))
    for add_result in page_to_metrics[self._page.name]:
      add_result(tab, results)
