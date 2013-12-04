# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

from telemetry import test

from measurements import page_cycler


class PageCyclerBloat(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/bloat.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerDhtml(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/dhtml.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIndexeddb(test.Test):
  tag = 'indexed_db'
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/indexed_db/basic_insert.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIntlArFaHe(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ar_fa_he.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIntlEsFrPtBr(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_es_fr_pt-BR.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIntlHiRu(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_hi_ru.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIntlJaZh(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ja_zh.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerIntlKoThVi(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ko_th_vi.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerMorejs(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/morejs.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerMoz(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/moz.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerNetsimTop10(test.Test):
  """Measures load time of the top 10 sites under simulated cable network."""
  tag = 'netsim'
  test = page_cycler.PageCycler
  page_set = 'page_sets/top_10.json'
  options = {
    'cold_load_percent': 100,
    'extra_wpr_args': [
      '--shaping_type=proxy',
      '--net=cable'
      ],
    'pageset_repeat_iters': 5,
    }

  def __init__(self):
    super(PageCyclerNetsimTop10, self).__init__()
    # TODO: This isn't quite right.
    # This option will still apply to page cyclers that run after this one.
    self.test.clear_cache_before_each_run = True


class PageCyclerTop10Mobile(test.Test):
  enabled = False  # Fails on Android.

  test = page_cycler.PageCycler
  page_set = 'page_sets/top_10_mobile.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerToughLayoutCases(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/tough_layout_cases.json'
  options = {'pageset_repeat_iters': 10}


class PageCyclerTypical25(test.Test):
  # crbug.com/273986: This test is really flakey on xp.
  enabled = not sys.platform.startswith('win')
  test = page_cycler.PageCycler
  page_set = 'page_sets/typical_25.json'
  options = {'pageset_repeat_iters': 10}
