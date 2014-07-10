# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class IntlHiRuPage(page_module.Page):

  def __init__(self, url, page_set):
    super(IntlHiRuPage, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/intl_hi_ru.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class IntlHiRuPageSet(page_set_module.PageSet):

  """ Popular pages in Hindi and Russian. """

  def __init__(self):
    super(IntlHiRuPageSet, self).__init__(
      user_agent_type='desktop',
      archive_data_file='data/intl_hi_ru.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      # Why: #12 site in Russia
      'http://www.rambler.ru/',
      'http://apeha.ru/',
      # pylint: disable=C0301
      'http://yandex.ru/yandsearch?lr=102567&text=%D0%9F%D0%BE%D0%B3%D0%BE%D0%B4%D0%B0',
      'http://photofile.ru/',
      'http://ru.wikipedia.org/',
      'http://narod.yandex.ru/',
      # Why: #15 in Russia
      'http://rutracker.org/forum/index.php',
      'http://hindi.webdunia.com/',
      # Why: #49 site in India
      'http://hindi.oneindia.in/',
      # Why: #9 site in India
      'http://www.indiatimes.com/',
      # Why: #2 site in India
      'http://news.google.co.in/nwshp?tab=in&hl=hi'
    ]

    for url in urls_list:
      self.AddPage(IntlHiRuPage(url, self))
