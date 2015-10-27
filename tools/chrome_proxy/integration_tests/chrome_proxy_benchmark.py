# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from common.chrome_proxy_benchmark import ChromeProxyBenchmark
from integration_tests import chrome_proxy_measurements as measurements
from integration_tests import chrome_proxy_pagesets as pagesets
from telemetry import benchmark

DESKTOP_PLATFORMS = ['mac', 'linux', 'win', 'chromeos']
WEBVIEW_PLATFORMS = ['android-webview', 'android-webview-shell']

class ChromeProxyClientVersion(ChromeProxyBenchmark):
  tag = 'client_version'
  test = measurements.ChromeProxyClientVersion
  page_set = pagesets.SyntheticStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_version.synthetic'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return False


class ChromeProxyClientType(ChromeProxyBenchmark):
  tag = 'client_type'
  test = measurements.ChromeProxyClientType
  page_set = pagesets.ClientTypeStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_type.client_type'


class ChromeProxyLoFi(ChromeProxyBenchmark):
  tag = 'lo_fi'
  test = measurements.ChromeProxyLoFi
  page_set = pagesets.LoFiStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.lo_fi.lo_fi'


class ChromeProxyExpDirective(ChromeProxyBenchmark):
  tag = 'exp_directive'
  test = measurements.ChromeProxyExpDirective
  page_set = pagesets.ExpDirectiveStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.exp_directive.exp_directive'


class ChromeProxyPassThrough(ChromeProxyBenchmark):
  tag = 'pass_through'
  test = measurements.ChromeProxyPassThrough
  page_set = pagesets.PassThroughStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.pass_through.pass_through'


class ChromeProxyBypass(ChromeProxyBenchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyBypass
  page_set = pagesets.BypassStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.bypass'


class ChromeProxyCorsBypass(ChromeProxyBenchmark):
  tag = 'bypass'
  test = measurements.ChromeProxyCorsBypass
  page_set = pagesets.CorsBypassStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.bypass.corsbypass'


class ChromeProxyBlockOnce(ChromeProxyBenchmark):
  tag = 'block_once'
  test = measurements.ChromeProxyBlockOnce
  page_set = pagesets.BlockOnceStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.block_once.block_once'


@benchmark.Disabled(*(DESKTOP_PLATFORMS + WEBVIEW_PLATFORMS))
# Safebrowsing is enabled for Android and iOS.
class ChromeProxySafeBrowsingOn(ChromeProxyBenchmark):
  tag = 'safebrowsing_on'
  test = measurements.ChromeProxySafebrowsingOn

  # Override CreateStorySet so that we can instantiate SafebrowsingStorySet
  # with a non default param.
  def CreateStorySet(self, options):
    del options  # unused
    return pagesets.SafebrowsingStorySet(expect_timeout=True)

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_on.safebrowsing'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return False


@benchmark.Enabled(*(DESKTOP_PLATFORMS + WEBVIEW_PLATFORMS))
# Safebrowsing is switched off for Android Webview and all desktop platforms.
class ChromeProxySafeBrowsingOff(ChromeProxyBenchmark):
  tag = 'safebrowsing_off'
  test = measurements.ChromeProxySafebrowsingOff
  page_set = pagesets.SafebrowsingStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.safebrowsing_off.safebrowsing'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return False


class ChromeProxyHTTPFallbackProbeURL(ChromeProxyBenchmark):
  tag = 'fallback_probe'
  test = measurements.ChromeProxyHTTPFallbackProbeURL
  page_set = pagesets.SyntheticStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_probe.synthetic'


class ChromeProxyHTTPFallbackViaHeader(ChromeProxyBenchmark):
  tag = 'fallback_viaheader'
  test = measurements.ChromeProxyHTTPFallbackViaHeader
  page_set = pagesets.FallbackViaHeaderStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.fallback_viaheader.fallback_viaheader'


class ChromeProxyHTTPToDirectFallback(ChromeProxyBenchmark):
  tag = 'http_to_direct_fallback'
  test = measurements.ChromeProxyHTTPToDirectFallback
  page_set = pagesets.HTTPToDirectFallbackStorySet

  @classmethod
  def Name(cls):
    return ('chrome_proxy_benchmark.http_to_direct_fallback.'
            'http_to_direct_fallback')


class ChromeProxyReenableAfterBypass(ChromeProxyBenchmark):
  tag = 'reenable_after_bypass'
  test = measurements.ChromeProxyReenableAfterBypass
  page_set = pagesets.ReenableAfterBypassStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.reenable_after_bypass.reenable_after_bypass'


class ChromeProxySmoke(ChromeProxyBenchmark):
  tag = 'smoke'
  test = measurements.ChromeProxySmoke
  page_set = pagesets.SmokeStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.smoke.smoke'


class ChromeProxyClientConfig(ChromeProxyBenchmark):
  tag = 'client_config'
  test = measurements.ChromeProxyClientConfig
  page_set = pagesets.SyntheticStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.client_config.synthetic'


@benchmark.Enabled(*DESKTOP_PLATFORMS)
class ChromeProxyVideoDirect(benchmark.Benchmark):
  tag = 'video'
  test = measurements.ChromeProxyVideoValidation
  page_set = pagesets.VideoDirectStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.video.direct'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True


@benchmark.Enabled(*DESKTOP_PLATFORMS)
class ChromeProxyVideoProxied(benchmark.Benchmark):
  tag = 'video'
  test = measurements.ChromeProxyVideoValidation
  page_set = pagesets.VideoProxiedStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.video.proxied'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True


@benchmark.Enabled(*DESKTOP_PLATFORMS)
class ChromeProxyVideoCompare(benchmark.Benchmark):
  """Comparison of direct and proxied video fetches.

  This benchmark runs the ChromeProxyVideoDirect and ChromeProxyVideoProxied
  benchmarks, then compares their results.
  """

  tag = 'video'
  test = measurements.ChromeProxyVideoValidation
  page_set = pagesets.VideoCompareStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.video.compare'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True


@benchmark.Enabled(*DESKTOP_PLATFORMS)
class ChromeProxyVideoFrames(benchmark.Benchmark):
  """Check for video frames similar to original video."""

  tag = 'video'
  test = measurements.ChromeProxyInstrumentedVideoValidation
  page_set = pagesets.VideoFrameStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.video.frames'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True


@benchmark.Enabled(*DESKTOP_PLATFORMS)
class ChromeProxyVideoAudio(benchmark.Benchmark):
  """Check that audio is similar to original video."""

  tag = 'video'
  test = measurements.ChromeProxyInstrumentedVideoValidation
  page_set = pagesets.VideoAudioStorySet

  @classmethod
  def Name(cls):
    return 'chrome_proxy_benchmark.video.audio'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True
