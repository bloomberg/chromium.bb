# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

from page_sets.login_helpers import dropbox_login
from page_sets.login_helpers import google_login

from telemetry.core import discover
from telemetry.page import page


_ALL_PLATFORMS = frozenset({'desktop', 'mobile'})
_DESKTOP_ONLY = frozenset({'desktop'})
_MOBILE_ONLY = frozenset({'mobile'})
_NO_PLATFORMS = frozenset()


_DUMP_WAIT_TIME = 3


class _SinglePageStory(page.Page):
  """Abstract base class for single-page System Health user stories."""

  # The full name of a single page story has the form CASE:GROUP:PAGE (e.g.
  # 'load:search:google').
  NAME = NotImplemented
  URL = NotImplemented
  SUPPORTED_PLATFORMS = _ALL_PLATFORMS

  def __init__(self, story_set, take_memory_measurement):
    case, group, _ = self.NAME.split(':')
    super(_SinglePageStory, self).__init__(
        page_set=story_set, name=self.NAME, url=self.URL,
        credentials_path='../data/credentials.json',
        grouping_keys={'case': case, 'group': group})
    self._take_memory_measurement = take_memory_measurement

  def _Measure(self, action_runner):
    if not self._take_memory_measurement:
      return
    # TODO(petrcermak): This method is essentially the same as
    # MemoryHealthPage._TakeMemoryMeasurement() in memory_health_story.py.
    # Consider sharing the common code.
    action_runner.Wait(_DUMP_WAIT_TIME)
    action_runner.ForceGarbageCollection()
    action_runner.Wait(_DUMP_WAIT_TIME)
    tracing_controller = action_runner.tab.browser.platform.tracing_controller
    if not tracing_controller.is_tracing_running:
      return  # Tracing is not running, e.g., when recording a WPR archive.
    if not action_runner.tab.browser.DumpMemory():
      logging.error('Unable to get a memory dump for %s.', self.name)

  def _Login(self, action_runner):
    pass

  def _DidLoadDocument(self, action_runner):
    pass

  def RunNavigateSteps(self, action_runner):
    self._Login(action_runner)
    super(_SinglePageStory, self).RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    self._DidLoadDocument(action_runner)
    self._Measure(action_runner)


def IterAllStoryClasses():
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for unused_cls_name, cls in sorted(discover.DiscoverClassesInModule(
      module=sys.modules[__name__],
      base_class=_SinglePageStory,
      index_by_class_name=True).iteritems()):
    yield cls


################################################################################
# Search and e-commerce.
################################################################################


class LoadGoogleStory(_SinglePageStory):
  NAME = 'load:search:google'
  URL = 'https://www.google.com/#hl=en&q=science'


class LoadBaiduStory(_SinglePageStory):
  NAME = 'load:search:baidu'
  URL = 'https://www.baidu.com/s?word=google'


class LoadYahooStory(_SinglePageStory):
  NAME = 'load:search:yahoo'
  URL = 'https://search.yahoo.com/search;_ylt=?p=google'


class LoadAmazonStory(_SinglePageStory):
  NAME = 'load:search:amazon'
  URL = 'https://www.amazon.com/s/?field-keywords=nexus'


class LoadTaobaoDesktopStory(_SinglePageStory):
  NAME = 'load:search:taobao'
  URL = 'https://world.taobao.com/'
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY


class LoadTaobaoMobileStory(_SinglePageStory):
  NAME = 'load:search:taobao'
  # "ali_trackid" in the URL suppresses "Download app" interstitial.
  URL = 'http://m.intl.taobao.com/?ali_trackid'
  SUPPORTED_PLATFORMS = _MOBILE_ONLY


class LoadYandexStory(_SinglePageStory):
  NAME = 'load:search:yandex'
  URL = 'https://yandex.ru/touchsearch?text=science'


class LoadEbayStory(_SinglePageStory):
  NAME = 'load:search:ebay'
  # Redirects to the "http://" version.
  URL = 'https://www.ebay.com/sch/i.html?_nkw=headphones'


################################################################################
# Social networks.
################################################################################


class LoadFacebookStory(_SinglePageStory):
  # Using Facebook login often causes "404 Not Found" with WPR.
  NAME = 'load:social:facebook'
  URL = 'https://www.facebook.com/rihanna'


class LoadTwitterStory(_SinglePageStory):
  NAME = 'load:social:twitter'
  URL = 'https://www.twitter.com/justinbieber?skip_interstitial=true'


