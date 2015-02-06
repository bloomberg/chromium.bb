# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from telemetry.web_perf import timeline_based_measurement
from telemetry import benchmark


@benchmark.Disabled('win')  # crbug.com/416502
class V8GarbageCollectionCases(benchmark.Benchmark):
  """Measure V8 metrics on the garbage collection cases."""
  def CreateTimelineBasedMeasurementOptions(self):
    # TODO(ernstm): Remove v8-overhead when benchmark relevant v8 events become
    # available in the 'benchmark' category.
    return timeline_based_measurement.Options(overhead_level='v8-overhead')

  @classmethod
  def Name(cls):
    return 'v8.garbage_collection_cases'

  page_set = page_sets.GarbageCollectionCasesPageSet
