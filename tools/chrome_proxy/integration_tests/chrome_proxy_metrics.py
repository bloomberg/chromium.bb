# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import logging
import os

from integration_tests import network_metrics
from telemetry.page import page_test
from telemetry.value import scalar


class ChromeProxyMetricException(page_test.MeasurementFailure):
  pass


CHROME_PROXY_VIA_HEADER = 'Chrome-Compression-Proxy'
CHROME_PROXY_VIA_HEADER_DEPRECATED = '1.1 Chrome Compression Proxy'

PROXY_SETTING_HTTPS = 'proxy.googlezip.net:443'
PROXY_SETTING_HTTPS_WITH_SCHEME = 'https://' + PROXY_SETTING_HTTPS
PROXY_SETTING_HTTP = 'compress.googlezip.net:80'
PROXY_SETTING_DIRECT = 'direct://'

# The default Chrome Proxy bypass time is a range from one to five mintues.
# See ProxyList::UpdateRetryInfoOnFallback in net/proxy/proxy_list.cc.
DEFAULT_BYPASS_MIN_SECONDS = 60
DEFAULT_BYPASS_MAX_SECONDS = 5 * 60

def GetProxyInfoFromNetworkInternals(tab, url='chrome://net-internals#proxy'):
  tab.Navigate(url)
  with open(os.path.join(os.path.dirname(__file__),
                         'chrome_proxy_metrics.js')) as f:
    js = f.read()
    tab.ExecuteJavaScript(js)
  tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
  info = tab.EvaluateJavaScript('window.__getChromeProxyInfo()')
  return info


def ProxyRetryTimeInRange(retry_time, low, high, grace_seconds=30):
  return (retry_time >= low and
          (retry_time < high + datetime.timedelta(seconds=grace_seconds)))


class ChromeProxyResponse(network_metrics.HTTPResponse):
  """ Represents an HTTP response from a timeleine event."""
  def __init__(self, event):
    super(ChromeProxyResponse, self).__init__(event)

  def ShouldHaveChromeProxyViaHeader(self):
    resp = self.response
    # Ignore https and data url
    if resp.url.startswith('https') or resp.url.startswith('data:'):
      return False
    # Ignore 304 Not Modified and cache hit.
    if resp.status == 304 or resp.served_from_cache:
      return False
    # Ignore invalid responses that don't have any header. Log a warning.
    if not resp.headers:
      logging.warning('response for %s does not any have header '
                      '(refer=%s, status=%s)',
                      resp.url, resp.GetHeader('Referer'), resp.status)
      return False
    return True

  def HasChromeProxyViaHeader(self):
    via_header = self.response.GetHeader('Via')
    if not via_header:
      return False
    vias = [v.strip(' ') for v in via_header.split(',')]
    # The Via header is valid if it is the old format or the new format
    # with 4-character version prefix, for example,
    # "1.1 Chrome-Compression-Proxy".
    return (CHROME_PROXY_VIA_HEADER_DEPRECATED in vias or
            any(v[4:] == CHROME_PROXY_VIA_HEADER for v in vias))

  def IsValidByViaHeader(self):
    return (not self.ShouldHaveChromeProxyViaHeader() or
            self.HasChromeProxyViaHeader())

  def IsSafebrowsingResponse(self):
    if (self.response.status == 307 and
        self.response.GetHeader('X-Malware-Url') == '1' and
        self.IsValidByViaHeader() and
        self.response.GetHeader('Location') == self.response.url):
      return True
    return False


