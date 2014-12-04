# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import page_sets
from benchmarks import blink_perf
from benchmarks import silk_flags
from measurements import oilpan_gc_times
from measurements import smoothness
from telemetry import benchmark
from telemetry.core import util
from telemetry import page


class OilpanGCTimesBlinkPerfAnimation(benchmark.Benchmark):
  tag = 'blink_perf_animation'
  test = oilpan_gc_times.OilpanGCTimesForBlinkPerf

  def CreatePageSet(self, options):
    path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, 'Animation')
    return blink_perf.CreatePageSetFromPath(path, blink_perf.SKIPPED_FILE)


class OilpanGCTimesSmoothnessAnimation(benchmark.Benchmark):
  test = oilpan_gc_times.OilpanGCTimesForSmoothness
  page_set = page_sets.ToughAnimationCasesPageSet


# TODO(peria): Add more page sets.  'key_silk_cases' and
# 'smoothness.sync_scroll.key_mobile_sites' are wanted for now.
