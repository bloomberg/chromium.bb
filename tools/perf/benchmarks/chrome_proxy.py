# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from measurements import chrome_proxy
import page_sets


class ChromeProxyLatency(benchmark.Benchmark):
  tag = 'latency'
  test = chrome_proxy.ChromeProxyLatency
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyLatencyDirect(benchmark.Benchmark):
  tag = 'latency_direct'
  test = chrome_proxy.ChromeProxyLatency
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}


class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = page_sets.SyntheticPageSet


class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = page_sets.SyntheticPageSet


class ChromeProxyDataSaving(benchmark.Benchmark):
  tag = 'data_saving'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 1}
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyDataSavingDirect(benchmark.Benchmark):
  tag = 'data_saving_direct'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = page_sets.Top20PageSet
  options = {'pageset_repeat_iters': 2}


class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = page_sets.SyntheticPageSet


class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = page_sets.SyntheticPageSet


class ChromeProxyHeaderValidation(benchmark.Benchmark):
  tag = 'header_validation'
  test = chrome_proxy.ChromeProxyHeaders
  page_set = page_sets.Top20PageSet


class ChromeProxyBypass(benchmark.Benchmark):
  tag = 'bypass'
  test = chrome_proxy.ChromeProxyBypass
  page_set = page_sets.BypassPageSet


class ChromeProxySafeBrowsing(benchmark.Benchmark):
  tag = 'safebrowsing'
  test = chrome_proxy.ChromeProxySafebrowsing
  page_set = page_sets.SafebrowsingPageSet


class ChromeProxyHTTPFallbackProbeURL(benchmark.Benchmark):
  tag = 'fallback-probe'
  test = chrome_proxy.ChromeProxyHTTPFallbackProbeURL
  page_set = page_sets.SyntheticPageSet


class ChromeProxyHTTPFallbackViaHeader(benchmark.Benchmark):
  tag = 'fallback-viaheader'
  test = chrome_proxy.ChromeProxyHTTPFallbackViaHeader
  page_set = page_sets.FallbackViaHeaderPageSet


class ChromeProxySmoke(benchmark.Benchmark):
  tag = 'smoke'
  test = chrome_proxy.ChromeProxySmoke
  page_set = page_sets.SmokePageSet
