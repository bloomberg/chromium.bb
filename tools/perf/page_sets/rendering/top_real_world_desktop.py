# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.login_helpers import google_login
from page_sets.login_helpers import linkedin_login
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class TopRealWorldDesktopPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOP_REAL_WORLD_DESKTOP]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(TopRealWorldDesktopPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
      action_runner.Wait(1)
      with action_runner.CreateGestureInteraction('ScrollAction'):
        action_runner.ScrollPage()
        if self.story_set.scroll_forever:
          while True:
            action_runner.ScrollPage(direction='up')
            action_runner.ScrollPage(direction='down')


class GoogleWebSearchPage(TopRealWorldDesktopPage):
  """ Why: top google property; a google tab is often open """
  BASE_NAME = 'google_web_search'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleWebSearchPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GoogleWebSearchPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GoogleWebSearch2018Page(TopRealWorldDesktopPage):
  """ Why: top google property; a google tab is often open """
  BASE_NAME = 'google_web_search_2018'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleWebSearch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GoogleWebSearch2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GoogleImageSearchPage(TopRealWorldDesktopPage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearchPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleImageSearchPage, self).RunNavigateSteps(action_runner)


class GoogleImageSearch2018Page(TopRealWorldDesktopPage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search_2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GoogleImageSearch2018Page, self).RunNavigateSteps(action_runner)


class GmailPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  ABSTRACT_STORY = True
  URL = 'https://mail.google.com/mail/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GmailPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GmailPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendarPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  ABSTRACT_STORY = True
  URL='https://www.google.com/calendar/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleCalendarPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendarPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ExecuteJavaScript("""
        (function() {
          var elem = document.createElement('meta');
          elem.name='viewport';
          elem.content='initial-scale=1';
          document.body.appendChild(elem);
        })();""")
    action_runner.Wait(1)


class GoogleDocPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  ABSTRACT_STORY = True
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleDocPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleDocPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')


class GooglePlusPage(TopRealWorldDesktopPage):
  """ Why: social; top google property; Public profile; infinite scrolls """
  BASE_NAME = 'google_plus'
  URL = 'https://plus.google.com/110031535020051778989/posts'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GooglePlusPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GooglePlusPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Home')


class GooglePlus2018Page(TopRealWorldDesktopPage):
  """ Why: social; top google property; Public profile; infinite scrolls """
  BASE_NAME = 'google_plus_2018'
  URL = 'https://plus.google.com/110031535020051778989/posts'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GooglePlus2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GooglePlus2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Posts')


class YoutubePage(TopRealWorldDesktopPage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube'
  URL = 'http://www.youtube.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(YoutubePage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(YoutubePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Youtube2018Page(TopRealWorldDesktopPage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube_2018'
  URL = 'http://www.youtube.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Youtube2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Youtube2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='#buttons')


class BlogspotPage(TopRealWorldDesktopPage):
  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """
  BASE_NAME = 'blogspot'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(BlogspotPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(BlogspotPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='accessibility')


class Blogspot2018Page(TopRealWorldDesktopPage):
  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """
  BASE_NAME = 'blogspot_2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Blogspot2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Blogspot2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('div[class="searchBox"]')


class WordpressPage(TopRealWorldDesktopPage):
  """ Why: #18 (Alexa global), Picked an interesting post """
  BASE_NAME = 'wordpress'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WordpressPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(WordpressPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(
        # pylint: disable=line-too-long
        'a[href="http://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]'
    )


class Wordpress2018Page(TopRealWorldDesktopPage):
  """ Why: #18 (Alexa global), Picked an interesting post """
  BASE_NAME = 'wordpress_2018'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Wordpress2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Wordpress2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(
        # pylint: disable=line-too-long
        'a[href="https://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]'
    )


class FacebookPage(TopRealWorldDesktopPage):
  """ Why: top social,Public profile """
  BASE_NAME = 'facebook'
  URL = 'https://www.facebook.com/barackobama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(FacebookPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(FacebookPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class Facebook2018Page(TopRealWorldDesktopPage):
  """ Why: top social,Public profile """
  BASE_NAME = 'facebook_2018'
  URL = 'https://www.facebook.com/barackobama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Facebook2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Facebook2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class LinkedinPage(TopRealWorldDesktopPage):
  """ Why: #12 (Alexa global), Public profile. """
  BASE_NAME = 'linkedin'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(LinkedinPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class Linkedin2018Page(TopRealWorldDesktopPage):
  """ Why: #12 (Alexa global), Public profile. """
  BASE_NAME = 'linkedin_2018'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Linkedin2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginDesktopAccount(action_runner, 'linkedin')
    super(Linkedin2018Page, self).RunNavigateSteps(action_runner)


class WikipediaPage(TopRealWorldDesktopPage):
  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page. """
  BASE_NAME = 'wikipedia'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WikipediaPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class Wikipedia2018Page(TopRealWorldDesktopPage):
  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page. """
  BASE_NAME = 'wikipedia_2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Wikipedia2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class TwitterPage(TopRealWorldDesktopPage):
  """ Why: #8 (Alexa global),Picked an interesting page """
  BASE_NAME = 'twitter'
  URL = 'https://twitter.com/katyperry'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(TwitterPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(TwitterPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Twitter2018Page(TopRealWorldDesktopPage):
  """ Why: #8 (Alexa global),Picked an interesting page """
  BASE_NAME = 'twitter_2018'
  URL = 'https://twitter.com/katyperry'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Twitter2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Twitter2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.ProfileNav')


class PinterestPage(TopRealWorldDesktopPage):
  """ Why: #37 (Alexa global) """
  BASE_NAME = 'pinterest'
  URL = 'http://pinterest.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(PinterestPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class Pinterest2018Page(TopRealWorldDesktopPage):
  """ Why: #37 (Alexa global) """
  BASE_NAME = 'pinterest_2018'
  URL = 'https://www.pinterest.com/search/pins/?q=flowers&rs=typed'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Pinterest2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class ESPNPage(TopRealWorldDesktopPage):
  """ Why: #1 sports """
  ABSTRACT_STORY = True
  URL = 'http://espn.go.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ESPNPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class WeatherPage(TopRealWorldDesktopPage):
  """ Why: #7 (Alexa news); #27 total time spent, picked interesting page. """
  BASE_NAME = 'weather.com'
  URL = 'http://www.weather.com/weather/right-now/Mountain+View+CA+94043'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WeatherPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class AccuWeather2018Page(TopRealWorldDesktopPage):
  """ Why: #2 weather according to Alexa """
  BASE_NAME = 'accu_weather_2018'
  URL = 'https://www.accuweather.com/en/us/new-york-ny/10017/weather-forecast/349727'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(AccuWeather2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class YahooGamesPage(TopRealWorldDesktopPage):
  """ Why: #1 games according to Alexa (with actual games in it) """
  BASE_NAME = 'yahoo_games'
  URL = 'http://games.yahoo.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(YahooGamesPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(YahooGamesPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Twitch2018Page(TopRealWorldDesktopPage):
  """ Why: #1 games according to Alexa  """
  BASE_NAME = 'twitch_2018'
  URL = 'https://www.twitch.tv'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Twitch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#mantle_skin')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPageToElement(selector='.footer')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up')
          action_runner.ScrollPage(direction='down')


class GmailSmoothPage(GmailPage):
  """ Why: productivity, top google properties """
  BASE_NAME = 'gmail'

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


class Gmail2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  BASE_NAME = 'gmail_2018'
  URL = 'https://mail.google.com/mail/'

  def RunNavigateSteps(self, action_runner):
    google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(Gmail2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='.Tm.aeJ')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.Tm.aeJ')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.Tm.aeJ')
          action_runner.ScrollElement(
              direction='down', selector='.Tm.aeJ')


class GoogleCalendarSmoothPage(GoogleCalendarPage):
  """ Why: productivity, top google properties """
  BASE_NAME='google_calendar'

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


class GoogleCalendar2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  BASE_NAME='google_calendar_2018'
  URL='https://www.google.com/calendar/'

  def RunNavigateSteps(self, action_runner):
    google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendar2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('span[class~="sm8sCf"]')
    action_runner.ExecuteJavaScript("""
        (function() {
          var elem = document.createElement('meta');
          elem.name='viewport';
          elem.content='initial-scale=1';
          document.body.appendChild(elem);
        })();""")
    action_runner.Wait(1)


  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement('span[class~="sm8sCf"]')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#YPCqFe')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='#YPCqFe')
          action_runner.ScrollElement(
              direction='down', selector='#YPCqFe')


class GoogleDocSmoothPage(GoogleDocPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  BASE_NAME='google_docs'

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


class GoogleDoc2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view'
  BASE_NAME='google_docs_2018'

  def RunNavigateSteps(self, action_runner):
    super(GoogleDoc2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#printButton')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.kix-appview-editor')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.kix-appview-editor')
          action_runner.ScrollElement(
              direction='down', selector='.kix-appview-editor')


class ESPNSmoothPage(ESPNPage):
  """ Why: #1 sports """
  BASE_NAME='espn'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


class ESPN2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: #1 sports """
  BASE_NAME='espn_2018'
  URL = 'http://espn.go.com'

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#global-scoreboard')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


class YahooNewsPage(TopRealWorldDesktopPage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news'
  URL = 'http://news.yahoo.com'


class YahooNews2018Page(TopRealWorldDesktopPage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news_2018'
  URL = 'http://news.yahoo.com'


class CNNNewsPage(TopRealWorldDesktopPage):
  """Why: #2 news worldwide"""
  BASE_NAME = 'cnn'
  URL = 'http://www.cnn.com'


class CNNNews2018Page(TopRealWorldDesktopPage):
  """Why: #2 news worldwide"""
  BASE_NAME = 'cnn_2018'
  URL = 'http://www.cnn.com'


class AmazonPage(TopRealWorldDesktopPage):
  # Why: #1 world commerce website by visits; #3 commerce in the US by
  # time spent
  BASE_NAME = 'amazon'
  URL = 'http://www.amazon.com'


class Amazon2018Page(TopRealWorldDesktopPage):
  # Why: #1 world commerce website by visits; #3 commerce in the US by
  # time spent
  BASE_NAME = 'amazon_2018'
  URL = 'http://www.amazon.com'


class EbayPage(TopRealWorldDesktopPage):
  # Why: #1 commerce website by time spent by users in US
  BASE_NAME = 'ebay'
  URL = 'http://www.ebay.com'


class Ebay2018Page(TopRealWorldDesktopPage):
  # Why: #1 commerce website by time spent by users in US
  BASE_NAME = 'ebay_2018'
  URL = 'http://www.ebay.com'


class BookingPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa recreation
  BASE_NAME = 'booking.com'
  URL = 'http://booking.com'


class Booking2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa recreation
  BASE_NAME = 'booking.com_2018'
  URL = 'http://booking.com'


class YahooAnswersPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa reference
  BASE_NAME = 'yahoo_answers'
  URL = 'http://answers.yahoo.com'


class YahooAnswers2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa reference
  BASE_NAME = 'yahoo_answers_2018'
  URL = 'http://answers.yahoo.com'


class YahooSportsPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa sports
  BASE_NAME = 'yahoo_sports'
  URL = 'http://sports.yahoo.com/'


class YahooSports2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa sports
  BASE_NAME = 'yahoo_sports_2018'
  URL = 'http://sports.yahoo.com/'


class TechCrunchPage(TopRealWorldDesktopPage):
  # Why: top tech blog
  BASE_NAME = 'techcrunch'
  URL = 'http://techcrunch.com'


class TechCrunch2018Page(TopRealWorldDesktopPage):
  # Why: top tech blog
  BASE_NAME = 'techcrunch_2018'
  URL = 'http://techcrunch.com'
