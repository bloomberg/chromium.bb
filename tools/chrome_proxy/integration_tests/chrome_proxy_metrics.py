# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import logging
import os

from integration_tests import network_metrics
from telemetry.core import util
from telemetry.page import page_test
from telemetry.value import scalar


class ChromeProxyMetricException(page_test.MeasurementFailure):
  pass


CHROME_PROXY_VIA_HEADER = 'Chrome-Compression-Proxy'
CHROME_PROXY_VIA_HEADER_DEPRECATED = '1.1 Chrome Compression Proxy'

PROXY_SETTING_HTTPS = 'proxy.googlezip.net:443'
PROXY_SETTING_HTTPS_WITH_SCHEME = 'https://' + PROXY_SETTING_HTTPS
PROXY_DEV_SETTING_HTTP = 'proxy-xt.googlezip.net:80'
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

  # Sometimes, the proxy information on net_internals#proxy is slow to come up.
  # In order to prevent this from causing tests to flake frequently, wait for
  # up to 10 seconds for this information to appear.
  def IsDataReductionProxyEnabled():
    info = tab.EvaluateJavaScript('window.__getChromeProxyInfo()')
    return info['enabled']

  util.WaitFor(IsDataReductionProxyEnabled, 10)
  info = tab.EvaluateJavaScript('window.__getChromeProxyInfo()')
  return info


