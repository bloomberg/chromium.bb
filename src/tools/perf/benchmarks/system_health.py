# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import loading_metrics_category

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement
import page_sets


class _CommonSystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Chrome Common System Health Benchmark.

  This test suite contains system health benchmarks that can be collected
  together due to the low overhead of the tracing agents required. If a
  benchmark does have significant overhead, it should either:

    1) Be rearchitected such that it doesn't. This is the most preferred option.
    2) Be run in a separate test suite (e.g. memory).

  https://goo.gl/Jek2NL.
  """

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='rail,toplevel')
    cat_filter.AddIncludedCategory('accessibility')
    # Needed for the metric reported by page.
    cat_filter.AddIncludedCategory('blink.user_timing')
    # Needed for the console error metric.
    cat_filter.AddIncludedCategory('v8.console')

    options = timeline_based_measurement.Options(cat_filter)
    options.config.enable_chrome_trace = True
    options.config.enable_cpu_trace = True
    options.SetTimelineBasedMetrics([
        'accessibilityMetric',
        'consoleErrorMetric',
        'cpuTimeMetric',
        'limitedCpuTimeMetric',
        'reportedByPageMetric',
        'tracingMetric'
    ])
    loading_metrics_category.AugmentOptionsForLoadingMetrics(options)
    # The EQT metric depends on the same categories as the loading metric.
    options.AddTimelineBasedMetric('expectedQueueingTimeMetric')
    return options

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM)


@benchmark.Info(emails=['charliea@chromium.org', 'sullivan@chromium.org',
                        'tdresser@chromium.org',
                        'chrome-speed-metrics-dev@chromium.org'],
                component='Speed>Metrics>SystemHealthRegressions',
                documentation_url='https://bit.ly/system-health-benchmarks')
class DesktopCommonSystemHealth(_CommonSystemHealthBenchmark):
  """Desktop Chrome Energy System Health Benchmark."""
  PLATFORM = 'desktop'
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'system_health.common_desktop'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(DesktopCommonSystemHealth,
                    self).CreateCoreTimelineBasedMeasurementOptions()
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(300 * 1024)
    return options


@benchmark.Info(emails=['charliea@chromium.org', 'sullivan@chromium.org',
                        'tdresser@chromium.org',
                        'chrome-speed-metrics-dev@chromium.org'],
                component='Speed>Metrics>SystemHealthRegressions',
                documentation_url='https://bit.ly/system-health-benchmarks')
class MobileCommonSystemHealth(_CommonSystemHealthBenchmark):
  """Mobile Chrome Energy System Health Benchmark."""
  PLATFORM = 'mobile'
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'system_health.common_mobile'


class _MemorySystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Chrome Memory System Health Benchmark.

  This test suite is run separately from the common one due to the high overhead
  of memory tracing.

  https://goo.gl/Jek2NL.
  """
  options = {'pageset_repeat': 3}

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,disabled-by-default-memory-infra')
    # Needed for the console error metric.
    cat_filter.AddIncludedCategory('v8.console')
    options = timeline_based_measurement.Options(cat_filter)
    options.config.enable_android_graphics_memtrack = True
    options.SetTimelineBasedMetrics([
      'consoleErrorMetric',
      'memoryMetric'
    ])
    # Setting an empty memory dump config disables periodic dumps.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM,
                                          take_memory_measurement=True)


@benchmark.Info(
    emails=['pasko@chromium.org', 'chrome-android-perf-status@chromium.org'],
    documentation_url='https://bit.ly/system-health-benchmarks')
class DesktopMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Desktop Chrome Memory System Health Benchmark."""
  PLATFORM = 'desktop'
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'system_health.memory_desktop'


@benchmark.Info(
    emails=['pasko@chromium.org', 'chrome-android-perf-status@chromium.org'],
    documentation_url='https://bit.ly/system-health-benchmarks')
class MobileMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Mobile Chrome Memory System Health Benchmark."""
  PLATFORM = 'mobile'
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    # Just before we measure memory we flush the system caches
    # unfortunately this doesn't immediately take effect, instead
    # the next story run is effected. Due to this the first story run
    # has anomalous results. This option causes us to flush caches
    # each time before Chrome starts so we effect even the first story
    # - avoiding the bug.
    options.flush_os_page_caches_on_start = True

  @classmethod
  def Name(cls):
    return 'system_health.memory_mobile'


@benchmark.Info(emails=['oksamyt@chromium.org', 'torne@chromium.org',
                        'changwan@chromium.org'],
                component='Mobile>WebView>Perf')
class WebviewStartupSystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Webview startup time benchmark

  Benchmark that measures how long WebView takes to start up
  and load a blank page.
  """
  options = {'pageset_repeat': 20}
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID_WEBVIEW]
  SUPPORTED_PLATFORMS = [story.expectations.ANDROID_WEBVIEW]

  def CreateStorySet(self, options):
    return page_sets.SystemHealthBlankStorySet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.SetTimelineBasedMetrics(['webviewStartupMetric'])
    options.config.enable_atrace_trace = True
    # TODO(crbug.com/1028882): Recording a Chrome trace at the same time as
    # atrace causes events to stack incorrectly. Fix this by recording a
    # system+Chrome trace via system perfetto on the device instead.
    options.config.enable_chrome_trace = False
    options.config.atrace_config.app_name = 'org.chromium.webview_shell'
    return options

  @classmethod
  def Name(cls):
    return 'system_health.webview_startup'
