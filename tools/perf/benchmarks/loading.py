# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
import page_sets

import ct_benchmarks_util
from benchmarks import page_cycler_v2
from telemetry import benchmark
from telemetry.page import cache_temperature
from telemetry.page import traffic_setting
from telemetry.web_perf import timeline_based_measurement


class _LoadingBase(perf_benchmark.PerfBenchmark):
  """ A base class for loading benchmarks. """

  options = {'pageset_repeat': 2}

  def CreateTimelineBasedMeasurementOptions(self):
    tbm_options = timeline_based_measurement.Options()
    page_cycler_v2.AugmentOptionsForLoadingMetrics(tbm_options)
    return tbm_options


@benchmark.Disabled('android')
@benchmark.Owner(emails=['kouhei@chormium.org', 'ksakamoto@chromium.org'])
class LoadingDesktop(_LoadingBase):
  """ A benchmark measuring loading performance of desktop sites. """

  @classmethod
  def ShouldDisable(cls, possible_browser):
    return possible_browser.browser_type == 'reference'

  def CreateStorySet(self, options):
    return page_sets.LoadingDesktopStorySet(
        cache_temperatures=[cache_temperature.PCV1_COLD,
                            cache_temperature.PCV1_WARM,])

  @classmethod
  def Name(cls):
    return 'loading.desktop'


@benchmark.Enabled('android')
@benchmark.Owner(emails=['kouhei@chromium.org', 'ksakamoto@chromium.org'])
class LoadingMobile(_LoadingBase):
  """ A benchmark measuring loading performance of mobile sites. """

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # crbug.com/619254
    if possible_browser.browser_type == 'reference':
      return True

    # crbug.com/676612
    if ((possible_browser.platform.GetDeviceTypeName() == 'Nexus 6' or
         possible_browser.platform.GetDeviceTypeName() == 'AOSP on Shamu') and
        possible_browser.browser_type == 'android-webview'):
      return True

    return False

  def CreateStorySet(self, options):
    return page_sets.LoadingMobileStorySet(
        cache_temperatures=[cache_temperature.ANY],
        traffic_settings=[traffic_setting.NONE, traffic_setting.REGULAR_3G])

  @classmethod
  def Name(cls):
    return 'loading.mobile'


# Disabled because we do not plan on running CT benchmarks on the perf
# waterfall any time soon.
@benchmark.Disabled('all')
class LoadingClusterTelemetry(_LoadingBase):

  options = {'upload_results': True}

  _ALL_NET_CONFIGS = traffic_setting.NETWORK_CONFIGS.keys()

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    super(LoadingClusterTelemetry, cls).AddBenchmarkCommandLineArgs(parser)
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_option(
        '--wait-time',  action='store', type='int',
        default=60, help='Number of seconds to wait for after navigation.')
    parser.add_option(
        '--traffic-setting',  choices=cls._ALL_NET_CONFIGS,
        default=traffic_setting.REGULAR_4G,
        help='Traffic condition (string). Default to "%%default". Can be: %s' %
         ', '.join(cls._ALL_NET_CONFIGS))

  def CreateStorySet(self, options):
    def Wait(action_runner):
      action_runner.Wait(options.wait_time)
    return page_sets.CTPageSet(
      options.urls_list, options.user_agent, options.archive_data_file,
      traffic_setting=options.traffic_setting,
      run_page_interaction_callback=Wait)

  @classmethod
  def Name(cls):
    return 'loading.cluster_telemetry'
