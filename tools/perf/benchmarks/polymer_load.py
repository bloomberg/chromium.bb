# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import polymer_load
from telemetry import benchmark
import page_sets


@benchmark.Enabled('android')
class PolymerLoadPica(perf_benchmark.PerfBenchmark):
  """Measures time to polymer-ready for Pica (News Reader)."""
  test = polymer_load.PolymerLoadMeasurement
  page_set = page_sets.PicaPageSet

  @classmethod
  def Name(cls):
    return 'polymer_load.pica'


# There is something weird about this test (or a test that precedes it)
# that causes it to fail in telemetry_perf_unittests when it is not run
# as the first of the benchmark_smoke_unittest test cases.
# See crbug.com/428207.
#@benchmark.Enabled('android')
@benchmark.Disabled
class PolymerLoadTopeka(perf_benchmark.PerfBenchmark):
  """Measures time to polymer-ready for Topeka (Quiz App)."""
  test = polymer_load.PolymerLoadMeasurement
  page_set = page_sets.TopekaPageSet
  @classmethod
  def Name(cls):
    return 'polymer_load.topeka'

