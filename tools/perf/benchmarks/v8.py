# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from telemetry.web_perf import timeline_based_measurement
from telemetry import benchmark

class V8GarbageCollectionCases(benchmark.Benchmark):
  """Measure V8 metrics on the garbage collection cases."""
  test = timeline_based_measurement.TimelineBasedMeasurement
  page_set = page_sets.GarbageCollectionCasesPageSet

  # TODO(ernstm): Remove this argument when benchmark relevant v8 events become
  # available in the 'benchmark' category.
  @classmethod
  def SetArgumentDefaults(cls, parser):
    parser.set_defaults(overhead_level='v8-overhead')
