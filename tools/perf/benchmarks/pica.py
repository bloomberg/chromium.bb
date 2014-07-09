# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.value import scalar

class _PicaMeasurement(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    # Needed for native custom elements (document.register)
    options.AppendExtraBrowserArgs(
        '--enable-experimental-web-platform-features')

  def MeasurePage(self, _, tab, results):
    result = int(tab.EvaluateJavaScript('__polymer_ready_time'))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Total', 'ms', result))


class Pica(benchmark.Benchmark):
  test = _PicaMeasurement
  page_set = page_sets.PicaPageSet
