# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

class CheapnessPredictorMeasurement(object):
  def __init__(self, tab):
    self._tab = tab
    self._initial_stats = {}
    self.stats = {}

  def GatherInitialStats(self):
    self._initial_stats = self._GatherStats()

  def GatherDeltaStats(self):
    final_stats = self._GatherStats()

    correct_count = final_stats['predictor_correct_count'] - \
                    self._initial_stats['predictor_correct_count']

    incorrect_count = final_stats['predictor_incorrect_count'] - \
                      self._initial_stats['predictor_incorrect_count']

    percent, total = self._GetPercentAndTotal(correct_count, incorrect_count)

    self.stats['picture_pile_count'] = total
    self.stats['predictor_correct_count'] = correct_count
    self.stats['predictor_incorrect_count'] = incorrect_count
    self.stats['predictor_accuracy'] = percent
    self.stats['predictor_safely_wrong_count'] = \
      final_stats['predictor_safely_wrong_count'] - \
      self._initial_stats['predictor_safely_wrong_count']
    self.stats['predictor_badly_wrong_count'] = \
      final_stats['predictor_badly_wrong_count'] - \
      self._initial_stats['predictor_badly_wrong_count']

  def _GatherStats(self):
    stats = {}

    incorrect_count, correct_count = \
      self._GetBooleanHistogramCounts(self._tab,
                                      'Renderer4.CheapPredictorAccuracy')

    percent, total = self._GetPercentAndTotal(correct_count, incorrect_count)
    stats['picture_pile_count'] = total
    stats['predictor_correct_count'] = correct_count
    stats['predictor_incorrect_count'] = incorrect_count
    stats['predictor_accuracy'] = percent

    _, safely_wrong_count = \
      self._GetBooleanHistogramCounts(self._tab,
                                      'Renderer4.CheapPredictorSafelyWrong')
    stats['predictor_safely_wrong_count'] = safely_wrong_count

    _, badly_wrong_count = \
      self._GetBooleanHistogramCounts(self._tab,
                                      'Renderer4.CheapPredictorBadlyWrong')
    stats['predictor_badly_wrong_count'] = badly_wrong_count

    return stats


  def _GetPercentAndTotal(self, correct_count, incorrect_count):
    total = incorrect_count + correct_count
    percent = 0
    if total > 0:
      percent = 100 * correct_count / float(total)
    return percent, total

  def _GetBooleanHistogramCounts(self, tab, histogram_name):
    count = [0, 0]
    js = ('window.domAutomationController.getHistogram ? '
          'window.domAutomationController.getHistogram('
          '"%s") : ""' % (histogram_name))
    data = tab.EvaluateJavaScript(js)
    if not data:
      return count

    histogram = json.loads(data)
    if histogram:
      for bucket in histogram['buckets']:
        if bucket['low'] > 1:
          continue
        count[bucket['low']] += bucket['count']

    return count
