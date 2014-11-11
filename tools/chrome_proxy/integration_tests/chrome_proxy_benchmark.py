# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from integration_tests import chrome_proxy_measurements as measurements
from integration_tests import chrome_proxy_pagesets as pagesets
from telemetry import benchmark


@benchmark.Enabled('android')
class ChromeProxyLatency(benchmark.Benchmark):
  tag = 'latency'
  test = measurements.ChromeProxyLatency
  page_set = pagesets.Top20PageSet

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


@benchmark.Enabled('android')
class ChromeProxyLatencyDirect(benchmark.Benchmark):
  tag = 'latency_direct'
  test = measurements.ChromeProxyLatency
  page_set = pagesets.Top20PageSet


@benchmark.Enabled('android')
class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyDataSaving(benchmark.Benchmark):
  tag = 'data_saving'
  test = measurements.ChromeProxyDataSaving
  page_set = pagesets.Top20PageSet

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


@benchmark.Enabled('android')
class ChromeProxyDataSavingDirect(benchmark.Benchmark):
  tag = 'data_saving_direct'
  test = measurements.ChromeProxyDataSaving
  page_set = pagesets.Top20PageSet


@benchmark.Enabled('android')
class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyHeaderValidation(benchmark.Benchmark):
  tag = 'header_validation'
  test = measurements.ChromeProxyHeaders
  page_set = pagesets.Top20PageSet


@benchmark.Enabled('android')
class ChromeProxyClientVersion(benchmark.Benchmark):
  tag = 'client_version'
  test = measurements.ChromeProxyClientVersion
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyClientType(benchmark.Benchmark):
  tag = 'client_type'
  test = measurements.ChromeProxyClientType
  page_set = pagesets.ClientTypePageSet


@benchmark.Enabled('android')
class ChromeProxyBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyBypass
  page_set = pagesets.BypassPageSet


@benchmark.Enabled('android')
class ChromeProxyCorsBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyCorsBypass
  page_set = pagesets.CorsBypassPageSet


@benchmark.Enabled('android')
class ChromeProxyBlockOnce(benchmark.Benchmark):
  tag = 'block_once'
  test = measurements.ChromeProxyBlockOnce
  page_set = pagesets.BlockOncePageSet


@benchmark.Enabled('android')
class ChromeProxySafeBrowsing(benchmark.Benchmark):
  tag = 'safebrowsing'
  test = measurements.ChromeProxySafebrowsing
  page_set = pagesets.SafebrowsingPageSet


@benchmark.Enabled('android')
class ChromeProxyHTTPFallbackProbeURL(benchmark.Benchmark):
  tag = 'fallback-probe'
  test = measurements.ChromeProxyHTTPFallbackProbeURL
  page_set = pagesets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyHTTPFallbackViaHeader(benchmark.Benchmark):
  tag = 'fallback-viaheader'
  test = measurements.ChromeProxyHTTPFallbackViaHeader
  page_set = pagesets.FallbackViaHeaderPageSet


@benchmark.Enabled('android')
class ChromeProxyHTTPToDirectFallback(benchmark.Benchmark):
  tag = 'http-to-direct-fallback'
  test = measurements.ChromeProxyHTTPToDirectFallback
  page_set = pagesets.HTTPToDirectFallbackPageSet


@benchmark.Enabled('android')
class ChromeProxyExplicitBypass(benchmark.Benchmark):
  tag = 'explicit-bypass'
  test = measurements.ChromeProxyExplicitBypass
  page_set = pagesets.ExplicitBypassPageSet


@benchmark.Enabled('android')
class ChromeProxySmoke(benchmark.Benchmark):
  tag = 'smoke'
  test = measurements.ChromeProxySmoke
  page_set = pagesets.SmokePageSet
