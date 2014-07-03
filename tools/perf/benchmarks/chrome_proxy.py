# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import chrome_proxy
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class ChromeProxyLatency(benchmark.Benchmark):
  tag = 'latency'
  test = chrome_proxy.ChromeProxyLatency
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


@benchmark.Enabled('android')
class ChromeProxyLatencyDirect(benchmark.Benchmark):
  tag = 'latency_direct'
  test = chrome_proxy.ChromeProxyLatency
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}


@benchmark.Enabled('android')
class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = page_sets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = page_sets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyDataSaving(benchmark.Benchmark):
  tag = 'data_saving'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 1}
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


@benchmark.Enabled('android')
class ChromeProxyDataSavingDirect(benchmark.Benchmark):
  tag = 'data_saving_direct'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}


@benchmark.Enabled('android')
class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = page_sets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = page_sets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyHeaderValidation(benchmark.Benchmark):
  tag = 'header_validation'
  test = chrome_proxy.ChromeProxyHeaders
  page_set = page_sets.Top20PageSet


@benchmark.Enabled('android')
class ChromeProxyBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = chrome_proxy.ChromeProxyBypass
  page_set = page_sets.BypassPageSet


@benchmark.Enabled('android')
class ChromeProxySafeBrowsing(benchmark.Benchmark):
  tag = 'safebrowsing'
  test = chrome_proxy.ChromeProxySafebrowsing
  page_set = page_sets.SafebrowsingPageSet


@benchmark.Enabled('android')
class ChromeProxyHTTPFallbackProbeURL(benchmark.Benchmark):
  tag = 'fallback-probe'
  test = chrome_proxy.ChromeProxyHTTPFallbackProbeURL
  page_set = page_sets.SyntheticPageSet


@benchmark.Enabled('android')
class ChromeProxyHTTPFallbackViaHeader(benchmark.Benchmark):
  tag = 'fallback-viaheader'
  test = chrome_proxy.ChromeProxyHTTPFallbackViaHeader
  page_set = page_sets.FallbackViaHeaderPageSet


@benchmark.Enabled('android')
class ChromeProxySmoke(benchmark.Benchmark):
  tag = 'smoke'
  test = chrome_proxy.ChromeProxySmoke
  page_set = page_sets.SmokePageSet
