# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import memory
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class MemoryMobile(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.MobileMemoryPageSet


@benchmark.Disabled
class MemoryTop25(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.Top25PageSet


@benchmark.Disabled
class Reload2012Q3(benchmark.Benchmark):
  tag = 'reload'
  test = memory.Memory
  page_set = page_sets.Top2012Q3PageSet


@benchmark.Disabled  # crbug.com/371153
class MemoryToughDomMemoryCases(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.ToughDomMemoryCasesPageSet
