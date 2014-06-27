# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark
from measurements import memory_pressure
import page_sets


@benchmark.Enabled('has tabs')
class MemoryPressure(benchmark.Benchmark):
  test = memory_pressure.MemoryPressure
  page_set = page_sets.Typical25PageSet
  options = {'pageset_repeat': 6}
