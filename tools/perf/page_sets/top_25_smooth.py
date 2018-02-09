# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

from page_sets import top_pages


def _IssueMarkerAndScroll(action_runner, scroll_forever):
  with action_runner.CreateGestureInteraction('ScrollAction'):
    action_runner.ScrollPage()
    if scroll_forever:
      while True:
        action_runner.ScrollPage(direction='up')
        action_runner.ScrollPage(direction='down')


def _CreatePageClassWithSmoothInteractions(page_cls):

  class DerivedSmoothPage(page_cls):  # pylint: disable=no-init

    def RunPageInteractions(self, action_runner):
      action_runner.Wait(1)
      _IssueMarkerAndScroll(action_runner, self.story_set.scroll_forever)

  return DerivedSmoothPage


class TopSmoothPage(page_module.Page):

  def __init__(self, url, page_set, name='', extra_browser_args=None):
    if name == '':
      name = url
    super(TopSmoothPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        shared_page_state_class=shared_page_state.SharedDesktopPageState,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    _IssueMarkerAndScroll(action_runner, self.story_set.scroll_forever)


class GmailSmoothPage(top_pages.GmailPage):
  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript("""
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });""")
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          element_function='window.__scrollableElementForTelemetry')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up',
              element_function='window.__scrollableElementForTelemetry')
          action_runner.ScrollElement(
              direction='down',
              element_function='window.__scrollableElementForTelemetry')


class GoogleCalendarSmoothPage(top_pages.GoogleCalendarPage):
  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#scrolltimedeventswk')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='#scrolltimedeventswk')
          action_runner.ScrollElement(
              direction='down', selector='#scrolltimedeventswk')


class GoogleDocSmoothPage(top_pages.GoogleDocPage):
  """ Why: productivity, top google properties; Sample doc in the link """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.kix-appview-editor')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.kix-appview-editor')
          action_runner.ScrollElement(
              direction='down', selector='.kix-appview-editor')


class ESPNSmoothPage(top_pages.ESPNPage):
  """ Why: #1 sports """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


class Top25SmoothPageSet(story.StorySet):
  """ Pages hand-picked for 2012 CrOS scrolling tuning efforts. """

  def __init__(self, scroll_forever=False):
    super(Top25SmoothPageSet, self).__init__(
        archive_data_file='data/top_25_smooth.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    self.scroll_forever = scroll_forever

    desktop_state_class = shared_page_state.SharedDesktopPageState

    smooth_page_classes = [
        GmailSmoothPage,
        GoogleCalendarSmoothPage,
        GoogleDocSmoothPage,
        ESPNSmoothPage,
    ]

    for smooth_page_class in smooth_page_classes:
      self.AddStory(
          smooth_page_class(self, shared_page_state_class=desktop_state_class))

    non_smooth_page_classes = [
        top_pages.GoogleWebSearchPage,
        top_pages.GoogleImageSearchPage,
        top_pages.GooglePlusPage,
        top_pages.YoutubePage,
        top_pages.BlogspotPage,
        top_pages.WordpressPage,
        top_pages.FacebookPage,
        top_pages.LinkedinPage,
        top_pages.WikipediaPage,
        top_pages.TwitterPage,
        top_pages.PinterestPage,
        top_pages.WeatherPage,
        top_pages.YahooGamesPage,
    ]

    for non_smooth_page_class in non_smooth_page_classes:
      self.AddStory(
          _CreatePageClassWithSmoothInteractions(non_smooth_page_class)(
              self, shared_page_state_class=desktop_state_class))

    other_urls = [
        # Why: #1 news worldwide (Alexa global)
        'http://news.yahoo.com',
        # Why: #2 news worldwide
        'http://www.cnn.com',
        # Why: #1 world commerce website by visits; #3 commerce in the US by
        # time spent
        'http://www.amazon.com',
        # Why: #1 commerce website by time spent by users in US
        'http://www.ebay.com',
        # Why: #1 Alexa recreation
        'http://booking.com',
        # Why: #1 Alexa reference
        'http://answers.yahoo.com',
        # Why: #1 Alexa sports
        'http://sports.yahoo.com/',
        # Why: top tech blog
        'http://techcrunch.com',
    ]

    for url in other_urls:
      self.AddStory(TopSmoothPage(url, self))