class ChromeProxyMetric(network_metrics.NetworkMetric):
  """A Chrome proxy timeline metric."""

  def __init__(self):
    super(ChromeProxyMetric, self).__init__()
    self.compute_data_saving = True
    self.effective_proxies = {
        "proxy": PROXY_SETTING_HTTPS_WITH_SCHEME,
        "fallback": PROXY_SETTING_HTTP,
        "direct": PROXY_SETTING_DIRECT,
        }

  def SetEvents(self, events):
    """Used for unittest."""
    self._events = events

  def ResponseFromEvent(self, event):
    return ChromeProxyResponse(event)

  def AddResults(self, tab, results):
    raise NotImplementedError

  def AddResultsForDataSaving(self, tab, results):
    resources_via_proxy = 0
    resources_from_cache = 0
    resources_direct = 0

    super(ChromeProxyMetric, self).AddResults(tab, results)
    for resp in self.IterResponses(tab):
      if resp.response.served_from_cache:
        resources_from_cache += 1
      if resp.HasChromeProxyViaHeader():
        resources_via_proxy += 1
      else:
        resources_direct += 1

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'resources_via_proxy', 'count',
        resources_via_proxy))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'resources_from_cache', 'count',
        resources_from_cache))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'resources_direct', 'count', resources_direct))

  def AddResultsForHeaderValidation(self, tab, results):
    via_count = 0
    bypass_count = 0
    for resp in self.IterResponses(tab):
      if resp.IsValidByViaHeader():
        via_count += 1
      elif tab and self.IsProxyBypassed(tab):
        logging.warning('Proxy bypassed for %s', resp.response.url)
        bypass_count += 1
      else:
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Via header (%s) is not valid (refer=%s, status=%d)' % (
                r.url, r.GetHeader('Via'), r.GetHeader('Referer'), r.status))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'checked_via_header', 'count', via_count))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'request_bypassed', 'count', bypass_count))

  def IsProxyBypassed(self, tab):
    """ Returns True if all configured proxies are bypassed."""
    info = GetProxyInfoFromNetworkInternals(tab)
    if not info['enabled']:
      raise ChromeProxyMetricException, (
          'Chrome proxy should be enabled. proxy info: %s' % info)

    bad_proxies = [str(p['proxy']) for p in info['badProxies']].sort()
    proxies = [self.effective_proxies['proxy'],
               self.effective_proxies['fallback']].sort()
    return bad_proxies == proxies

  @staticmethod
  def VerifyBadProxies(
      badProxies, expected_proxies,
      retry_seconds_low = DEFAULT_BYPASS_MIN_SECONDS,
      retry_seconds_high = DEFAULT_BYPASS_MAX_SECONDS):
    """Verify the bad proxy list and their retry times are expected. """
    if not badProxies or (len(badProxies) != len(expected_proxies)):
      return False

    # Check all expected proxies.
    proxies = [p['proxy'] for p in badProxies]
    expected_proxies.sort()
    proxies.sort()
    if not expected_proxies == proxies:
      raise ChromeProxyMetricException, (
          'Bad proxies: got %s want %s' % (
              str(badProxies), str(expected_proxies)))

    # Check retry time
    for p in badProxies:
      retry_time_low = (datetime.datetime.now() +
                        datetime.timedelta(seconds=retry_seconds_low))
      retry_time_high = (datetime.datetime.now() +
                        datetime.timedelta(seconds=retry_seconds_high))
      got_retry_time = datetime.datetime.fromtimestamp(int(p['retry'])/1000)
      if not ProxyRetryTimeInRange(
          got_retry_time, retry_time_low, retry_time_high):
        raise ChromeProxyMetricException, (
            'Bad proxy %s retry time (%s) should be within range (%s-%s).' % (
                p['proxy'], str(got_retry_time), str(retry_time_low),
                str(retry_time_high)))
    return True

  def AddResultsForBypass(self, tab, results):
    bypass_count = 0
    for resp in self.IterResponses(tab):
      if resp.HasChromeProxyViaHeader():
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Should not have Via header (%s) (refer=%s, status=%d)' % (
                r.url, r.GetHeader('Via'), r.GetHeader('Referer'), r.status))
      bypass_count += 1

    if tab:
      info = GetProxyInfoFromNetworkInternals(tab)
      if not info['enabled']:
        raise ChromeProxyMetricException, (
            'Chrome proxy should be enabled. proxy info: %s' % info)
      self.VerifyBadProxies(
          info['badProxies'],
          [self.effective_proxies['proxy'],
           self.effective_proxies['fallback']])

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'bypass', 'count', bypass_count))

  def AddResultsForSafebrowsing(self, tab, results):
    count = 0
    safebrowsing_count = 0
    for resp in self.IterResponses(tab):
      count += 1
      if resp.IsSafebrowsingResponse():
        safebrowsing_count += 1
      else:
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Not a valid safe browsing response.\n'
            'Reponse: status=(%d, %s)\nHeaders:\n %s' % (
                r.url, r.status, r.status_text, r.headers))
    if count == safebrowsing_count:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'safebrowsing', 'boolean', True))
    else:
      raise ChromeProxyMetricException, (
          'Safebrowsing failed (count=%d, safebrowsing_count=%d)\n' % (
              count, safebrowsing_count))

  def AddResultsForHTTPFallback(
      self, tab, results, expected_proxies=None, expected_bad_proxies=None):
    info = GetProxyInfoFromNetworkInternals(tab)
    if not 'enabled' in info or not info['enabled']:
      raise ChromeProxyMetricException, (
          'Chrome proxy should be enabled. proxy info: %s' % info)

    if not expected_proxies:
      expected_proxies = [self.effective_proxies['fallback'],
                          self.effective_proxies['direct']]
    if not expected_bad_proxies:
      expected_bad_proxies = []

    proxies = info['proxies']
    if proxies != expected_proxies:
      raise ChromeProxyMetricException, (
          'Wrong effective proxies (%s). Expect: "%s"' % (
          str(proxies), str(expected_proxies)))

    bad_proxies = []
    if 'badProxies' in info and info['badProxies']:
      bad_proxies = [p['proxy'] for p in info['badProxies']
                     if 'proxy' in p and p['proxy']]
    if bad_proxies != expected_bad_proxies:
      raise ChromeProxyMetricException, (
          'Wrong bad proxies (%s). Expect: "%s"' % (
          str(bad_proxies), str(expected_bad_proxies)))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'http_fallback', 'boolean', True))
