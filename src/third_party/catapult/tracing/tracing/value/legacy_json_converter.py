# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


def ConvertLegacyDicts(dicts):
  """Convert legacy JSON dicts to Histograms.

  Args:
    dicts: A list of v0 JSON dicts

  Returns:
    A HistogramSet containing equivalent histograms and diagnostics
  """
  if len(dicts) < 1:
    return histogram_set.HistogramSet()

  first_dict = dicts[0]
  master = first_dict['master']
  bot = first_dict['bot']
  suite = first_dict['test'].split('/')[0]
  point_id = first_dict['revision']

  hs = histogram_set.HistogramSet()

  for d in dicts:
    assert d['master'] == master
    assert d['bot'] == bot

    test_parts = d['test'].split('/')
    assert test_parts[0] == suite
    name = test_parts[1]

    # TODO(843643): Generalize this
    assert 'units' not in d
    # TODO(861822): Port this to CreateHistogram
    h = histogram.Histogram(name, 'unitless')
    h.AddSample(d['value'])
    # TODO(876379): Support more than three components
    if len(test_parts) == 3:
      h.diagnostics[reserved_infos.STORIES.name] = generic_set.GenericSet(
          [test_parts[2]])

    hs.AddHistogram(h)

  hs.AddSharedDiagnostic(reserved_infos.MASTERS.name, generic_set.GenericSet(
      [master]))
  hs.AddSharedDiagnostic(reserved_infos.BOTS.name, generic_set.GenericSet(
      [bot]))
  hs.AddSharedDiagnostic(reserved_infos.BENCHMARKS.name, generic_set.GenericSet(
      [suite]))
  hs.AddSharedDiagnostic(reserved_infos.POINT_ID.name, generic_set.GenericSet(
      [point_id]))

  return hs
