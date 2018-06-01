# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class SimpleScrollPage(page_module.Page):

  def __init__(self, name, url, page_set):
    super(SimpleScrollPage, self).__init__(
        name=name,
        url=url,
        page_set=page_set,
        shared_page_state_class=shared_page_state.Shared10InchTabletPageState)

  def RunNavigateSteps(self, action_runner):
    super(SimpleScrollPage, self).RunNavigateSteps(action_runner)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)

  def RunPageInteractions(self, action_runner):
    # Make the scroll longer to reduce noise.
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=300)


class SimpleMobileSitesPageSet(story.StorySet):
  """ Simple mobile sites """

  def __init__(self):
    super(SimpleMobileSitesPageSet, self).__init__(
      archive_data_file='data/simple_mobile_sites.json',
      cloud_storage_bucket=story.PUBLIC_BUCKET)

    scroll_page_list = [
      # Why: Scrolls moderately complex pages (up to 60 layers)
      ('ebay_scroll', 'http://www.ebay.co.uk/'),
      ('flickr_scroll', 'https://www.flickr.com/'),
      ('nyc_gov_scroll', 'http://www.nyc.gov'),
      ('nytimes_scroll', 'http://m.nytimes.com/')
    ]

    for name,url in scroll_page_list:
      self.AddStory(SimpleScrollPage(name=name,
                                     url=url,
                                     page_set=self))
