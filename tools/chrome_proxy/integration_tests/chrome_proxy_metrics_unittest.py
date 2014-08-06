# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import unittest

from integration_tests import chrome_proxy_metrics as metrics
from integration_tests import network_metrics_unittest as network_unittest
from metrics import test_page_test_results


# Timeline events used in tests.
# An HTML not via proxy.
EVENT_HTML_PROXY = network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
    url='http://test.html1',
    response_headers={
        'Content-Type': 'text/html',
        'Content-Length': str(len(network_unittest.HTML_BODY)),
        },
    body=network_unittest.HTML_BODY)

# An HTML via proxy with the deprecated Via header.
EVENT_HTML_PROXY_DEPRECATED_VIA = (
    network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
    url='http://test.html2',
    response_headers={
        'Content-Type': 'text/html',
        'Content-Encoding': 'gzip',
        'X-Original-Content-Length': str(len(network_unittest.HTML_BODY)),
        'Via': (metrics.CHROME_PROXY_VIA_HEADER_DEPRECATED +
                ',other-via'),
        },
    body=network_unittest.HTML_BODY))

# An image via proxy with Via header and it is cached.
EVENT_IMAGE_PROXY_CACHED = (
    network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
    url='http://test.image',
    response_headers={
        'Content-Type': 'image/jpeg',
        'Content-Encoding': 'gzip',
        'X-Original-Content-Length': str(network_unittest.IMAGE_OCL),
        'Via': '1.1 ' + metrics.CHROME_PROXY_VIA_HEADER,
        },
    body=base64.b64encode(network_unittest.IMAGE_BODY),
    base64_encoded_body=True,
    served_from_cache=True))

# An image fetched directly.
EVENT_IMAGE_DIRECT = (
    network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
    url='http://test.image',
    response_headers={
        'Content-Type': 'image/jpeg',
        'Content-Encoding': 'gzip',
        },
    body=base64.b64encode(network_unittest.IMAGE_BODY),
    base64_encoded_body=True))

# A safe-browsing malware response.
EVENT_MALWARE_PROXY = (
    network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
    url='http://test.malware',
    response_headers={
        'X-Malware-Url': '1',
        'Via': '1.1 ' + metrics.CHROME_PROXY_VIA_HEADER,
        'Location': 'http://test.malware',
        },
    status=307))


