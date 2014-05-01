# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from measurements import chrome_proxy


class ChromeProxyLatency(test.Test):
  tag = 'latency'
  test = chrome_proxy.ChromeProxyLatency
  page_set = 'page_sets/chrome_proxy/top_20.py'
  options = {'pageset_repeat_iters': 2}

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyLatencyDirect(test.Test):
  tag = 'latency_direct'
  test = chrome_proxy.ChromeProxyLatency
  page_set = 'page_sets/chrome_proxy/top_20.py'
  options = {'pageset_repeat_iters': 2}


class ChromeProxyLatencySynthetic(ChromeProxyLatency):
  page_set = 'page_sets/chrome_proxy/synthetic.py'


class ChromeProxyLatencySyntheticDirect(ChromeProxyLatencyDirect):
  page_set = 'page_sets/chrome_proxy/synthetic.py'


class ChromeProxyDataSaving(test.Test):
  tag = 'data_saving'
  test = chrome_proxy.ChromeProxyDataSaving
  page_set = 'page_sets/chrome_proxy/top_20.py'
  options = {'pageset_repeat_iters': 1}
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyDataSavingDirect(test.Test):
  tag = 'data_saving_direct'
  test = chrome_proxy.ChromeProxyDataSaving
  options = {'pageset_repeat_iters': 2}
  page_set = 'page_sets/chrome_proxy/top_20.py'


class ChromeProxyDataSavingSynthetic(ChromeProxyDataSaving):
  page_set = 'page_sets/chrome_proxy/synthetic.py'


class ChromeProxyDataSavingSyntheticDirect(ChromeProxyDataSavingDirect):
  page_set = 'page_sets/chrome_proxy/synthetic.py'


class ChromeProxyHeaderValidation(test.Test):
  tag = 'header_validation'
  test = chrome_proxy.ChromeProxyHeaders
  page_set = 'page_sets/chrome_proxy/top_20.py'


class ChromeProxyBypass(test.Test):
  tag = 'bypass'
  test = chrome_proxy.ChromeProxyBypass
  page_set = 'page_sets/chrome_proxy/bypass.py'


class ChromeProxySafeBrowsing(test.Test):
  tag = 'safebrowsing'
  test = chrome_proxy.ChromeProxySafebrowsing
  page_set = 'page_sets/chrome_proxy/safebrowsing.py'


class ChromeProxySmoke(test.Test):
  tag = 'smoke'
  test = chrome_proxy.ChromeProxySmoke
  page_set = 'page_sets/chrome_proxy/smoke.py'
