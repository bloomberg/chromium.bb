# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import page_cycler
from telemetry import test


class PageCyclerBloat(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/bloat.py'
  options = {'pageset_repeat': 10}


class PageCyclerDhtml(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/dhtml.py'
  options = {'pageset_repeat': 10}


class PageCyclerIntlArFaHe(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ar_fa_he.json'
  options = {'pageset_repeat': 10}


class PageCyclerIntlEsFrPtBr(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_es_fr_pt-BR.json'
  options = {'pageset_repeat': 10}


class PageCyclerIntlHiRu(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_hi_ru.json'
  options = {'pageset_repeat': 10}


@test.Disabled('win')  # crbug.com/330909
class PageCyclerIntlJaZh(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ja_zh.json'
  options = {'pageset_repeat': 10}


class PageCyclerIntlKoThVi(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/intl_ko_th_vi.json'
  options = {'pageset_repeat': 10}


class PageCyclerMorejs(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/morejs.py'
  options = {'pageset_repeat': 10}


class PageCyclerMoz(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/page_cycler/moz.py'
  options = {'pageset_repeat': 10}


class PageCyclerNetsimTop10(test.Test):
  """Measures load time of the top 10 sites under simulated cable network."""
  tag = 'netsim'
  test = page_cycler.PageCycler
  page_set = 'page_sets/top_10.py'
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


class PageCyclerTop10Mobile(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/top_10_mobile.json'
  options = {'pageset_repeat': 10}


class PageCyclerKeyMobileSites(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/key_mobile_sites.json'
  options = {'pageset_repeat': 10}


class PageCyclerToughLayoutCases(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/tough_layout_cases.json'
  options = {'pageset_repeat': 10}


# crbug.com/273986: This test is really flakey on xp.
# cabug.com/341843: This test is always timing out on Android.
@test.Disabled('android', 'win')
class PageCyclerTypical25(test.Test):
  test = page_cycler.PageCycler
  page_set = 'page_sets/typical_25.json'
  options = {'pageset_repeat': 10}
