# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

from page_sets import top_pages


def _IssueMarkerAndScroll(action_runner):
  interaction = action_runner.BeginGestureInteraction(
      'ScrollAction')
  action_runner.ScrollPage()
  interaction.End()


def _CreatePageClassWithSmoothInteractions(page_cls):
  class DerivedSmoothPage(page_cls):  # pylint: disable=W0232

    def RunPageInteractions(self, action_runner):
      _IssueMarkerAndScroll(action_runner)
  return DerivedSmoothPage


class TopSmoothPage(page_module.Page):

  def __init__(self, url, page_set, name='', credentials=None):
    super(TopSmoothPage, self).__init__(
        url=url, page_set=page_set, name=name,
        credentials_path='data/credentials.json')
    self.user_agent_type = 'desktop'
    self.credentials = credentials

  def RunPageInteractions(self, action_runner):
    _IssueMarkerAndScroll(action_runner)


class GmailSmoothPage(top_pages.GmailPage):

  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('''
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });''')
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction')
    action_runner.ScrollElement(
        element_function='window.__scrollableElementForTelemetry')
    interaction.End()


class GmailMouseScrollPage(top_pages.GmailPage):

  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('''
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });''')
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    scrollbar_x, start_y, end_y = self._CalculateScrollBarRatios(action_runner)

    interaction = action_runner.BeginGestureInteraction(
        'DragAction')
    action_runner.DragPage(left_start_ratio=scrollbar_x,
        top_start_ratio=start_y, left_end_ratio=scrollbar_x,
        top_end_ratio=end_y, speed_in_pixels_per_second=100,
        element_function='window.__scrollableElementForTelemetry')
    interaction.End()

  def CanRunOnBrowser(self, browser_info):
    return (browser_info._browser._platform_backend.platform.GetOSName() !=
        'android')

  def _CalculateScrollBarRatios(self, action_runner):
    viewport_height = float(action_runner.EvaluateJavaScript(
        'window.__scrollableElementForTelemetry.clientHeight'))
    content_height = float(action_runner.EvaluateJavaScript(
        'window.__scrollableElementForTelemetry.scrollHeight'))
    viewport_width = float(action_runner.EvaluateJavaScript(
        'window.__scrollableElementForTelemetry.offsetWidth'))
    scrollbar_width = float(action_runner.EvaluateJavaScript('''
        window.__scrollableElementForTelemetry.offsetWidth -
        window.__scrollableElementForTelemetry.scrollWidth'''))

    # This calculation is correct only when the element doesn't have border or
    # padding or scroll buttons (eg: gmail mail element).
    scrollbar_start_mid_y = viewport_height / (2 * content_height)
    scrollbar_end_mid_y = 1 - scrollbar_start_mid_y
    scrollbar_mid_x_offset = scrollbar_width / (2 * viewport_width)
    scrollbar_mid_x = 1 - scrollbar_mid_x_offset
    return scrollbar_mid_x, scrollbar_start_mid_y, scrollbar_end_mid_y


class GoogleCalendarSmoothPage(top_pages.GoogleCalendarPage):

  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction')
    action_runner.ScrollElement(selector='#scrolltimedeventswk')
    interaction.End()


class GoogleDocSmoothPage(top_pages.GoogleDocPage):

  """ Why: productivity, top google properties; Sample doc in the link """

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction')
    action_runner.ScrollElement(selector='.kix-appview-editor')
    interaction.End()


class GoogleMapsPage(top_pages.GoogleMapsPage):

  """ Why: productivity, top google properties; Supports drag gestures """

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'DragAction', repeatable=True)
    action_runner.DragPage(left_start_ratio=0.5, top_start_ratio=0.75,
                           left_end_ratio=0.75, top_end_ratio=0.5)
    interaction.End()
    action_runner.Wait(2)
    interaction = action_runner.BeginGestureInteraction(
        'DragAction', repeatable=True)
    action_runner.DragPage(left_start_ratio=0.5, top_start_ratio=0.5,
                           left_end_ratio=0.35, top_end_ratio=0.75)
    interaction.End()
    # TODO(ssid): Add zoom gestures after fixing bug crbug.com/462214.


class ESPNSmoothPage(top_pages.ESPNPage):

  """ Why: #1 sports """

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction')
    action_runner.ScrollPage(left_start_ratio=0.1)
    interaction.End()


class Top25SmoothPageSet(page_set_module.PageSet):

  """ Pages hand-picked for 2012 CrOS scrolling tuning efforts. """

  def __init__(self):
    super(Top25SmoothPageSet, self).__init__(
        user_agent_type='desktop',
        archive_data_file='data/top_25_smooth.json',
        bucket=page_set_module.PARTNER_BUCKET)

    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.GoogleWebSearchPage)(self))
    self.AddUserStory(GmailSmoothPage(self))
    self.AddUserStory(GmailMouseScrollPage(self))
    self.AddUserStory(GoogleCalendarSmoothPage(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.GoogleImageSearchPage)(self))
    self.AddUserStory(GoogleDocSmoothPage(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.GooglePlusPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.YoutubePage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.BlogspotPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.WordpressPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.FacebookPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.LinkedinPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.WikipediaPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.TwitterPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.PinterestPage)(self))
    self.AddUserStory(ESPNSmoothPage(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.WeatherPage)(self))
    self.AddUserStory(_CreatePageClassWithSmoothInteractions(
        top_pages.YahooGamesPage)(self))
    self.AddUserStory(GoogleMapsPage(self))

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
        'http://techcrunch.com'
    ]

    for url in other_urls:
      self.AddUserStory(TopSmoothPage(url, self))
