# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from telemetry.web_perf import timeline_based_measurement
import page_sets


class _BattOrBenchmark(perf_benchmark.PerfBenchmark):

  def CreateTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.enable_battor_trace = True
    # TODO(rnephew): Enable chrome trace when clock syncs work well with it.
    options.config.enable_chrome_trace = False
    options.SetTimelineBasedMetric('powerMetric')
    return options

  @classmethod
  def ShouldDisable(cls, possible_browser):
    # Only run if BattOr is detected.
    if not possible_browser.platform.HasBattOrConnected():
      return True

    # Galaxy S5s have problems with running system health metrics.
    # http://crbug.com/600463
    galaxy_s5_type_name = 'SM-G900H'
    return possible_browser.platform.GetDeviceTypeName() == galaxy_s5_type_name

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True


class BattOrPowerMobileSites(_BattOrBenchmark):
  page_set = page_sets.power_cases.PowerCasesPageSet

  @classmethod
  def Name(cls):
    return 'BattOr.BattOrCases'

