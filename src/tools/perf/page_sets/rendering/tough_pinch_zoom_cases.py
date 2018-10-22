# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import story

from page_sets.rendering import rendering_shared_state
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms
from page_sets.login_helpers import linkedin_login
from page_sets.login_helpers import google_login


class ToughPinchZoomPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  # TODO(crbug.com/851499): expand supported_platforms to both desktop & mobile
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOUGH_PINCH_ZOOM]

  def __init__(self,
               page_set,
               name_suffix='',
               shared_page_state_class=(
                   rendering_shared_state.DesktopRenderingSharedState),
               extra_browser_args=None):
    super(ToughPinchZoomPage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

    self.target_scale_factor = page_set.target_scale_factor

  def RunPinchGesture(self, action_runner, left_anchor_ratio=0.5,
                      top_anchor_ratio=0.5, scale_factor=None,
                      speed_in_pixels_per_second=800):
      with action_runner.CreateGestureInteraction('PinchAction',
                                                  repeatable=True):
        action_runner.PinchPage(
            left_anchor_ratio=left_anchor_ratio,
            top_anchor_ratio=top_anchor_ratio,
            scale_factor=scale_factor,
            speed_in_pixels_per_second=speed_in_pixels_per_second)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    for _ in xrange(0, 3):
      current_scale_factor = self.target_scale_factor
      self.RunPinchGesture(action_runner, scale_factor=current_scale_factor)
      while current_scale_factor > 1.0:
        current_scale_factor *= 1/2.0
        self.RunPinchGesture(action_runner, scale_factor=1/2.0)


class GoogleSearchPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: top google property; a google tab is often open. """

  BASE_NAME = 'google_search_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def RunNavigateSteps(self, action_runner):
    super(GoogleSearchPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GmailPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: productivity, top google properties """

  BASE_NAME = 'gmail_pinch'
  YEAR = '2018'
  URL = 'https://mail.google.com/mail/'

  def RunNavigateSteps(self, action_runner):
    google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GmailPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendarPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: productivity, top google properties """

  BASE_NAME = 'google_calendar_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/calendar/'

  def RunNavigateSteps(self, action_runner):
    google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendarPinchZoom2018Page, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForElement('span[class~="sm8sCf"]')


class GoogleImagePinchZoom2018Page(ToughPinchZoomPage):

  """ Why: tough image case; top google properties """

  BASE_NAME = 'google_image_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'


class YoutubePinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #3 (Alexa global) """

  BASE_NAME = 'youtube_pinch'
  YEAR = '2018'
  URL = 'http://www.youtube.com'

  def RunNavigateSteps(self, action_runner):
    super(YoutubePinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='#buttons')


class BlogSpotPinchZoom2018Page(ToughPinchZoomPage):

  """
  Why: #11 (Alexa global), google property; some blogger layouts have infinite
  scroll but more interesting
  """

  BASE_NAME = 'blogspot_pinch'
  YEAR = '2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def RunNavigateSteps(self, action_runner):
    super(BlogSpotPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('div[class="searchBox"]')


class FacebookPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: top social,Public profile """

  BASE_NAME = 'facebook_pinch'
  YEAR = '2018'
  URL = 'http://www.facebook.com/barackobama'

  def RunNavigateSteps(self, action_runner):
    super(FacebookPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class LinkedinPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #12 (Alexa global),Public profile """

  BASE_NAME = 'linkedin_pinch'
  YEAR = '2018'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginDesktopAccount(action_runner, 'linkedin')
    super(LinkedinPinchZoom2018Page, self).RunNavigateSteps(action_runner)


class TwitterPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #8 (Alexa global),Picked an interesting page """

  BASE_NAME = 'twitter_pinch'
  YEAR = '2018'
  URL = 'https://twitter.com/katyperry'

  def RunNavigateSteps(self, action_runner):
    super(TwitterPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.ProfileNav')


class ESPNPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 sports """

  BASE_NAME = 'espn_pinch'
  YEAR = '2018'
  URL = 'http://espn.go.com/nba'


class AccuWeatherPinchZoom2018Page(ToughPinchZoomPage):
  """ Why: #2 weather according to Alexa """
  BASE_NAME = 'accu_weather_pinch'
  YEAR = '2018'
  URL = 'https://www.accuweather.com/en/us/new-york-ny/10017/weather-forecast/349727'


class TwitchPinchZoom2018Page(ToughPinchZoomPage):
  """ Why: #1 games according to Alexa  """
  BASE_NAME = 'twitch_pinch'
  YEAR = '2018'
  URL = 'https://www.twitch.tv'


class YahooNewsPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 news worldwide (Alexa global) """

  BASE_NAME = 'yahoo_news_pinch'
  YEAR = '2018'
  URL = 'http://news.yahoo.com'


class CnnPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #2 news worldwide """

  BASE_NAME = 'cnn_pinch'
  YEAR = '2018'
  URL = 'http://www.cnn.com'


class AmazonPinchZoom2018Page(ToughPinchZoomPage):

  """
  Why: #1 world commerce website by visits; #3 commerce in the US by
  time spent
  """

  BASE_NAME = 'amazon_pinch'
  YEAR = '2018'
  URL = 'http://www.amazon.com'


class EBayPinchZoom2018Page(ToughPinchZoomPage):

  """  Why: #1 commerce website by time spent by users in US"""

  BASE_NAME = 'ebay_pinch'
  YEAR = '2018'
  URL = 'http://www.ebay.com'


class BookingPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 Alexa recreation"""

  BASE_NAME = 'booking_pinch'
  YEAR = '2018'
  URL = 'http://booking.com'


class YahooSportsPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 Alexa sports"""
  BASE_NAME = 'yahoo_sports_pinch'
  YEAR = '2018'
  URL = 'http://sports.yahoo.com/'


# TODO(crbug.com/760553):remove this class after
# smoothness.tough_pinch_zoom_cases benchmark is completely
# replaced by rendering benchmarks
class ToughPinchZoomCasesPageSet(story.StorySet):

  """ Set of pages that are tricky to pinch-zoom """

  def __init__(self, target_scale_factor):
    super(ToughPinchZoomCasesPageSet, self).__init__(
      archive_data_file='../data/tough_pinch_zoom_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    self.target_scale_factor = target_scale_factor

    self.AddStory(GoogleSearchPinchZoom2018Page(
        page_set=self))
    self.AddStory(GmailPinchZoom2018Page(
        page_set=self))
    self.AddStory(GoogleCalendarPinchZoom2018Page(
        page_set=self))
    self.AddStory(GoogleImagePinchZoom2018Page(
        page_set=self))
    self.AddStory(YoutubePinchZoom2018Page(
        page_set=self))
    self.AddStory(BlogSpotPinchZoom2018Page(
        page_set=self))
    self.AddStory(FacebookPinchZoom2018Page(
        page_set=self))
    self.AddStory(LinkedinPinchZoom2018Page(
        page_set=self))
    self.AddStory(TwitterPinchZoom2018Page(
        page_set=self))
    self.AddStory(ESPNPinchZoom2018Page(
        page_set=self))
    self.AddStory(TwitchPinchZoom2018Page(
        page_set=self))
    self.AddStory(YahooNewsPinchZoom2018Page(
        page_set=self))
    self.AddStory(CnnPinchZoom2018Page(
        page_set=self))
    self.AddStory(AmazonPinchZoom2018Page(
        page_set=self))
    self.AddStory(EBayPinchZoom2018Page(
        page_set=self))
    self.AddStory(AccuWeatherPinchZoom2018Page(
        page_set=self))
    self.AddStory(YahooSportsPinchZoom2018Page(
        page_set=self))
    self.AddStory(BookingPinchZoom2018Page(
        page_set=self))


class AndroidToughPinchZoomCasesPageSet(ToughPinchZoomCasesPageSet):

  """
  ToughPinchZoomCasesPageSet using the maximum Android zoom level. This is
  chosen as 7x, which may seem to exceed the 5x value specified in
  WebPreferences::default_maximum_page_scale_factor. However, as desktop sites
  on Android start at less than 1x scale (up to 0.25x), a value of 7x does not
  exceed the 5x limit.
  """

  def __init__(self):
    super(AndroidToughPinchZoomCasesPageSet, self).__init__(7.0)


class DesktopToughPinchZoomCasesPageSet(ToughPinchZoomCasesPageSet):

  """ ToughPinchZoomCasesPageSet using the maximum desktop zoom level """

  def __init__(self):
    super(DesktopToughPinchZoomCasesPageSet, self).__init__(4.0)
