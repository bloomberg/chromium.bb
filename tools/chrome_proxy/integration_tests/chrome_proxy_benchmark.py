# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from integration_tests import chrome_proxy_measurements as measurements
from integration_tests import chrome_proxy_pagesets as pagesets
from telemetry import benchmark


class ChromeProxyLatency(benchmark.Benchmark):
  tag = 'latency'
  test = measurements.ChromeProxyLatency
  page_set = pagesets.Top20PageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.latency.top_20'


class ChromeProxyLatencyDirect(benchmark.Benchmark):
  tag = 'latency_direct'
  test = measurements.ChromeProxyLatency
  page_set = pagesets.Top20PageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.latency_direct.top_20'


class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.latency.synthetic'


class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.latency_direct.synthetic'


class ChromeProxyDataSaving(benchmark.Benchmark):
  tag = 'data_saving'
  test = measurements.ChromeProxyDataSaving
  page_set = pagesets.Top20PageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.data_saving.top_20'


class ChromeProxyDataSavingDirect(benchmark.Benchmark):
  tag = 'data_saving_direct'
  test = measurements.ChromeProxyDataSaving
  page_set = pagesets.Top20PageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.data_saving_direct.top_20'


class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.data_saving.synthetic'


class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.data_saving_direct.synthetic'


class ChromeProxyHeaderValidation(benchmark.Benchmark):
  tag = 'header_validation'
  test = measurements.ChromeProxyHeaders
  page_set = pagesets.Top20PageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.header_validation.top_20'


class ChromeProxyClientVersion(benchmark.Benchmark):
  tag = 'client_version'
  test = measurements.ChromeProxyClientVersion
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_version.synthetic'


class ChromeProxyClientType(benchmark.Benchmark):
  tag = 'client_type'
  test = measurements.ChromeProxyClientType
  page_set = pagesets.ClientTypePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_type.client_type'


class ChromeProxyLoFi(benchmark.Benchmark):
  tag = 'lo_fi'
  test = measurements.ChromeProxyLoFi
  page_set = pagesets.LoFiPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.lo_fi.lo_fi'


class ChromeProxyBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyBypass
  page_set = pagesets.BypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.bypass'


class ChromeProxyCorsBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyCorsBypass
  page_set = pagesets.CorsBypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.corsbypass'


class ChromeProxyBlockOnce(benchmark.Benchmark):
  tag = 'block_once'
  test = measurements.ChromeProxyBlockOnce
  page_set = pagesets.BlockOncePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.block_once.block_once'


@benchmark.Enabled('android')
# Safebrowsing is enabled for Android and iOS.
class ChromeProxySafeBrowsingOn(benchmark.Benchmark):
  tag = 'safebrowsing_on'
  test = measurements.ChromeProxySafebrowsingOn
  page_set = pagesets.SafebrowsingPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_on.safebrowsing'


# Safebrowsing is switched off for Android Webview and all desktop platforms.
class ChromeProxySafeBrowsingOff(benchmark.Benchmark):
  tag = 'safebrowsing_off'
  test = measurements.ChromeProxySafebrowsingOff
  page_set = pagesets.SafebrowsingPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_off.safebrowsing'

class ChromeProxyHTTPFallbackProbeURL(benchmark.Benchmark):
  tag = 'fallback_probe'
  test = measurements.ChromeProxyHTTPFallbackProbeURL
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_probe.synthetic'


class ChromeProxyHTTPFallbackViaHeader(benchmark.Benchmark):
  tag = 'fallback_viaheader'
  test = measurements.ChromeProxyHTTPFallbackViaHeader
  page_set = pagesets.FallbackViaHeaderPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_viaheader.fallback_viaheader'


class ChromeProxyHTTPToDirectFallback(benchmark.Benchmark):
  tag = 'http_to_direct_fallback'
  test = measurements.ChromeProxyHTTPToDirectFallback
  page_set = pagesets.HTTPToDirectFallbackPageSet

  @classmethod
  def Name(cls):
    return ('chrome_proxy_benchmark.http_to_direct_fallback.'
            'http_to_direct_fallback')


class ChromeProxyReenableAfterBypass(benchmark.Benchmark):
  tag = 'reenable_after_bypass'
  test = measurements.ChromeProxyReenableAfterBypass
  page_set = pagesets.ReenableAfterBypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.reenable_after_bypass.reenable_after_bypass'


class ChromeProxySmoke(benchmark.Benchmark):
  tag = 'smoke'
  test = measurements.ChromeProxySmoke
  page_set = pagesets.SmokePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.smoke.smoke'
