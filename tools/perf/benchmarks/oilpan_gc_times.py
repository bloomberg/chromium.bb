# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import page_sets
from benchmarks import blink_perf
from benchmarks import silk_flags
from measurements import oilpan_gc_times
from telemetry import benchmark
from telemetry.core import util
from telemetry import page


class OilpanGCTimesBlinkPerfAnimation(benchmark.Benchmark):
  tag = 'blink_perf_animation'
  test = oilpan_gc_times.OilpanGCTimesForBlinkPerf

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.blink_perf_animation'

  def CreatePageSet(self, options):
    path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, 'Animation')
    return blink_perf.CreatePageSetFromPath(path, blink_perf.SKIPPED_FILE)


@benchmark.Enabled('content-shell')
class OilpanGCTimesBlinkPerfStress(benchmark.Benchmark):
  tag = 'blink_perf_stress'
  test = oilpan_gc_times.OilpanGCTimesForInternals

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.blink_perf_stress'

  def CreatePageSet(self, options):
    path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, 'BlinkGC')
    return blink_perf.CreatePageSetFromPath(path, blink_perf.SKIPPED_FILE)


class OilpanGCTimesSmoothnessAnimation(benchmark.Benchmark):
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.ToughAnimationCasesPageSet

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.tough_animation_cases'


@benchmark.Enabled('android')
class OilpanGCTimesKeySilkCases(benchmark.Benchmark):
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.KeySilkCasesPageSet

  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.key_silk_cases'


@benchmark.Enabled('android')
class OilpanGCTimesSyncScrollKeyMobileSites(benchmark.Benchmark):
  tag = 'sync_scroll'
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)
  @classmethod
  def Name(cls):
    return 'oilpan_gc_times.sync_scroll.key_mobile_sites_smooth'

