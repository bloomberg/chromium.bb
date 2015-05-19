# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from common.chrome_proxy_benchmark import ChromeProxyBenchmark
from integration_tests import chrome_proxy_pagesets as pagesets
from integration_tests import chrome_proxy_measurements as measurements
from telemetry import benchmark
from telemetry.core.backends.chrome import android_browser_finder


ANDROID_CHROME_BROWSERS = [
  browser for browser in android_browser_finder.CHROME_PACKAGE_NAMES
    if 'webview' not in browser]


class ChromeProxyClientVersion(ChromeProxyBenchmark):
  tag = 'client_version'
  test = measurements.ChromeProxyClientVersion
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_version.synthetic'


class ChromeProxyClientType(ChromeProxyBenchmark):
  tag = 'client_type'
  test = measurements.ChromeProxyClientType
  page_set = pagesets.ClientTypePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_type.client_type'


class ChromeProxyLoFi(ChromeProxyBenchmark):
  tag = 'lo_fi'
  test = measurements.ChromeProxyLoFi
  page_set = pagesets.LoFiPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.lo_fi.lo_fi'


class ChromeProxyExpDirective(ChromeProxyBenchmark):
  tag = 'exp_directive'
  test = measurements.ChromeProxyExpDirective
  page_set = pagesets.ExpDirectivePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.exp_directive.exp_directive'


class ChromeProxyBypass(ChromeProxyBenchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyBypass
  page_set = pagesets.BypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.bypass'


class ChromeProxyCorsBypass(ChromeProxyBenchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyCorsBypass
  page_set = pagesets.CorsBypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.corsbypass'


class ChromeProxyBlockOnce(ChromeProxyBenchmark):
  tag = 'block_once'
  test = measurements.ChromeProxyBlockOnce
  page_set = pagesets.BlockOncePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.block_once.block_once'


@benchmark.Enabled(*ANDROID_CHROME_BROWSERS)
# Safebrowsing is enabled for Android and iOS.
class ChromeProxySafeBrowsingOn(ChromeProxyBenchmark):
  tag = 'safebrowsing_on'
  test = measurements.ChromeProxySafebrowsingOn

  # Override CreateUserStorySet so that we can instantiate SafebrowsingPageSet
  # with a non default param.
  def CreateUserStorySet(self, options):
    del options  # unused
    return pagesets.SafebrowsingPageSet(expect_timeout=True)

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_on.safebrowsing'


@benchmark.Disabled(*ANDROID_CHROME_BROWSERS)
# Safebrowsing is switched off for Android Webview and all desktop platforms.
class ChromeProxySafeBrowsingOff(ChromeProxyBenchmark):
  tag = 'safebrowsing_off'
  test = measurements.ChromeProxySafebrowsingOff
  page_set = pagesets.SafebrowsingPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_off.safebrowsing'

class ChromeProxyHTTPFallbackProbeURL(ChromeProxyBenchmark):
  tag = 'fallback_probe'
  test = measurements.ChromeProxyHTTPFallbackProbeURL
  page_set = pagesets.SyntheticPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_probe.synthetic'


class ChromeProxyHTTPFallbackViaHeader(ChromeProxyBenchmark):
  tag = 'fallback_viaheader'
  test = measurements.ChromeProxyHTTPFallbackViaHeader
  page_set = pagesets.FallbackViaHeaderPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_viaheader.fallback_viaheader'


class ChromeProxyHTTPToDirectFallback(ChromeProxyBenchmark):
  tag = 'http_to_direct_fallback'
  test = measurements.ChromeProxyHTTPToDirectFallback
  page_set = pagesets.HTTPToDirectFallbackPageSet

  @classmethod
  def Name(cls):
    return ('chrome_proxy_benchmark.http_to_direct_fallback.'
            'http_to_direct_fallback')


class ChromeProxyReenableAfterBypass(ChromeProxyBenchmark):
  tag = 'reenable_after_bypass'
  test = measurements.ChromeProxyReenableAfterBypass
  page_set = pagesets.ReenableAfterBypassPageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.reenable_after_bypass.reenable_after_bypass'


class ChromeProxySmoke(ChromeProxyBenchmark):
  tag = 'smoke'
  test = measurements.ChromeProxySmoke
  page_set = pagesets.SmokePageSet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.smoke.smoke'
