# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import page_cycler
import page_sets
from telemetry import benchmark


class PageCyclerBloat(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.BloatPageSet
  options = {'pageset_repeat': 10}


class PageCyclerDhtml(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.DhtmlPageSet
  options = {'pageset_repeat': 10}


class PageCyclerIntlArFaHe(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.IntlArFaHePageSet
  options = {'pageset_repeat': 10}


@benchmark.Disabled('win')  # crbug.com/388337
class PageCyclerIntlEsFrPtBr(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.IntlEsFrPtBrPageSet
  options = {'pageset_repeat': 10}


class PageCyclerIntlHiRu(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.IntlHiRuPageSet
  options = {'pageset_repeat': 10}


@benchmark.Disabled('android', 'win')  # crbug.com/379564, crbug.com/330909
class PageCyclerIntlJaZh(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.IntlJaZhPageSet
  options = {'pageset_repeat': 10}


class PageCyclerIntlKoThVi(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.IntlKoThViPageSet
  options = {'pageset_repeat': 10}


class PageCyclerMorejs(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.MorejsPageSet
  options = {'pageset_repeat': 10}


class PageCyclerMoz(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.MozPageSet
  options = {'pageset_repeat': 10}


@benchmark.Disabled('linux', 'win')  # crbug.com/353260
class PageCyclerNetsimTop10(benchmark.Benchmark):
  """Measures load time of the top 10 sites under simulated cable network."""
  tag = 'netsim'
  test = page_cycler.PageCycler
  page_set = page_sets.Top10PageSet
  options = {
      'cold_load_percent': 100,
      'extra_wpr_args': [
          '--shaping_type=proxy',
          '--net=cable'
      ],
      'pageset_repeat': 5,
  }

  def __init__(self):
    super(PageCyclerNetsimTop10, self).__init__()
    # TODO: This isn't quite right.
    # This option will still apply to page cyclers that run after this one.
    self.test.clear_cache_before_each_run = True


class PageCyclerTop10Mobile(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.Top10MobilePageSet
  options = {'pageset_repeat': 10}


@benchmark.Disabled('android')
class PageCyclerKeyMobileSites(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.KeyMobileSitesPageSet
  options = {'pageset_repeat': 10}


@benchmark.Disabled('android')  # crbug.com/357326
class PageCyclerToughLayoutCases(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.ToughLayoutCasesPageSet
  options = {'pageset_repeat': 10}


# crbug.com/273986: This test is really flakey on xp.
# cabug.com/341843: This test is always timing out on Android.
@benchmark.Disabled('android', 'win')
class PageCyclerTypical25(benchmark.Benchmark):
  test = page_cycler.PageCycler
  page_set = page_sets.Typical25PageSet
  options = {'pageset_repeat': 10}
