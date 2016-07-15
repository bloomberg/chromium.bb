# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import system_health_story


class _BrowsingStory(system_health_story.SystemHealthStory):
  """Abstract base class for browsing stories.

  A browsing story visits items on the main page. Subclasses provide
  CSS selector to identify the items and implement interaction using
  the helper methods of this class.
  """

  IS_SINGLE_PAGE_APP = False
  ITEM_SELECTOR = NotImplemented
  ITEMS_TO_VISIT = 4
  ABSTRACT_STORY = True

  def _WaitForNavigation(self, action_runner):
    if not self.IS_SINGLE_PAGE_APP:
      action_runner.WaitForNavigate()

  def _NavigateToItem(self, action_runner, index):
    item_selector = 'document.querySelectorAll("%s")[%d]' % (
        self.ITEM_SELECTOR, index)
    self._ClickLink(action_runner, item_selector)

  def _ClickLink(self, action_runner, element_function):
    action_runner.WaitForElement(element_function=element_function)
    action_runner.ClickElement(element_function=element_function)
    self._WaitForNavigation(action_runner)

  def _NavigateBack(self, action_runner):
    action_runner.ExecuteJavaScript('window.history.back()')
    self._WaitForNavigation(action_runner)


class _NewsBrowsingStory(_BrowsingStory):
  """Abstract base class for news user stories.

  A news story imitates browsing a news website:
  1. Load the main page.
  2. Open and scroll the first news item.
  3. Go back to the main page and scroll it.
  4. Open and scroll the second news item.
  5. Go back to the main page and scroll it.
  6. etc.
  """

  ITEM_READ_TIME_IN_SECONDS = 3
  ITEM_SCROLL_REPEAT = 2
  MAIN_PAGE_SCROLL_REPEAT = 0
  ABSTRACT_STORY = True

  def _DidLoadDocument(self, action_runner):
    for i in xrange(self.ITEMS_TO_VISIT):
      self._NavigateToItem(action_runner, i)
      self._ReadNewsItem(action_runner)
      self._NavigateBack(action_runner)
      self._ScrollMainPage(action_runner)

  def _ReadNewsItem(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(self.ITEM_READ_TIME_IN_SECONDS)
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.ITEM_SCROLL_REPEAT)

  def _ScrollMainPage(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.MAIN_PAGE_SCROLL_REPEAT)


##############################################################################
# News browsing stories.
##############################################################################


class CnnStory(_NewsBrowsingStory):
  """The second top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:cnn'
  URL = 'http://edition.cnn.com/'
  ITEM_SELECTOR = '.cd__content > h3 > a'
  ITEMS_TO_VISIT = 2
  # TODO(ulan): Enable this story on mobile once it uses less memory and
  # does not crash with OOM.
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class FacebookMobileStory(_NewsBrowsingStory):
  NAME = 'browse:social:facebook'
  URL = 'https://www.facebook.com/rihanna'
  ITEM_SELECTOR = 'article ._5msj'
  # Facebook on desktop is not interesting because it embeds post comments
  # directly in the main timeline.
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class HackerNewsStory(_NewsBrowsingStory):
  NAME = 'browse:news:hackernews'
  URL = 'https://news.ycombinator.com'
  ITEM_SELECTOR = '.athing .title > a'


class NytimesMobileStory(_NewsBrowsingStory):
  """The third top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:nytimes'
  URL = 'http://mobile.nytimes.com'
  ITEM_SELECTOR = '.sfgAsset-link'
  # Visiting more items causes OOM.
  ITEMS_TO_VISIT = 2
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class NytimesDesktopStory(_NewsBrowsingStory):
  """The third top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:nytimes'
  URL = 'http://www.nytimes.com'
  ITEM_SELECTOR = '.story-heading > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class QqMobileStory(_NewsBrowsingStory):
  NAME = 'browse:news:qq'
  URL = 'http://news.qq.com'
  # Desktop qq.com opens a news item in a separate tab, for which the back
  # button does not work.
  # Mobile qq.com is disabled due to crbug.com/627166
  ITEM_SELECTOR = '.list .full a'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS


class RedditDesktopStory(_NewsBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  ITEM_SELECTOR = '.thing .title > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class RedditMobileStory(_NewsBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.PostHeader__post-title-line'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class TwitterMobileStory(_NewsBrowsingStory):
  NAME = 'browse:social:twitter'
  URL = 'https://www.twitter.com/justinbieber?skip_interstitial=true'
  ITEM_SELECTOR = '.Tweet-text'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class TwitterDesktopStory(_NewsBrowsingStory):
  NAME = 'browse:social:twitter'
  URL = 'https://www.twitter.com/justinbieber?skip_interstitial=true'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.tweet-text'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class WashingtonPostMobileStory(_NewsBrowsingStory):
  """Progressive website"""
  NAME = 'browse:news:washingtonpost'
  URL = 'https://www.washingtonpost.com/pwa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.hed > a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def _DidLoadDocument(self, action_runner):
    # Close the popup window.
    action_runner.ClickElement(selector='.close')
    super(WashingtonPostMobileStory, self)._DidLoadDocument(action_runner)
