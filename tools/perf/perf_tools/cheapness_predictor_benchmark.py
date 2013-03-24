# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import cheapness_predictor_measurement
from telemetry.page import page_benchmark

PREDICTOR_STATS = [
  {'name': 'picture_pile_count', 'units': ''},
  {'name': 'predictor_accuracy', 'units': 'percent'},
  {'name': 'predictor_safely_wrong_count', 'units': ''},
  {'name': 'predictor_badly_wrong_count', 'units': ''}]

class CheapnessPredictorBenchmark(page_benchmark.PageBenchmark):
  def __init__(self):
    super(CheapnessPredictorBenchmark, self).__init__('smoothness')
    self._measurement = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--enable-prediction-benchmarking')
    options.AppendExtraBrowserArg('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArg('--enable-threaded-compositing')
    options.AppendExtraBrowserArg('--enable-impl-side-painting')

  def DidNavigateToPage(self, page, tab):
    self._measurement = \
      cheapness_predictor_measurement.CheapnessPredictorMeasurement(tab)
    self._measurement.GatherInitialStats()

  def DidRunAction(self, page, tab, action):
    self._measurement.GatherDeltaStats()

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def MeasurePage(self, page, tab, results):
    predictor_stats = self._measurement.stats

    for stat_to_gather in PREDICTOR_STATS:
      results.Add(stat_to_gather['name'],
                  stat_to_gather['units'],
                  predictor_stats[stat_to_gather['name']])
