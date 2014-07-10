# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class KeySearchMobilePage(page_module.Page):

  def __init__(self, url, page_set):
    super(KeySearchMobilePage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/key_search_mobile.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class KeySearchMobilePageSet(page_set_module.PageSet):

  """ Key mobile search queries on google """

  def __init__(self):
    super(KeySearchMobilePageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='mobile',
      archive_data_file='data/key_search_mobile.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    urls_list = [
      # Why: An empty page should be as snappy as possible
      'http://www.google.com/',
      # Why: A reasonable search term with no images or ads usually
      'https://www.google.com/search?q=science',
      # Why: A reasonable search term with images but no ads usually
      'http://www.google.com/search?q=orange',
      # Why: An address search
      # pylint: disable=C0301
      'https://www.google.com/search?q=1600+Amphitheatre+Pkwy%2C+Mountain+View%2C+CA',
      # Why: A search for a known actor
      'http://www.google.com/search?q=tom+hanks',
      # Why: A search for weather
      'https://www.google.com/search?q=weather+94110',
      # Why: A search for a stock
      'http://www.google.com/search?q=goog',
      # Why: Charts
      'https://www.google.com/search?q=population+of+california',
      # Why: Flights
      'http://www.google.com/search?q=sfo+jfk+flights',
      # Why: Movie showtimes
      'https://www.google.com/search?q=movies+94110',
      # Why: A tip calculator
      'http://www.google.com/search?q=tip+on+100+bill',
      # Why: Time
      'https://www.google.com/search?q=time+in+san+francisco',
      # Why: Definitions
      'http://www.google.com/search?q=define+define',
      # Why: Local results
      'https://www.google.com/search?q=burritos+94110',
      # Why: Graph
      'http://www.google.com/search?q=x^3'
    ]

    for url in urls_list:
      self.AddPage(KeySearchMobilePage(url, self))
