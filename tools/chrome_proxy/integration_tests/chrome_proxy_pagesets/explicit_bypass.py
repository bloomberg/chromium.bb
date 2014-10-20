# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from integration_tests import chrome_proxy_measurements as measurements
from integration_tests import chrome_proxy_metrics as metrics
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ExplicitBypassPage(page_module.Page):
  """A test page for the explicit bypass tests.

  Attributes:
      num_bypassed_proxies: The number of proxies that should be bypassed as a
          direct result of loading this test page. 1 indicates that only the
          primary data reduction proxy should be bypassed, while 2 indicates
          that both the primary and fallback data reduction proxies should be
          bypassed.
      bypass_seconds_low: The minimum number of seconds that the bypass
          triggered by loading this page should last.
      bypass_seconds_high: The maximum number of seconds that the bypass
          triggered by loading this page should last.
  """

  def __init__(self,
               url,
               page_set,
               num_bypassed_proxies,
               bypass_seconds_low,
               bypass_seconds_high):
    super(ExplicitBypassPage, self).__init__(url=url, page_set=page_set)
    self.num_bypassed_proxies = num_bypassed_proxies
    self.bypass_seconds_low = bypass_seconds_low
    self.bypass_seconds_high = bypass_seconds_high


class ExplicitBypassPageSet(page_set_module.PageSet):
  """ Chrome proxy test sites """

  def __init__(self):
    super(ExplicitBypassPageSet, self).__init__()

    # Test page for "Chrome-Proxy: bypass=0".
    self.AddPage(ExplicitBypassPage(
        url=measurements.GetResponseOverrideURL(
            respHeader='{"Chrome-Proxy":["bypass=0"],'
                       '"Via":["1.1 Chrome-Compression-Proxy"]}'),
        page_set=self,
        num_bypassed_proxies=1,
        bypass_seconds_low=metrics.DEFAULT_BYPASS_MIN_SECONDS,
        bypass_seconds_high=metrics.DEFAULT_BYPASS_MAX_SECONDS))

    # Test page for "Chrome-Proxy: bypass=3600".
    self.AddPage(ExplicitBypassPage(
        url=measurements.GetResponseOverrideURL(
            respHeader='{"Chrome-Proxy":["bypass=3600"],'
                       '"Via":["1.1 Chrome-Compression-Proxy"]}'),
        page_set=self,
        num_bypassed_proxies=1,
        bypass_seconds_low=3600,
        bypass_seconds_high=3600))

    # Test page for "Chrome-Proxy: block=0".
    self.AddPage(ExplicitBypassPage(
        url=measurements.GetResponseOverrideURL(
            respHeader='{"Chrome-Proxy":["block=0"],'
                       '"Via":["1.1 Chrome-Compression-Proxy"]}'),
        page_set=self,
        num_bypassed_proxies=2,
        bypass_seconds_low=metrics.DEFAULT_BYPASS_MIN_SECONDS,
        bypass_seconds_high=metrics.DEFAULT_BYPASS_MAX_SECONDS))

    # Test page for "Chrome-Proxy: block=3600".
    self.AddPage(ExplicitBypassPage(
        url=measurements.GetResponseOverrideURL(
            respHeader='{"Chrome-Proxy":["block=3600"],'
                       '"Via":["1.1 Chrome-Compression-Proxy"]}'),
        page_set=self,
        num_bypassed_proxies=2,
        bypass_seconds_low=3600,
        bypass_seconds_high=3600))
