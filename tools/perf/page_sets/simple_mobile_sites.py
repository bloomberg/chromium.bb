# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SimplePage(page_module.Page):

  def __init__(self, url, page_set):
    super(SimplePage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.archive_data_file = 'data/simple_mobile_sites.json'
    self.disabled = 'Times out on Windows; crbug.com/400922'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)

class SimpleScrollPage(SimplePage):

  def __init__(self, url, page_set):
    super(SimpleScrollPage, self).__init__(url=url, page_set=page_set)

  def RunSmoothness(self, action_runner):
    # Make the scroll longer to reduce noise.
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=300)
    interaction.End()

class SimpleMobileSitesPageSet(page_set_module.PageSet):

  """ Simple mobile sites """

  def __init__(self):
    super(SimpleMobileSitesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='tablet_10_inch',
      archive_data_file='data/simple_mobile_sites.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    scroll_page_list = [
      # Why: Scrolls moderately complex pages (up to 60 layers)
      'http://www.ebay.co.uk/',
      'https://www.flickr.com/',
      'http://www.apple.com/mac/',
      'http://www.nyc.gov',
      'http://m.nytimes.com/',
      'https://www.yahoo.com/',
      'http://m.us.wsj.com/',
    ]

    for url in scroll_page_list:
      self.AddPage(SimpleScrollPage(url, self))

