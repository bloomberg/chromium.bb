# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import page_cycler
import page_sets
from telemetry import benchmark


class _PageCycler(benchmark.Benchmark):
  options = {'pageset_repeat': 6}

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--v8-object-stats',
        action='store_true',
        help='Enable detailed V8 object statistics.')

    parser.add_option('--report-speed-index',
        action='store_true',
        help='Enable the speed index metric.')

    parser.add_option('--cold-load-percent', type='int', default=50,
                      help='%d of page visits for which a cold load is forced')

  def CreatePageTest(self, options):
    return page_cycler.PageCycler(
        page_repeat = options.page_repeat,
        pageset_repeat = options.pageset_repeat,
        cold_load_percent = options.cold_load_percent,
        record_v8_object_stats = options.v8_object_stats,
        report_speed_index = options.report_speed_index)


# This is an old page set, we intend to remove it after more modern benchmarks
# work on CrOS.
@benchmark.Enabled('chromeos')
class PageCyclerDhtml(_PageCycler):
  page_set = page_sets.DhtmlPageSet


class PageCyclerIntlArFaHe(_PageCycler):
  page_set = page_sets.IntlArFaHePageSet


@benchmark.Disabled('win')  # crbug.com/388337
class PageCyclerIntlEsFrPtBr(_PageCycler):
  page_set = page_sets.IntlEsFrPtBrPageSet


class PageCyclerIntlHiRu(_PageCycler):
  page_set = page_sets.IntlHiRuPageSet


@benchmark.Disabled('android', 'win')  # crbug.com/379564, crbug.com/330909
class PageCyclerIntlJaZh(_PageCycler):
  page_set = page_sets.IntlJaZhPageSet


@benchmark.Disabled('xp')  # crbug.com/434366
class PageCyclerIntlKoThVi(_PageCycler):
  page_set = page_sets.IntlKoThViPageSet


class PageCyclerMorejs(_PageCycler):
  page_set = page_sets.MorejsPageSet


# This is an old page set, we intend to remove it after more modern benchmarks
# work on CrOS.
@benchmark.Enabled('chromeos')
class PageCyclerMoz(_PageCycler):
  page_set = page_sets.MozPageSet


@benchmark.Disabled('linux', 'win', 'mac')  # crbug.com/353260
class PageCyclerNetsimTop10(_PageCycler):
  """Measures load time of the top 10 sites under simulated cable network."""
  tag = 'netsim'
  page_set = page_sets.Top10PageSet
  options = {
      'cold_load_percent': 100,
      'extra_wpr_args_as_string': '--shaping_type=proxy --net=cable',
      'pageset_repeat': 6,
  }

  def CreatePageTest(self, options):
    return page_cycler.PageCycler(
        page_repeat = options.page_repeat,
        pageset_repeat = options.pageset_repeat,
        cold_load_percent = options.cold_load_percent,
        record_v8_object_stats = options.v8_object_stats,
        report_speed_index = options.report_speed_index,
        clear_cache_before_each_run = True)


@benchmark.Enabled('android')
class PageCyclerTop10Mobile(_PageCycler):
  page_set = page_sets.Top10MobilePageSet


@benchmark.Disabled
class PageCyclerKeyMobileSites(_PageCycler):
  page_set = page_sets.KeyMobileSitesSmoothPageSet


@benchmark.Disabled('android')  # crbug.com/357326
class PageCyclerToughLayoutCases(_PageCycler):
  page_set = page_sets.ToughLayoutCasesPageSet


# crbug.com/273986: This test is really flakey on xp.
# cabug.com/341843: This test is always timing out on Android.
@benchmark.Disabled('android', 'win')
class PageCyclerTypical25(_PageCycler):
  page_set = page_sets.Typical25PageSet
