# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from measurements import chrome_proxy
from page_sets.chrome_proxy import bypass
from page_sets.chrome_proxy import fallback_viaheader
from page_sets.chrome_proxy import safebrowsing
from page_sets.chrome_proxy import smoke
from page_sets.chrome_proxy import synthetic
from page_sets.chrome_proxy import top_20

class ChromeProxyLatency(test.Test):
  tag = 'latency'
  test = chrome_proxy.ChromeProxyLatency
  page_set = top_20.Top20PageSet()
  options = {'pageset_repeat_iters': 2}

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyLatencyDirect(test.Test):
  tag = 'latency_direct'
  test = chrome_proxy.ChromeProxyLatency
  page_set = top_20.Top20PageSet()
  options = {'pageset_repeat_iters': 2}


class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = synthetic.SyntheticPageSet()


class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = synthetic.SyntheticPageSet()


class ChromeProxyDataSaving(test.Test):
  tag = 'data_saving'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = top_20.Top20PageSet()
  options = {'pageset_repeat_iters': 1}
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyDataSavingDirect(test.Test):
  tag = 'data_saving_direct'
  test = chrome_proxy.ChromeProxyDataSaving
  options = {'pageset_repeat_iters': 2}
  page_set = top_20.Top20PageSet()


class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = synthetic.SyntheticPageSet()


class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = synthetic.SyntheticPageSet()


class ChromeProxyHeaderValidation(test.Test):
  tag = 'header_validation'
  test = chrome_proxy.ChromeProxyHeaders
  page_set = top_20.Top20PageSet()


class ChromeProxyBypass(test.Test):
  tag = 'bypass'
  test = chrome_proxy.ChromeProxyBypass
  page_set = bypass.BypassPageSet()


class ChromeProxySafeBrowsing(test.Test):
  tag = 'safebrowsing'
  test = chrome_proxy.ChromeProxySafebrowsing
  page_set = safebrowsing.SafebrowsingPageSet()


class ChromeProxyHTTPFallbackProbeURL(test.Test):
  tag = 'fallback-probe'
  test = chrome_proxy.ChromeProxyHTTPFallbackProbeURL
  page_set = synthetic.SyntheticPageSet()


class ChromeProxyHTTPFallbackViaHeader(test.Test):
  tag = 'fallback-viaheader'
  test = chrome_proxy.ChromeProxyHTTPFallbackViaHeader
  page_set = fallback_viaheader.FallbackViaHeaderPageSet()


class ChromeProxySmoke(test.Test):
  tag = 'smoke'
  test = chrome_proxy.ChromeProxySmoke
  page_set = smoke.SmokePageSet()