class LoadVkStory(_SinglePageStory):
  NAME = 'load:social:vk'
  URL = 'https://vk.com/sbeatles'
  # Due to the deterministic date injected by WPR (February 2008), the cookie
  # set by https://vk.com immediately expires, so the page keeps refreshing
  # indefinitely on mobile
  # (see https://github.com/chromium/web-page-replay/issues/71).
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY


class LoadInstagramStory(_SinglePageStory):
  NAME = 'load:social:instagram'
  URL = 'https://www.instagram.com/selenagomez/'


class LoadPinterestStory(_SinglePageStory):
  NAME = 'load:social:pinterest'
  URL = 'https://uk.pinterest.com/categories/popular/'


class LoadTumblrStory(_SinglePageStory):
  NAME = 'load:social:tumblr'
  # Redirects to the "http://" version.
  URL = 'https://50thousand.tumblr.com/'


################################################################################
# News, discussion and knowledge portals and blogs.
################################################################################


class LoadBbcStory(_SinglePageStory):
  NAME = 'load:news:bbc'
  # Redirects to the "http://" version.
  URL = 'https://www.bbc.co.uk/news/world-asia-china-36189636'


class LoadCnnStory(_SinglePageStory):
  NAME = 'load:news:cnn'
  # Using "https://" shows "Your connection is not private".
  URL = (
      'http://edition.cnn.com/2016/05/02/health/three-habitable-planets-earth-dwarf-star/index.html')


class LoadRedditDesktopStory(_SinglePageStory):
  NAME = 'load:news:reddit'
  URL = (
      'https://www.reddit.com/r/AskReddit/comments/4hi90e/whats_your_best_wedding_horror_story/')
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY


class LoadRedditMobileStory(_SinglePageStory):
  NAME = 'load:news:reddit'
  URL = (
      'https://m.reddit.com/r/AskReddit/comments/4hi90e/whats_your_best_wedding_horror_story/')
  SUPPORTED_PLATFORMS = _MOBILE_ONLY


class LoadQqMobileStory(_SinglePageStory):
  NAME = 'load:news:qq'
  # Using "https://" hangs and shows "This site can't be reached".
  URL = 'http://news.qq.com/a/20160503/003186.htm'


class LoadSohuStory(_SinglePageStory):
  NAME = 'load:news:sohu'
  # Using "https://" leads to missing images and scripts on mobile (due to
  # mixed content).
  URL = 'http://m.sohu.com/n/447433356/'
  # The desktop page (http://news.sohu.com/20160503/n447433356.shtml) almost
  # always fails to completely load due to
  # https://github.com/chromium/web-page-replay/issues/74.
  SUPPORTED_PLATFORMS = _MOBILE_ONLY


class LoadWikipediaStory(_SinglePageStory):
  NAME = 'load:news:wikipedia'
  URL = 'https://en.wikipedia.org/wiki/Science'


################################################################################
# Audio and video.
################################################################################


class LoadYouTubeStory(_SinglePageStory):
  # No way to disable autoplay on desktop.
  NAME = 'load:media:youtube'
  URL = 'https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'


class LoadDailymotionStory(_SinglePageStory):
  # The side panel with related videos doesn't show on desktop due to
  # https://github.com/chromium/web-page-replay/issues/74.
  NAME = 'load:media:dailymotion'
  URL = (
      'https://www.dailymotion.com/video/x489k7d_street-performer-shows-off-slinky-skills_fun?autoplay=false')


class LoadGoogleImagesStory(_SinglePageStory):
  NAME = 'load:media:google_images'
  URL = 'https://www.google.co.uk/search?tbm=isch&q=love'


class LoadSoundCloudStory(_SinglePageStory):
  # No way to disable autoplay on desktop. Album artwork doesn't load due to
  # https://github.com/chromium/web-page-replay/issues/73.
  NAME = 'load:media:soundcloud'
  URL = 'https://soundcloud.com/lifeofdesiigner/desiigner-panda'


class Load9GagStory(_SinglePageStory):
  NAME = 'load:media:9gag'
  URL = 'https://www.9gag.com/'


class LoadFlickr(_SinglePageStory):
  NAME = 'load:media:flickr'
  URL = 'https://www.flickr.com/photos/tags/farm'

  def _DidLoadDocument(self, action_runner):
    # Wait until the 'Recently tagged' view loads.
    action_runner.WaitForJavaScriptCondition('''
        document.querySelector(
            '.search-photos-everyone-trending-view .photo-list-view')
                !== null''')


################################################################################
# Online tools (documents, emails, storage, ...).
################################################################################


