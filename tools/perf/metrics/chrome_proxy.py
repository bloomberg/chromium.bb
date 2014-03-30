# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_measurement
from metrics import network


class ChromeProxyMetricException(page_measurement.MeasurementFailure):
  pass


CHROME_PROXY_VIA_HEADER = 'Chrome-Compression-Proxy'
CHROME_PROXY_VIA_HEADER_DEPRECATED = '1.1 Chrome Compression Proxy'


class ChromeProxyResponse(network.HTTPResponse):
  """ Represents an HTTP response from a timeleine event."""
  def __init__(self, event):
    super(ChromeProxyResponse, self).__init__(event)

  def ShouldHaveChromeProxyViaHeader(self):
    resp = self.response
    # Ignore https and data url
    if resp.url.startswith('https') or resp.url.startswith('data:'):
      return False
    # Ignore 304 Not Modified.
    if resp.status == 304:
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


class ChromeProxyMetric(network.NetworkMetric):
  """A Chrome proxy timeline metric."""

  def __init__(self):
    super(ChromeProxyMetric, self).__init__()
    self.compute_data_saving = True

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

    results.Add('resources_via_proxy', 'count', resources_via_proxy)
    results.Add('resources_from_cache', 'count', resources_from_cache)
    results.Add('resources_direct', 'count', resources_direct)

  def AddResultsForHeaderValidation(self, tab, results):
    via_count = 0
    for resp in self.IterResponses(tab):
      if resp.IsValidByViaHeader():
        via_count += 1
      else:
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Via header (%s) is not valid (refer=%s, status=%d)' % (
                r.url, r.GetHeader('Via'), r.GetHeader('Referer'), r.status))
    results.Add('checked_via_header', 'count', via_count)

  def AddResultsForBypass(self, tab, results):
    bypass_count = 0
    for resp in self.IterResponses(tab):
      if resp.HasChromeProxyViaHeader():
        r = resp.response
        raise ChromeProxyMetricException, (
            '%s: Should not have Via header (%s) (refer=%s, status=%d)' % (
                r.url, r.GetHeader('Via'), r.GetHeader('Referer'), r.status))
      bypass_count += 1
    results.Add('bypass', 'count', bypass_count)

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
      results.Add('safebrowsing', 'boolean', True)
    else:
      raise ChromeProxyMetricException, (
          'Safebrowsing failed (count=%d, safebrowsing_count=%d)\n' % (
              count, safebrowsing_count))
