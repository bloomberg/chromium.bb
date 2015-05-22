# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core import perf_benchmark

from benchmarks import blink_perf
from benchmarks import silk_flags
from measurements import oilpan_gc_times
from telemetry import benchmark
import page_sets


class OilpanGCTimesBlinkPerfAnimation(perf_benchmark.PerfBenchmark):
  tag = 'blink_perf_animation'
  test = oilpan_gc_times.OilpanGCTimesForBlinkPerf

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.blink_perf_animation'

  def CreatePageSet(self, options):
    path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, 'Animation')
    return blink_perf.CreatePageSetFromPath(path, blink_perf.SKIPPED_FILE)


@benchmark.Enabled('content-shell')
class OilpanGCTimesBlinkPerfStress(perf_benchmark.PerfBenchmark):
  tag = 'blink_perf_stress'
  test = oilpan_gc_times.OilpanGCTimesForInternals

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.blink_perf_stress'

  def CreatePageSet(self, options):
    path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, 'BlinkGC')
    return blink_perf.CreatePageSetFromPath(path, blink_perf.SKIPPED_FILE)


class OilpanGCTimesSmoothnessAnimation(perf_benchmark.PerfBenchmark):
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.ToughAnimationCasesPageSet

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.tough_animation_cases'


@benchmark.Enabled('android')
class OilpanGCTimesKeySilkCases(perf_benchmark.PerfBenchmark):
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.KeySilkCasesPageSet

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.key_silk_cases'


@benchmark.Enabled('android')
class OilpanGCTimesSyncScrollKeyMobileSites(perf_benchmark.PerfBenchmark):
  tag = 'sync_scroll'
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)
  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.sync_scroll.key_mobile_sites_smooth'