class LoadDocsStory(_SinglePageStory):
  NAME = 'load:tools:docs'
  URL = (
      'https://docs.google.com/document/d/1GvzDP-tTLmJ0myRhUAfTYWs3ZUFilUICg8psNHyccwQ/edit?usp=sharing')


class _LoadGmailBaseStory(_SinglePageStory):
  NAME = 'load:tools:gmail'
  URL = 'https://mail.google.com/mail/'
  SUPPORTED_PLATFORMS = _NO_PLATFORMS

  def _Login(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest',
                                    self.credentials_path)

    # Navigating to https://mail.google.com immediately leads to an infinite
    # redirection loop due to a bug in WPR (see
    # https://github.com/chromium/web-page-replay/issues/70). We therefore first
    # navigate to a sub-URL to set up the session and hit the resulting
    # redirection loop. Afterwards, we can safely navigate to
    # https://mail.google.com.
    action_runner.Navigate(
        'https://mail.google.com/mail/mu/mp/872/trigger_redirection_loop')
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()


class LoadGmailDesktopStory(_LoadGmailBaseStory):
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY

  def _DidLoadDocument(self, action_runner):
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("loading").style.display === "none"')

class LoadGmailMobileStory(_LoadGmailBaseStory):
  SUPPORTED_PLATFORMS = _MOBILE_ONLY

  def _DidLoadDocument(self, action_runner):
    # Close the "Get Inbox by Gmail" interstitial.
    action_runner.WaitForJavaScriptCondition(
        'document.querySelector("#isppromo a") !== null')
    action_runner.ExecuteJavaScript(
        'document.querySelector("#isppromo a").click()')
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("apploadingdiv").style.height === "0px"')


class LoadMapsStory(_SinglePageStory):
  NAME = 'load:tools:maps'
  URL = 'https://www.google.com/maps/place/London,+UK/'


class LoadStackOverflowStory(_SinglePageStory):
  NAME = 'load:tools:stackoverflow'
  URL = (
      'https://stackoverflow.com/questions/36827659/compiling-an-application-for-use-in-highly-radioactive-environments')


class LoadDropboxStory(_SinglePageStory):
  NAME = 'load:tools:dropbox'
  URL = 'https://www.dropbox.com'

  def _Login(self, action_runner):
    dropbox_login.LoginAccount(action_runner, 'dropbox', self.credentials_path)


class LoadWeatherStory(_SinglePageStory):
  NAME = 'load:tools:weather'
  URL = 'https://weather.com/en-GB/weather/today/l/USCA0286:1:US'


class LoadDriveStory(_SinglePageStory):
  NAME = 'load:tools:drive'
  URL = 'https://drive.google.com/drive/my-drive'

  def _Login(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest',
                                    self.credentials_path)


################################################################################
# In-browser games (HTML5 and Flash).
################################################################################


class LoadBubblesStory(_SinglePageStory):
  NAME = 'load:games:bubbles'
  URL = (
      'https://games.cdn.famobi.com/html5games/s/smarty-bubbles/v010/?fg_domain=play.famobi.com&fg_uid=d8f24956-dc91-4902-9096-a46cb1353b6f&fg_pid=4638e320-4444-4514-81c4-d80a8c662371&fg_beat=620')

  def _DidLoadDocument(self, action_runner):
    # The #logo element is removed right before the main menu is displayed.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("logo") === null')


class LoadLazorsStory(_SinglePageStory):
  NAME = 'load:games:lazors'
  # Using "https://" hangs and shows "This site can't be reached".
  URL = 'http://www8.games.mobi/games/html5/lazors/lazors.html'


class LoadSpyChaseStory(_SinglePageStory):
  NAME = 'load:games:spychase'
  # Using "https://" shows "Your connection is not private".
  URL = 'http://playstar.mobi/games/spychase/index.php'

  def _DidLoadDocument(self, action_runner):
    # The background of the game canvas is set when the "Tap screen to play"
    # caption is displayed.
    action_runner.WaitForJavaScriptCondition(
        'document.querySelector("#game canvas").style.background !== ""')


class LoadMiniclipStory(_SinglePageStory):
  NAME = 'load:games:miniclip'
  # Using "https://" causes "404 Not Found" during WPR recording.
  URL = 'http://www.miniclip.com/games/en/'
  # Desktop only (requires Flash).
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY


class LoadAlphabettyStory(_SinglePageStory):
  NAME = 'load:games:alphabetty'
  URL = 'https://king.com/play/alphabetty'
  # Desktop only (requires Flash).
  SUPPORTED_PLATFORMS = _DESKTOP_ONLY
