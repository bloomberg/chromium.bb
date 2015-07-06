# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets import diagonal_scrolling_supported_shared_state

from telemetry.page import page as page_module
from telemetry import story


class ToughScrollingWhileZoomedInCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughScrollingWhileZoomedInCasesPage, self).__init__(
        url=url,
        page_set=page_set,
        shared_page_state_class=(
            diagonal_scrolling_supported_shared_state.
            DiagonalScrollingSupportedSharedState))

  def RunPageInteractions(self, action_runner):
    # First, zoom into the page
    action_runner.PinchPage(
        scale_factor=20.0,
        speed_in_pixels_per_second=10000)
    # 20.0 was chosen because at the time it was close to the maximum.
    # The more zoomed in, the more noticable the tile rasterization.
    #
    # 10,000 was chosen to complete this pre-step quickly.

    # Then start measurements
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # And begin the diagonal scroll
      action_runner.ScrollPage(
          direction='downright',
          # 10,000 was chosen because it is fast enough to completely stress the
          # rasterization (on a Nexus 5) without saturating results.
          speed_in_pixels_per_second=10000)


class ToughScrollingWhileZoomedInCasesPageSet(story.StorySet):
  """
  Description: A collection of difficult scrolling tests
  """

  def __init__(self):
    super(ToughScrollingWhileZoomedInCasesPageSet, self).__init__(
        archive_data_file='data/tough_pinch_zoom_cases.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    # The following urls were chosen because they tend to have >15%
    # mean_pixels_approximated at this scrolling speed.
    urls_list = [
        'file://tough_scrolling_cases/background_fixed.html',
        'file://tough_scrolling_cases/fixed_nonstacking.html',
        'file://tough_scrolling_cases/iframe_scrolls.html',
        'file://tough_scrolling_cases/wheel_div_prevdefault.html'
    ]

    for url in urls_list:
      self.AddStory(ToughScrollingWhileZoomedInCasesPage(url, self))