class ChromeProxyMetricTest(unittest.TestCase):

  _test_proxy_info = {}

  def _StubGetProxyInfo(self, info):
    def stub(unused_tab, unused_url=''):  # pylint: disable=W0613
      return ChromeProxyMetricTest._test_proxy_info
    metrics.GetProxyInfoFromNetworkInternals = stub
    ChromeProxyMetricTest._test_proxy_info = info

  def testChromeProxyResponse(self):
    # An https non-proxy response.
    resp = metrics.ChromeProxyResponse(
        network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
            url='https://test.url',
            response_headers={
                'Content-Type': 'text/html',
                'Content-Length': str(len(network_unittest.HTML_BODY)),
                'Via': 'some other via',
                },
            body=network_unittest.HTML_BODY))
    self.assertFalse(resp.ShouldHaveChromeProxyViaHeader())
    self.assertFalse(resp.HasChromeProxyViaHeader())
    self.assertTrue(resp.IsValidByViaHeader())

    # A proxied JPEG image response
    resp = metrics.ChromeProxyResponse(
        network_unittest.NetworkMetricTest.MakeNetworkTimelineEvent(
            url='http://test.image',
            response_headers={
                'Content-Type': 'image/jpeg',
                'Content-Encoding': 'gzip',
                'Via': '1.1 ' + metrics.CHROME_PROXY_VIA_HEADER,
                'X-Original-Content-Length': str(network_unittest.IMAGE_OCL),
                },
            body=base64.b64encode(network_unittest.IMAGE_BODY),
            base64_encoded_body=True))
    self.assertTrue(resp.ShouldHaveChromeProxyViaHeader())
    self.assertTrue(resp.HasChromeProxyViaHeader())
    self.assertTrue(resp.IsValidByViaHeader())

  def testChromeProxyMetricForDataSaving(self):
    metric = metrics.ChromeProxyMetric()
    events = [
        EVENT_HTML_PROXY,
        EVENT_HTML_PROXY_DEPRECATED_VIA,
        EVENT_IMAGE_PROXY_CACHED,
        EVENT_IMAGE_DIRECT]
    metric.SetEvents(events)

    self.assertTrue(len(events), len(list(metric.IterResponses(None))))
    results = test_page_test_results.TestPageTestResults(self)

    metric.AddResultsForDataSaving(None, results)
    results.AssertHasPageSpecificScalarValue('resources_via_proxy', 'count', 2)
    results.AssertHasPageSpecificScalarValue('resources_from_cache', 'count', 1)
    results.AssertHasPageSpecificScalarValue('resources_direct', 'count', 2)

  def testChromeProxyMetricForHeaderValidation(self):
    metric = metrics.ChromeProxyMetric()
    metric.SetEvents([
        EVENT_HTML_PROXY,
        EVENT_HTML_PROXY_DEPRECATED_VIA,
        EVENT_IMAGE_PROXY_CACHED,
        EVENT_IMAGE_DIRECT])

    results = test_page_test_results.TestPageTestResults(self)

    missing_via_exception = False
    try:
      metric.AddResultsForHeaderValidation(None, results)
    except metrics.ChromeProxyMetricException:
      missing_via_exception = True
    # Only the HTTP image response does not have a valid Via header.
    self.assertTrue(missing_via_exception)

    # Two events with valid Via headers.
    metric.SetEvents([
        EVENT_HTML_PROXY_DEPRECATED_VIA,
        EVENT_IMAGE_PROXY_CACHED])
    metric.AddResultsForHeaderValidation(None, results)
    results.AssertHasPageSpecificScalarValue('checked_via_header', 'count', 2)

  def testChromeProxyMetricForBypass(self):
    metric = metrics.ChromeProxyMetric()
    metric.SetEvents([
        EVENT_HTML_PROXY,
        EVENT_HTML_PROXY_DEPRECATED_VIA,
        EVENT_IMAGE_PROXY_CACHED,
        EVENT_IMAGE_DIRECT])
    results = test_page_test_results.TestPageTestResults(self)

    bypass_exception = False
    try:
      metric.AddResultsForBypass(None, results)
    except metrics.ChromeProxyMetricException:
      bypass_exception = True
    # Two of the first three events have Via headers.
    self.assertTrue(bypass_exception)

    # Use directly fetched image only. It is treated as bypassed.
    metric.SetEvents([EVENT_IMAGE_DIRECT])
    metric.AddResultsForBypass(None, results)
    results.AssertHasPageSpecificScalarValue('bypass', 'count', 1)

  def testChromeProxyMetricForHTTPFallback(self):
    metric = metrics.ChromeProxyMetric()
    metric.SetEvents([
        EVENT_HTML_PROXY,
        EVENT_HTML_PROXY_DEPRECATED_VIA])
    results = test_page_test_results.TestPageTestResults(self)

    fallback_exception = False
    info = {}
    info['enabled'] = False
    self._StubGetProxyInfo(info)
    try:
      metric.AddResultsForBypass(None, results)
    except metrics.ChromeProxyMetricException:
      fallback_exception = True
    self.assertTrue(fallback_exception)

    fallback_exception = False
    info['enabled'] = True
    info['proxies'] = [
        'something.else.com:80',
        metrics.PROXY_SETTING_DIRECT
        ]
    self._StubGetProxyInfo(info)
    try:
      metric.AddResultsForBypass(None, results)
    except metrics.ChromeProxyMetricException:
      fallback_exception = True
    self.assertTrue(fallback_exception)

    info['enabled'] = True
    info['proxies'] = [
        metrics.PROXY_SETTING_HTTP,
        metrics.PROXY_SETTING_DIRECT
        ]
    self._StubGetProxyInfo(info)
    metric.AddResultsForHTTPFallback(None, results)

  def testChromeProxyMetricForSafebrowsing(self):
    metric = metrics.ChromeProxyMetric()
    metric.SetEvents([EVENT_MALWARE_PROXY])
    results = test_page_test_results.TestPageTestResults(self)

    metric.AddResultsForSafebrowsing(None, results)
    results.AssertHasPageSpecificScalarValue('safebrowsing', 'boolean', True)

    # Clear results and metrics to test no response for safebrowsing
    results = test_page_test_results.TestPageTestResults(self)
    metric.SetEvents([])
    metric.AddResultsForSafebrowsing(None, results)
    results.AssertHasPageSpecificScalarValue('safebrowsing', 'boolean', True)