def ProxyRetryTimeInRange(retry_time, low, high, grace_seconds=30):
  return (retry_time >= low - datetime.timedelta(seconds=grace_seconds) and
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

  def GetChromeProxyClientType(self):
    """Get the client type directive from the Chrome-Proxy request header.

    Returns:
        The client type directive from the Chrome-Proxy request header for the
        request that lead to this response. For example, if the request header
        "Chrome-Proxy: c=android" is present, then this method would return
        "android". Returns None if no client type directive is present.
    """
    if 'Chrome-Proxy' not in self.response.request_headers:
      return None

    chrome_proxy_request_header = self.response.request_headers['Chrome-Proxy']
    values = [v.strip() for v in chrome_proxy_request_header.split(',')]
    for value in values:
      kvp = value.split('=', 1)
      if len(kvp) == 2 and kvp[0].strip() == 'c':
        return kvp[1].strip()
    return None


class ChromeProxyMetric(network_metrics.NetworkMetric):
  """A Chrome proxy timeline metric."""

  def __init__(self):
    super(ChromeProxyMetric, self).__init__()
    self.compute_data_saving = True
    self.effective_proxies = {
        "proxy": PROXY_SETTING_HTTPS_WITH_SCHEME,
        "proxy-dev": PROXY_DEV_SETTING_HTTP,
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
      else:
        bypassed, _ = self.IsProxyBypassed(tab)
        if tab and bypassed:
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

  def AddResultsForClientVersion(self, tab, results):
    for resp in self.IterResponses(tab):
      r = resp.response
      if resp.response.status != 200:
        raise ChromeProxyMetricException, ('%s: Response is not 200: %d' %
                                           (r.url, r.status))
      if not resp.IsValidByViaHeader():
        raise ChromeProxyMetricException, ('%s: Response missing via header' %
                                           (r.url))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'version_test', 'count', 1))

  def GetClientTypeFromRequests(self, tab):
    """Get the Chrome-Proxy client type value from requests made in this tab.

    Returns:
        The client type value from the first request made in this tab that
        specifies a client type in the Chrome-Proxy request header. See
        ChromeProxyResponse.GetChromeProxyClientType for more details about the
        Chrome-Proxy client type. Returns None if none of the requests made in
        this tab specify a client type.
    """
    for resp in self.IterResponses(tab):
      client_type = resp.GetChromeProxyClientType()
      if client_type:
        return client_type
    return None

  def AddResultsForClientType(self, tab, results, client_type,
                              bypass_for_client_type):
    via_count = 0
    bypass_count = 0

    for resp in self.IterResponses(tab):
      if resp.HasChromeProxyViaHeader():
        via_count += 1
        if client_type.lower() == bypass_for_client_type.lower():
          raise ChromeProxyMetricException, (
              '%s: Response for client of type "%s" has via header, but should '
              'be bypassed.' % (
                  resp.response.url, bypass_for_client_type, client_type))
      elif resp.ShouldHaveChromeProxyViaHeader():
        bypass_count += 1
        if client_type.lower() != bypass_for_client_type.lower():
          raise ChromeProxyMetricException, (
              '%s: Response missing via header. Only "%s" clients should '
              'bypass for this page, but this client is "%s".' % (
                  resp.response.url, bypass_for_client_type, client_type))

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'via', 'count', via_count))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'bypass', 'count', bypass_count))

  def ProxyListForDev(self, proxies):
    return [self.effective_proxies['proxy-dev']
            if proxy == self.effective_proxies['proxy']
            else proxy for proxy in proxies]

  def IsProxyBypassed(self, tab):
    """Get whether all configured proxies are bypassed.

    Returns:
        A tuple of the form (boolean, string list). If all configured proxies
        are bypassed, then the return value will be (True, bypassed proxies).
        Otherwise, the return value will be (False, empty list).
    """
    if not tab:
      return False, []

    info = GetProxyInfoFromNetworkInternals(tab)
    if not info['enabled']:
      raise ChromeProxyMetricException, (
          'Chrome proxy should be enabled. proxy info: %s' % info)
    if not info['badProxies']:
      return False, []

    bad_proxies = [str(p['proxy']) for p in info['badProxies']]
    # Expect all but the "direct://" proxy to be bad.
    expected_bad_proxies = info['proxies'][:-1]
    if set(bad_proxies) == set(expected_bad_proxies):
      return True, expected_bad_proxies
    return False, []

  def VerifyBadProxies(self, bad_proxies, expected_bad_proxies):
    """Verify the bad proxy list and their retry times are expected.

    Args:
        bad_proxies: the list of actual bad proxies and their retry times.
        expected_bad_proxies: a list of dictionaries in the form:

            {'proxy': <proxy origin>,
             'retry_seconds_low': <minimum bypass duration in seconds>,
             'retry_seconds_high': <maximum bypass duration in seconds>}

            If an element in the list is missing either the 'retry_seconds_low'
            entry or the 'retry_seconds_high' entry, the default bypass minimum
            and maximum durations respectively will be used for that element.
    """
    if not bad_proxies:
      bad_proxies = []
    if len(bad_proxies) != len(expected_bad_proxies):
      raise ChromeProxyMetricException, (
          'Actual and expected bad proxy lists should match: %s vs. %s' % (
              str(bad_proxies), str(expected_bad_proxies)))

    # Check that each of the proxy origins and retry times match.
    for expected_bad_proxy in expected_bad_proxies:
      # Find a matching actual bad proxy origin, allowing for the proxy-dev
      # origin in the place of the HTTPS proxy origin.
      bad_proxy = None
      for actual_proxy in bad_proxies:
        if (expected_bad_proxy['proxy'] == actual_proxy['proxy'] or (
            self.effective_proxies['proxy-dev'] == actual_proxy['proxy'] and
            self.effective_proxies['proxy'] == expected_bad_proxy['proxy'])):
          bad_proxy = actual_proxy
          break
      if not bad_proxy:
        raise ChromeProxyMetricException, (
            'No match for expected bad proxy %s - actual and expected bad '
            'proxies should match: %s vs. %s' % (expected_bad_proxy['proxy'],
                                                 str(bad_proxies),
                                                 str(expected_bad_proxies)))

      # Check that the retry times match.
      retry_seconds_low = expected_bad_proxy.get('retry_seconds_low',
                                                 DEFAULT_BYPASS_MIN_SECONDS)
      retry_seconds_high = expected_bad_proxy.get('retry_seconds_high',
                                                  DEFAULT_BYPASS_MAX_SECONDS)
      retry_time_low = (datetime.datetime.now() +
                        datetime.timedelta(seconds=retry_seconds_low))
      retry_time_high = (datetime.datetime.now() +
                         datetime.timedelta(seconds=retry_seconds_high))
      got_retry_time = datetime.datetime.fromtimestamp(
          int(bad_proxy['retry'])/1000)
      if not ProxyRetryTimeInRange(
          got_retry_time, retry_time_low, retry_time_high):
        raise ChromeProxyMetricException, (
            'Bad proxy %s retry time (%s) should be within range (%s-%s).' % (
                bad_proxy['proxy'], str(got_retry_time), str(retry_time_low),
                str(retry_time_high)))

  def VerifyAllProxiesBypassed(self, tab):
    """Verify that all proxies are bypassed for 1 to 5 minutes."""
    if tab:
      info = GetProxyInfoFromNetworkInternals(tab)
      if not info['enabled']:
        raise ChromeProxyMetricException, (
            'Chrome proxy should be enabled. proxy info: %s' % info)
      is_bypassed, expected_bad_proxies = self.IsProxyBypassed(tab)
      if not is_bypassed:
        raise ChromeProxyMetricException, (
            'Chrome proxy should be bypassed. proxy info: %s' % info)
      self.VerifyBadProxies(info['badProxies'],
                            [{'proxy': p} for p in expected_bad_proxies])

  def AddResultsForBypass(self, tab, results):
    bypass_count = 0
    for resp in self.IterResponses(tab):
      if resp.HasChromeProxyViaHeader():
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Should not have Via header (%s) (refer=%s, status=%d)' % (
                r.url, r.GetHeader('Via'), r.GetHeader('Referer'), r.status))
      bypass_count += 1

    self.VerifyAllProxiesBypassed(tab)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'bypass', 'count', bypass_count))

  def AddResultsForCorsBypass(self, tab, results):
    eligible_response_count = 0
    bypass_count = 0
    bypasses = {}
    for resp in self.IterResponses(tab):
      logging.warn('got a resource %s' % (resp.response.url))

    for resp in self.IterResponses(tab):
      if resp.ShouldHaveChromeProxyViaHeader():
        eligible_response_count += 1
        if not resp.HasChromeProxyViaHeader():
          bypass_count += 1
        elif resp.response.status == 502:
          bypasses[resp.response.url] = 0

    for resp in self.IterResponses(tab):
      if resp.ShouldHaveChromeProxyViaHeader():
        if not resp.HasChromeProxyViaHeader():
          if resp.response.status == 200:
            if (bypasses.has_key(resp.response.url)):
              bypasses[resp.response.url] = bypasses[resp.response.url] + 1

    for url in bypasses:
      if bypasses[url] == 0:
        raise ChromeProxyMetricException, (
              '%s: Got a 502 without a subsequent 200' % (url))
      elif bypasses[url] > 1:
        raise ChromeProxyMetricException, (
              '%s: Got a 502 and multiple 200s: %d' % (url, bypasses[url]))
    if bypass_count == 0:
      raise ChromeProxyMetricException, (
          'At least one response should be bypassed. '
          '(eligible_response_count=%d, bypass_count=%d)\n' % (
              eligible_response_count, bypass_count))

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'cors_bypass', 'count', bypass_count))

  def AddResultsForBlockOnce(self, tab, results):
    eligible_response_count = 0
    bypass_count = 0
    for resp in self.IterResponses(tab):
      if resp.ShouldHaveChromeProxyViaHeader():
        eligible_response_count += 1
        if not resp.HasChromeProxyViaHeader():
          bypass_count += 1

    if tab:
      info = GetProxyInfoFromNetworkInternals(tab)
      if not info['enabled']:
        raise ChromeProxyMetricException, (
            'Chrome proxy should be enabled. proxy info: %s' % info)
      self.VerifyBadProxies(info['badProxies'], [])

    if eligible_response_count <= 1:
      raise ChromeProxyMetricException, (
          'There should be more than one DRP eligible response '
          '(eligible_response_count=%d, bypass_count=%d)\n' % (
              eligible_response_count, bypass_count))
    elif bypass_count != 1:
      raise ChromeProxyMetricException, (
          'Exactly one response should be bypassed. '
          '(eligible_response_count=%d, bypass_count=%d)\n' % (
              eligible_response_count, bypass_count))
    else:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'eligible_responses', 'count',
          eligible_response_count))
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

  def VerifyProxyInfo(self, tab, expected_proxies, expected_bad_proxies):
    info = GetProxyInfoFromNetworkInternals(tab)
    if not 'enabled' in info or not info['enabled']:
      raise ChromeProxyMetricException, (
          'Chrome proxy should be enabled. proxy info: %s' % info)
    proxies = info['proxies']
    if (set(proxies) != set(expected_proxies) and
        set(proxies) != set(self.ProxyListForDev(expected_proxies))):
      raise ChromeProxyMetricException, (
          'Wrong effective proxies (%s). Expect: "%s"' % (
          str(proxies), str(expected_proxies)))

    bad_proxies = []
    if 'badProxies' in info and info['badProxies']:
      bad_proxies = [p['proxy'] for p in info['badProxies']
                     if 'proxy' in p and p['proxy']]
    if (set(bad_proxies) != set(expected_bad_proxies) and
        set(bad_proxies) != set(self.ProxyListForDev(expected_bad_proxies))):
      raise ChromeProxyMetricException, (
          'Wrong bad proxies (%s). Expect: "%s"' % (
          str(bad_proxies), str(expected_bad_proxies)))

  def AddResultsForHTTPFallback(
      self, tab, results, expected_proxies=None, expected_bad_proxies=None):
    if not expected_proxies:
      expected_proxies = [self.effective_proxies['fallback'],
                          self.effective_proxies['direct']]
    if not expected_bad_proxies:
      expected_bad_proxies = []

    self.VerifyProxyInfo(tab, expected_proxies, expected_bad_proxies)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'http_fallback', 'boolean', True))

  def AddResultsForHTTPToDirectFallback(self, tab, results):
    self.VerifyAllProxiesBypassed(tab)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'direct_fallback', 'boolean', True))

  def AddResultsForExplicitBypass(self, tab, results, expected_bad_proxies):
    """Verify results for an explicit bypass test.

    Args:
        tab: the tab for the test.
        results: the results object to add the results values to.
        expected_bad_proxies: A list of dictionary objects representing
            expected bad proxies and their expected retry time windows.
            See the definition of VerifyBadProxies for details.
    """
    info = GetProxyInfoFromNetworkInternals(tab)
    if not 'enabled' in info or not info['enabled']:
      raise ChromeProxyMetricException, (
          'Chrome proxy should be enabled. proxy info: %s' % info)
    self.VerifyBadProxies(info['badProxies'],
                          expected_bad_proxies)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'explicit_bypass', 'boolean', True))
