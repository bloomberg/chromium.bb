# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from measurements import draw_properties
import page_sets


@benchmark.Disabled()  # http://crbug.com/463111
class DrawPropertiesToughScrolling(benchmark.Benchmark):
  test = draw_properties.DrawProperties
  page_set = page_sets.ToughScrollingCasesPageSet
  @classmethod
  def Name(cls):
    return 'draw_properties.tough_scrolling'


@benchmark.Disabled()  # http://crbug.com/463111
class DrawPropertiesTop25(benchmark.Benchmark):
  """Measures the relative performance of CalcDrawProperties vs computing draw
  properties from property trees.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = draw_properties.DrawProperties
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'draw_properties.top_25'
