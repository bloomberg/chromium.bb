# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class FlashEnergyCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(FlashEnergyCasesPage, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/flash_energy_cases.json'


class FlashEnergyCasesPageSet(page_set_module.PageSet):

  """ Pages with flash heavy content. """

  def __init__(self):
    super(FlashEnergyCasesPageSet, self).__init__(
      user_agent_type='desktop',
      archive_data_file='data/flash_energy_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
        # pylint: disable=C0301
        'http://v.qq.com/cover/d/dm9vn9cnsn2v2gx.html?ptag=v.newplaybutton.program.dpjd',
        'http://videos.huffingtonpost.com/politics/protesters-march-to-hong-kong-leaders-home-518476075',
        'http://nos.nl/video/712855-eerste-beelden-na-de-aanslag-in-canada.html',
        # Why: Large flash ad
        'http://forum.gazeta.pl/forum/f,17007,Polityka_i_Gospodarka.html',
        # Why: Multiple flash ads
        'http://tinypic.com/',
        'https://www.facebook.com/video.php?v=524346051035153&set=vb.134474600022302&type=2&theater'
    ]

    for url in urls_list:
      self.AddUserStory(FlashEnergyCasesPage(url, self))
