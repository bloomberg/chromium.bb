# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import cheapness_predictor_metrics
from telemetry.page import page_measurement

PREDICTOR_STATS = [
  {'name': 'picture_pile_count', 'units': ''},
  {'name': 'predictor_accuracy', 'units': 'percent'},
  {'name': 'predictor_safely_wrong_count', 'units': ''},
  {'name': 'predictor_badly_wrong_count', 'units': ''}]

class CheapnessPredictorMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(CheapnessPredictorMeasurement, self).__init__('smoothness')
    self._metrics = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')
    options.AppendExtraBrowserArg('--enable-prediction-benchmarking')
    options.AppendExtraBrowserArg('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArg('--enable-threaded-compositing')
    options.AppendExtraBrowserArg('--enable-impl-side-painting')

  def DidNavigateToPage(self, page, tab):
    self._metrics = \
      cheapness_predictor_metrics.CheapnessPredictorMetrics(tab)
    self._metrics.GatherInitialStats()

  def DidRunAction(self, page, tab, action):
    self._metrics.GatherDeltaStats()

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def MeasurePage(self, page, tab, results):
    predictor_stats = self._metrics.stats

    for stat_to_gather in PREDICTOR_STATS:
      results.Add(stat_to_gather['name'],
                  stat_to_gather['units'],
                  predictor_stats[stat_to_gather['name']])
