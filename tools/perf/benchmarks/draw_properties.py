# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import draw_properties
import page_sets
from telemetry import benchmark


# This benchmark depends on tracing categories available in M43
# This benchmark is still useful for manual testing, but need not be enabled
# and run regularly.
@benchmark.Disabled('all')
class DrawPropertiesToughScrolling(perf_benchmark.PerfBenchmark):
  test = draw_properties.DrawProperties
  page_set = page_sets.ToughScrollingCasesPageSet

  @classmethod
  def Name(cls):
    return 'draw_properties.tough_scrolling'


# This benchmark depends on tracing categories available in M43
# This benchmark is still useful for manual testing, but need not be enabled
# and run regularly.
@benchmark.Disabled('all')
class DrawPropertiesTop25(perf_benchmark.PerfBenchmark):
  """Measures the performance of computing draw properties from property trees.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = draw_properties.DrawProperties
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'draw_properties.top_25'
