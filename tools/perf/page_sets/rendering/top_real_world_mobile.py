# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.login_helpers import linkedin_login
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class TopRealWorldMobilePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.SYNC_SCROLL, story_tags.TOP_REAL_WORLD_MOBILE]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TopRealWorldMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class CapitolVolkswagenMobilePage(TopRealWorldMobilePage):
  """ Why: Typical mobile business site """
  BASE_NAME = 'capitolvolkswagen_mobile'
  URL = ('http://iphone.capitolvolkswagen.com/index.htm'
         '#new-inventory_p_2Fsb-new_p_2Ehtm_p_3Freset_p_3DInventoryListing')

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CapitolVolkswagenMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CapitolVolkswagenMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next 35')
    action_runner.WaitForJavaScriptCondition(
        'document.body.scrollHeight > 2560')


class CapitolVolkswagenMobile2018Page(TopRealWorldMobilePage):
  """ Why: Typical mobile business site """
  BASE_NAME = 'capitolvolkswagen_mobile'
  YEAR = '2018'
  URL = 'https://www.capitolvolkswagen.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CapitolVolkswagenMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class TheVergeArticleMobilePage(TopRealWorldMobilePage):
  """ Why: Top tech blog """
  BASE_NAME = 'theverge_article_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.theverge.com/2012/10/28/3568746/amazon-7-inch-fire-hd-ipad-mini-ad-ballsy'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TheVergeArticleMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(TheVergeArticleMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.Chorus !== undefined &&'
        'window.Chorus.Comments !== undefined &&'
        'window.Chorus.Comments.Json !== undefined &&'
        '(window.Chorus.Comments.loaded ||'
        ' window.Chorus.Comments.Json.load_comments())')


class TheVergeArticleMobile2018Page(TopRealWorldMobilePage):
  """ Why: Top tech blog """
  BASE_NAME = 'theverge_article_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.theverge.com/2018/7/18/17582836/chrome-os-tablet-acer-chromebook-tab-10-android-ipad'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TheVergeArticleMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class CnnArticleMobilePage(TopRealWorldMobilePage):
  """ Why: Top news site """
  BASE_NAME = 'cnn_article_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.cnn.com/2012/10/03/politics/michelle-obama-debate/index.html'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CnnArticleMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CnnArticleMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(8)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # With default top_start_ratio=0.5 the corresponding element in this page
      # will not be in the root scroller.
      action_runner.ScrollPage(top_start_ratio=0.01)


class CnnArticleMobile2018Page(TopRealWorldMobilePage):
  """ Why: Top news site """
  BASE_NAME = 'cnn_article_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.cnn.com/travel/article/airbus-a330-900-neo-tours-us-airports/index.html'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CnnArticleMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CnnArticleMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.Article__entitlement')

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # With default top_start_ratio=0.5 the corresponding element in this page
      # will not be in the root scroller.
      action_runner.ScrollPage(top_start_ratio=0.01)


