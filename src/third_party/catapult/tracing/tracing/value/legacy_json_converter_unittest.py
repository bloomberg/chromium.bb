# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from tracing.value import legacy_json_converter
from tracing.value.diagnostics import reserved_infos


class LegacyJsonConverterUnittest(unittest.TestCase):

  def testConvertBasic(self):
    dicts = [{
        'master': 'Master',
        'bot': 'Bot',
        'test': 'Suite/Metric',
        'revision': 1234,
        'value': 42.0
    }]

    histograms = legacy_json_converter.ConvertLegacyDicts(dicts)

    self.assertEqual(len(histograms), 1)

    h = histograms.GetFirstHistogram()

    self.assertEqual(h.name, 'Metric')
    self.assertEqual(len(h.diagnostics), 4)

    masters = h.diagnostics[reserved_infos.MASTERS.name]
    self.assertEqual(masters.GetOnlyElement(), 'Master')

    bots = h.diagnostics[reserved_infos.BOTS.name]
    self.assertEqual(bots.GetOnlyElement(), 'Bot')

    benchmarks = h.diagnostics[reserved_infos.BENCHMARKS.name]
    self.assertEqual(benchmarks.GetOnlyElement(), 'Suite')

    point_id = h.diagnostics[reserved_infos.POINT_ID.name].GetOnlyElement()
    self.assertEqual(point_id, 1234)

    self.assertEqual(h.num_values, 1)
    self.assertEqual(h.average, 42.0)

  def testConvertWithStory(self):
    dicts = [{
        'master': 'Master',
        'bot': 'Bot',
        'test': 'Suite/Metric/Case',
        'revision': 1234,
        'value': 42.0
    }]

    histograms = legacy_json_converter.ConvertLegacyDicts(dicts)
    h = histograms.GetFirstHistogram()

    stories = h.diagnostics[reserved_infos.STORIES.name]
    self.assertEqual(stories.GetOnlyElement(), 'Case')
