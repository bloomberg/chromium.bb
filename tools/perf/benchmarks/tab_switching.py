# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from measurements import tab_switching
import page_sets


@benchmark.Enabled('has tabs')
class TabSwitchingTop10(benchmark.Benchmark):
  test = tab_switching.TabSwitching
  page_set = page_sets.Top10PageSet


@benchmark.Enabled('has tabs')
class TabSwitchingTypical25(benchmark.Benchmark):
  test = tab_switching.TabSwitching
  page_set = page_sets.Typical25PageSet


@benchmark.Enabled('has tabs')
class TabSwitchingFiveBlankTabs(benchmark.Benchmark):
  test = tab_switching.TabSwitching
  page_set = page_sets.FiveBlankPagesPageSet
  options = {'pageset_repeat': 10}


@benchmark.Enabled('has tabs')
class TabSwitchingToughEnergyCases(benchmark.Benchmark):
  test = tab_switching.TabSwitching
  page_set = page_sets.ToughEnergyCasesPageSet
  options = {'pageset_repeat': 10}