class FacebookMobilePage(TopRealWorldMobilePage):
  """ Why: #1 (Alexa global) """
  BASE_NAME = 'facebook_mobile'
  URL = 'https://facebook.com/barackobama'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FacebookMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(FacebookMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class FacebookMobile2018Page(TopRealWorldMobilePage):
  """ Why: #1 (Alexa global) """
  BASE_NAME = 'facebook_mobile'
  YEAR = '2018'
  URL = 'https://facebook.com/barackobama'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FacebookMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(FacebookMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class YoutubeMobilePage(TopRealWorldMobilePage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube_mobile'
  URL = 'http://m.youtube.com/watch?v=9hBpF_Zj4OA'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YoutubeMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YoutubeMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("paginatortarget") !== null')


class YoutubeMobile2018Page(TopRealWorldMobilePage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube_mobile'
  YEAR = '2018'
  URL = 'http://m.youtube.com/watch?v=9hBpF_Zj4OA'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YoutubeMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YoutubeMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("player") !== null')


class LinkedInMobilePage(TopRealWorldMobilePage):
  """ Why: #12 (Alexa global),Public profile """
  BASE_NAME = 'linkedin_mobile'
  URL = 'https://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(LinkedInMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(LinkedInMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')

    action_runner.ScrollPage()

    super(LinkedInMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')


class LinkedInMobile2018Page(TopRealWorldMobilePage):
  """ Why: #12 (Alexa global),Public profile """
  BASE_NAME = 'linkedin_mobile'
  YEAR = '2018'
  URL = 'https://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(LinkedInMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginMobileAccount(action_runner, 'linkedin')
    super(LinkedInMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-wrapper") !== null')

    action_runner.ScrollPage()

    super(LinkedInMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-wrapper") !== null')


class YahooAnswersMobilePage(TopRealWorldMobilePage):
  """ Why: #1 Alexa reference """
  BASE_NAME = 'yahoo_answers_mobile'
  # pylint: disable=line-too-long
  URL = 'http://answers.yahoo.com/question/index?qid=20110117024343AAopj8f'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YahooAnswersMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YahooAnswersMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Other Answers (1 - 20 of 149)')
    action_runner.ClickElement(text='Other Answers (1 - 20 of 149)')


class YahooAnswersMobile2018Page(TopRealWorldMobilePage):
  """ Why: #1 Alexa reference """
  BASE_NAME = 'yahoo_answers_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://ca.answers.yahoo.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YahooAnswersMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
   super(YahooAnswersMobile2018Page, self).RunNavigateSteps(action_runner)
   action_runner.ScrollElement(selector='#page_scrollable')


class GoogleNewsMobilePage(TopRealWorldMobilePage):
  """ Why: Google News: accelerated scrolling version """
  BASE_NAME = 'google_news_mobile'
  URL = 'http://mobile-news.sandbox.google.com/news/pt1'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleNewsMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(GoogleNewsMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'typeof NEWS_telemetryReady !== "undefined" && '
        'NEWS_telemetryReady == true')


class GoogleNewsMobile2018Page(TopRealWorldMobilePage):
  """ Why: Google News: accelerated scrolling version """
  BASE_NAME = 'google_news_mobile'
  YEAR = '2018'
  URL = 'https://news.google.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleNewsMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class GoogleImageSearchMobile2018Page(TopRealWorldMobilePage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search_mobile'
  YEAR = '2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearchMobile2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class AmazonNicolasCageMobilePage(TopRealWorldMobilePage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """
  BASE_NAME = 'amazon_mobile'
  URL = 'http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(AmazonNicolasCageMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#search',
          distance_expr='document.body.scrollHeight - window.innerHeight')


class AmazonNicolasCageMobile2018Page(TopRealWorldMobilePage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """
  BASE_NAME = 'amazon_mobile'
  YEAR = '2018'
  URL = 'http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(AmazonNicolasCageMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class WowwikiMobilePage(TopRealWorldMobilePage):
  """Why: Mobile wiki."""
  BASE_NAME = 'wowwiki_mobile'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WowwikiMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Wowwiki has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(WowwikiMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    super(WowwikiMobilePage, self).RunNavigateSteps(action_runner)


class WowwikiMobile2018Page(TopRealWorldMobilePage):
  """Why: Mobile wiki."""
  BASE_NAME = 'wowwiki_mobile'
  YEAR = '2018'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WowwikiMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Wowwiki has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(WowwikiMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    super(WowwikiMobile2018Page, self).RunNavigateSteps(action_runner)


class WikipediaDelayedScrollMobilePage(TopRealWorldMobilePage):
  """Why: Wikipedia page with a delayed scroll start"""
  BASE_NAME = 'wikipedia_delayed_scroll_start'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WikipediaDelayedScrollMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState == "complete"', timeout=30)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class WikipediaDelayedScrollMobile2018Page(TopRealWorldMobilePage):
  """Why: Wikipedia page with a delayed scroll start"""
  BASE_NAME = 'wikipedia_delayed_scroll_start'
  YEAR = '2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WikipediaDelayedScrollMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState == "complete"', timeout=30)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class BlogspotMobilePage(TopRealWorldMobilePage):
  """Why: #11 (Alexa global), google property"""
  BASE_NAME = 'blogspot_mobile'
  URL = 'http://googlewebmastercentral.blogspot.com/'


class BlogspotMobile2018Page(TopRealWorldMobilePage):
  """Why: #11 (Alexa global), google property"""
  BASE_NAME = 'blogspot_mobile'
  YEAR = '2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'


class WordpressMobilePage(TopRealWorldMobilePage):
  """Why: #18 (Alexa global), Picked an interesting post"""
  BASE_NAME = 'wordpress_mobile'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'


class WordpressMobile2018Page(TopRealWorldMobilePage):
  """Why: #18 (Alexa global), Picked an interesting post"""
  BASE_NAME = 'wordpress_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'


class WikipediaMobilePage(TopRealWorldMobilePage):
  """Why: #6 (Alexa) most visited worldwide, picked an interesting page"""
  BASE_NAME = 'wikipedia_mobile'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'


class WikipediaMobile2018Page(TopRealWorldMobilePage):
  """Why: #6 (Alexa) most visited worldwide, picked an interesting page"""
  BASE_NAME = 'wikipedia_mobile'
  YEAR = '2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'


class TwitterMobilePage(TopRealWorldMobilePage):
  """Why: #8 (Alexa global), picked an interesting page"""
  BASE_NAME = 'twitter_mobile'
  URL = 'http://twitter.com/katyperry'


class TwitterMobile2018Page(TopRealWorldMobilePage):
  """Why: #8 (Alexa global), picked an interesting page"""
  BASE_NAME = 'twitter_mobile'
  YEAR = '2018'
  URL = 'http://twitter.com/katyperry'


class PinterestMobilePage(TopRealWorldMobilePage):
  """Why: #37 (Alexa global)."""
  BASE_NAME = 'pinterest_mobile'
  URL = 'http://pinterest.com'


class PinterestMobile2018Page(TopRealWorldMobilePage):
  """Why: #37 (Alexa global)."""
  BASE_NAME = 'pinterest_mobile'
  YEAR = '2018'
  URL = 'https://www.pinterest.com/search/pins/?q=flowers&rs=typed'


class ESPNMobilePage(TopRealWorldMobilePage):
  """Why: #1 sports."""
  BASE_NAME = 'espn_mobile'
  URL = 'http://espn.go.com'


class ESPNMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 sports."""
  BASE_NAME = 'espn_mobile'
  YEAR = '2018'
  URL = 'http://www.espn.com/'


class ForecastIOMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/231413"""
  BASE_NAME = 'forecast.io_mobile'
  URL = 'http://forecast.io'


class ForecastIOMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/231413"""
  BASE_NAME = 'forecast.io_mobile'
  YEAR = '2018'
  URL = 'http://forecast.io'


class GooglePlusMobilePage(TopRealWorldMobilePage):
  """Why: Social; top Google property; Public profile; infinite scrolls."""
  BASE_NAME = 'google_plus_mobile'
  # pylint: disable=line-too-long
  URL = 'https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo'


class GooglePlusMobile2018Page(TopRealWorldMobilePage):
  """Why: Social; top Google property; Public profile; infinite scrolls."""
  BASE_NAME = 'google_plus_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo'


class AndroidPoliceMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/242544"""
  BASE_NAME = 'androidpolice_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-program/'


class AndroidPoliceMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/242544"""
  BASE_NAME = 'androidpolice_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-program/'


class GSPMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/149958"""
  BASE_NAME = 'gsp.ro_mobile'
  URL = 'http://gsp.ro'


class GSPMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/149958"""
  BASE_NAME = 'gsp.ro_mobile'
  YEAR = '2018'
  URL = 'http://gsp.ro'


class TheVergeMobilePage(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'theverge_mobile'
  URL = 'http://theverge.com'


class TheVergeMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'theverge_mobile'
  YEAR = '2018'
  URL = 'http://theverge.com'


class DiggMobilePage(TopRealWorldMobilePage):
  """Why: Top tech site"""
  BASE_NAME = 'digg_mobile'
  URL = 'http://digg.com'


class DiggMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech site"""
  BASE_NAME = 'digg_mobile'
  YEAR = '2018'
  URL = 'http://digg.com/channel/digg-feature'


class GoogleSearchMobilePage(TopRealWorldMobilePage):
  """Why: Top Google property; a Google tab is often open"""
  BASE_NAME = 'google_web_search_mobile'
  URL = 'https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h'


class GoogleSearchMobile2018Page(TopRealWorldMobilePage):
  """Why: Top Google property; a Google tab is often open"""
  BASE_NAME = 'google_web_search_mobile'
  YEAR = '2018'
  URL = 'https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h'


class YahooNewsMobilePage(TopRealWorldMobilePage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news_mobile'
  URL = 'http://news.yahoo.com'


class YahooNewsMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news_mobile'
  YEAR = '2018'
  URL = 'http://news.yahoo.com'


class CnnNewsMobilePage(TopRealWorldMobilePage):
  """# Why: #2 news worldwide"""
  BASE_NAME = 'cnn_mobile'
  URL = 'http://www.cnn.com'


class CnnNewsMobile2018Page(TopRealWorldMobilePage):
  """# Why: #2 news worldwide"""
  BASE_NAME = 'cnn_mobile'
  YEAR = '2018'
  URL = 'http://www.cnn.com'


class EbayMobilePage(TopRealWorldMobilePage):
  """Why: #1 commerce website by time spent by users in US"""
  BASE_NAME = 'ebay_mobile'
  URL = 'http://shop.mobileweb.ebay.com/searchresults?kw=viking+helmet'


class EbayMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 commerce website by time spent by users in US"""
  BASE_NAME = 'ebay_mobile'
  YEAR = '2018'
  URL = 'https://m.ebay.com/'


class BookingMobilePage(TopRealWorldMobilePage):
  """Why: #1 Alexa recreation"""
  BASE_NAME = 'booking.com_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.booking.com/searchresults.html?src=searchresults&latitude=65.0500&longitude=25.4667'


class BookingMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 Alexa recreation"""
  BASE_NAME = 'booking.com_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.booking.com'


class TechCrunchMobilePage(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'techcrunch_mobile'
  URL = 'http://techcrunch.com'


class TechCrunchMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'techcrunch_mobile'
  YEAR = '2018'
  URL = 'http://techcrunch.com'


class MLBMobilePage(TopRealWorldMobilePage):
  """Why: #6 Alexa sports"""
  BASE_NAME = 'mlb_mobile'
  URL = 'http://mlb.com/'


class MLBMobile2018Page(TopRealWorldMobilePage):
  """Why: #6 Alexa sports"""
  BASE_NAME = 'mlb_mobile'
  YEAR = '2018'
  URL = 'http://mlb.com/'


class SFGateMobilePage(TopRealWorldMobilePage):
  """Why: #14 Alexa California"""
  BASE_NAME = 'sfgate_mobile'
  URL = 'http://www.sfgate.com/'


class SFGateMobile2018Page(TopRealWorldMobilePage):
  """Why: #14 Alexa California"""
  BASE_NAME = 'sfgate_mobile'
  YEAR = '2018'
  URL = 'http://www.sfgate.com/'


class WorldJournalMobilePage(TopRealWorldMobilePage):
  """Why: Non-latin character set"""
  BASE_NAME = 'worldjournal_mobile'
  URL = 'http://worldjournal.com/'


class WorldJournalMobile2018Page(TopRealWorldMobilePage):
  """Why: Non-latin character set"""
  BASE_NAME = 'worldjournal_mobile'
  YEAR = '2018'
  URL = 'http://worldjournal.com/'


class WSJMobilePage(TopRealWorldMobilePage):
  """Why: #15 Alexa news"""
  BASE_NAME = 'wsj_mobile'
  URL = 'http://online.wsj.com/home-page'


class WSJMobile2018Page(TopRealWorldMobilePage):
  """Why: #15 Alexa news"""
  BASE_NAME = 'wsj_mobile'
  YEAR = '2018'
  URL = 'http://online.wsj.com/home-page'


class DeviantArtMobilePage(TopRealWorldMobilePage):
  """Why: Image-heavy mobile site"""
  BASE_NAME = 'deviantart_mobile'
  URL = 'http://www.deviantart.com/'


class DeviantArtMobile2018Page(TopRealWorldMobilePage):
  """Why: Image-heavy mobile site"""
  BASE_NAME = 'deviantart_mobile'
  YEAR = '2018'
  URL = 'http://www.deviantart.com/'


class BaiduMobilePage(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'baidu_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920'


class BaiduMobile2018Page(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'baidu_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920'


class BingMobilePage(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'bing_mobile'
  URL = 'http://www.bing.com/search?q=sloths'


class BingMobile2018Page(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'bing_mobile'
  YEAR = '2018'
  URL = 'http://www.bing.com/search?q=sloths'


class USATodayMobilePage(TopRealWorldMobilePage):
  """Why: Good example of poor initial scrolling"""
  BASE_NAME = 'usatoday_mobile'
  URL = 'http://ftw.usatoday.com/2014/05/spelling-bee-rules-shenanigans'


class USATodayMobile2018Page(TopRealWorldMobilePage):
  """Why: Good example of poor initial scrolling"""
  BASE_NAME = 'usatoday_mobile'
  YEAR = '2018'
  URL = 'http://ftw.usatoday.com/'


class FastPathSmoothMobilePage(TopRealWorldMobilePage):
  ABSTRACT_STORY = True
  TAGS = [story_tags.FASTPATH, story_tags.TOP_REAL_WORLD_MOBILE]

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FastPathSmoothMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class NYTimesMobilePage(FastPathSmoothMobilePage):
  """Why: Top news site."""
  BASE_NAME = 'nytimes_mobile'
  URL = 'http://nytimes.com/'


class NYTimesMobile2018Page(FastPathSmoothMobilePage):
  """Why: Top news site."""
  BASE_NAME = 'nytimes_mobile'
  YEAR = '2018'
  URL = 'http://nytimes.com/'


class CuteOverloadMobilePage(FastPathSmoothMobilePage):
  """Why: Image-heavy site."""
  BASE_NAME = 'cuteoverload_mobile'
  URL = 'http://cuteoverload.com'


class RedditMobilePage(FastPathSmoothMobilePage):
  """Why: #5 Alexa news."""
  BASE_NAME = 'reddit_mobile'
  URL = 'http://www.reddit.com/r/programming/comments/1g96ve'


class RedditMobile2018Page(FastPathSmoothMobilePage):
  """Why: #5 Alexa news."""
  BASE_NAME = 'reddit_mobile'
  YEAR = '2018'
  URL = 'http://www.reddit.com/r/programming/comments/1g96ve'


class BoingBoingMobilePage(FastPathSmoothMobilePage):
  """Why: Problematic use of fixed position elements."""
  BASE_NAME = 'boingboing_mobile'
  URL = 'http://www.boingboing.net'


class BoingBoingMobile2018Page(FastPathSmoothMobilePage):
  """Why: Problematic use of fixed position elements."""
  BASE_NAME = 'boingboing_mobile'
  YEAR = '2018'
  URL = 'http://www.boingboing.net'


class SlashDotMobilePage(FastPathSmoothMobilePage):
  """Why: crbug.com/169827"""
  BASE_NAME = 'slashdot_mobile'
  URL = 'http://slashdot.org'


class SlashDotMobile2018Page(FastPathSmoothMobilePage):
  """Why: crbug.com/169827"""
  BASE_NAME = 'slashdot_mobile'
  YEAR = '2018'
  URL = 'http://slashdot.org'
