# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import silk_flags
import page_sets
from telemetry import benchmark
from telemetry.core.platform import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import gpu_timeline

TOPLEVEL_GL_CATEGORY = 'gpu_toplevel'
TOPLEVEL_CATEGORIES = ['disabled-by-default-gpu.device',
                       'disabled-by-default-gpu.service']


class _GPUTimes(benchmark.Benchmark):
  def CreateTimelineBasedMeasurementOptions(self):
    cat_string = ','.join(TOPLEVEL_CATEGORIES)
    cat_filter = tracing_category_filter.TracingCategoryFilter(cat_string)

    return timeline_based_measurement.Options(
        overhead_level=cat_filter)

  @classmethod
  def ValueCanBeAddedPredicate(cls, value):
    return (isinstance(value, gpu_timeline.GPUTimelineListOfValues) or
            isinstance(value, gpu_timeline.GPUTimelineValue))

@benchmark.Disabled  # http://crbug.com/455292
class GPUTimesKeyMobileSites(_GPUTimes):
  """Measures GPU timeline metric on key mobile sites."""
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'gpu_times.key_mobile_sites_smooth'

@benchmark.Disabled  # http://crbug.com/455292
class GPUTimesGpuRasterizationKeyMobileSites(_GPUTimes):
  """Measures GPU timeline metric on key mobile sites with GPU rasterization.
  """
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'gpu_times.gpu_rasterization.key_mobile_sites_smooth'

@benchmark.Disabled  # http://crbug.com/453131, http://crbug.com/455292
class GPUTimesTop25Sites(_GPUTimes):
  """Measures GPU timeline metric for the top 25 sites."""
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'gpu_times.top_25_smooth'

@benchmark.Disabled  # http://crbug.com/455292
class GPUTimesGpuRasterizationTop25Sites(_GPUTimes):
  """Measures GPU timeline metric for the top 25 sites with GPU rasterization.
  """
  page_set = page_sets.Top25SmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'gpu_times.gpu_rasterization.top_25_smooth'

