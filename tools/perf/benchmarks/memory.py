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


class MemoryTop7Stress(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.Top7StressPageSet


@benchmark.Disabled
class Reload2012Q3(benchmark.Benchmark):
  tag = 'reload'
  test = memory.Memory
  page_set = page_sets.Top2012Q3StressPageSet


@benchmark.Disabled  # crbug.com/371153
class MemoryToughDomMemoryCases(benchmark.Benchmark):
  test = memory.Memory
  page_set = page_sets.ToughDomMemoryCasesPageSet


@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
@benchmark.Enabled('has tabs')
class TypicalMobileSites(benchmark.Benchmark):
  """Long memory test."""
  test = memory.Memory
  page_set = page_sets.TypicalMobileSitesPageSet
  options = {'pageset_repeat': 15}
