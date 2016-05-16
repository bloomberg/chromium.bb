# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from page_sets.login_helpers import dropbox_login
from page_sets.login_helpers import google_login

from telemetry import story
from telemetry.page import page as page_module


def _LogIntoGoogleAccount(action_runner, credentials_path):
  google_login.LoginGoogleAccount(action_runner, 'googletest', credentials_path)


def _LogIntoGoogleAccountAndSetUpGmailSession(action_runner, credentials_path):
  _LogIntoGoogleAccount(action_runner, credentials_path)

  # Navigating to https://mail.google.com immediately leads to an infinite
  # redirection loop due to a bug in WPR (see
  # https://github.com/chromium/web-page-replay/issues/70). We therefore first
  # navigate to a sub-URL to set up the session and hit the resulting
  # redirection loop. Afterwards, we can safely navigate to
  # https://mail.google.com.
  action_runner.Navigate(
      'https://mail.google.com/mail/mu/mp/872/trigger_redirection_loop')
  action_runner.tab.WaitForDocumentReadyStateToBeComplete()


def _LogIntoDropboxAccount(action_runner, credentials_path):
  dropbox_login.LoginAccount(action_runner, 'dropbox', credentials_path)


def _CloseInboxInterstitialAndWaitUntilGmailReady(platform, action_runner):
  if platform == 'desktop':
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("loading").style.display === "none"')
  elif platform == 'mobile':
    # Close the "Get Inbox by Gmail" interstitial.
    action_runner.WaitForJavaScriptCondition(
        'document.querySelector("#isppromo a") !== null')
    action_runner.ExecuteJavaScript(
        'document.querySelector("#isppromo a").click()')
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("apploadingdiv").style.height === "0px"')


def _WaitUntilBubblesReady(_, action_runner):
  # The #logo element is removed right before the main menu is displayed.
  action_runner.WaitForJavaScriptCondition(
      'document.getElementById("logo") === null')


def _WaitUntilSpyChaseReady(_, action_runner):
  # The background of the game canvas is set when the "Tap screen to play"
  # caption is displayed.
  action_runner.WaitForJavaScriptCondition(
      'document.querySelector("#game canvas").style.background !== ""')


def _WaitUntilFlickrReady(_, action_runner):
  # Wait until the 'Recently tagged' view loads.
  action_runner.WaitForJavaScriptCondition('''
      document.querySelector(
          '.search-photos-everyone-trending-view .photo-list-view') !== null''')


class _PageSpec(object):

  def __init__(self, name, url, login_hook=None, post_load_hook=None):
    assert isinstance(url, (dict, str))
    self._name = name
    self._url = url
    self._login_hook = login_hook
    self._post_load_hook = post_load_hook

  @property
  def name(self):
    return self._name

  @property
  def url(self):
    return self._url

  @property
  def login_hook(self):
    return self._login_hook

  @property
  def post_load_hook(self):
    return self._post_load_hook


_SINGLE_PAGE_SPECS = {
    # Search and e-commerce.
    'search': [
        _PageSpec(
            name='google',
            url='https://www.google.com/#hl=en&q=science'),
        _PageSpec(
            name='baidu',
            url='https://www.baidu.com/s?word=google'),
        _PageSpec(
            name='yahoo',
            url='https://search.yahoo.com/search;_ylt=?p=google'),
        _PageSpec(
            name='amazon',
            url='https://www.amazon.com/s/?field-keywords=nexus'),
        # "ali_trackid" in the URL suppresses "Download app" interstitial.
        _PageSpec(
            name='taobao',
            url={'desktop': 'https://world.taobao.com/',
                 'mobile': 'http://m.intl.taobao.com/?ali_trackid'}),
        _PageSpec(
            name='yandex',
            url='https://yandex.ru/touchsearch?text=science'),
        # Redirects to the "http://" version.
        _PageSpec(
            name='ebay',
            url='https://www.ebay.com/sch/i.html?_nkw=headphones'),
        # Using "https://" forces mobile version on desktop. The mobile page
        # (https://www.flipkart.com/search?q=nexus+5x) almost always freezes
        # due to crbug.com/611390.
        _PageSpec(
            name='flipkart',
            url={'desktop': 'http://www.flipkart.com/search?q=nexus+5x'}),
    ],

    # Social networks.
    'social': [
        # Using Facebook login often causes "404 Not Found" with WPR.
        _PageSpec(
            name='facebook',
            url='https://www.facebook.com/rihanna'),
        _PageSpec(
            name='twitter',
            url='https://www.twitter.com/justinbieber?skip_interstitial=true'),
        # Due to the deterministic date injected by WPR (February 2008), the
        # cookie set by https://vk.com immediately expires, so the page keeps
        # refreshing indefinitely on mobile
        # (see https://github.com/chromium/web-page-replay/issues/71).
        _PageSpec(
            name='vk',
            url={'desktop': 'https://vk.com/sbeatles'}),
        _PageSpec(
            name='instagram',
            url='https://www.instagram.com/selenagomez/'),
        _PageSpec(
            name='pinterest',
            url='https://uk.pinterest.com/categories/popular/'),
        # Redirects to the "http://" version.
        _PageSpec(
            name='tumblr',
            url='https://50thousand.tumblr.com/'),
    ],

    # News, discussion and knowledge portals and blogs.
    'news': [
        # Redirects to the "http://" version.
        _PageSpec(
            name='bbc',
            url='https://www.bbc.co.uk/news/world-asia-china-36189636'),
        # Using "https://" shows "Your connection is not private".
        _PageSpec(
            name='cnn',
            url=(
                'http://edition.cnn.com/2016/05/02/health/three-habitable-planets-earth-dwarf-star/index.html')),
        _PageSpec(
            name='reddit',
            url={'desktop': (
                     'https://www.reddit.com/r/AskReddit/comments/4hi90e/whats_your_best_wedding_horror_story/'),
                 'mobile': (
                     'https://m.reddit.com/r/AskReddit/comments/4hi90e/whats_your_best_wedding_horror_story/')}),
        # Using "https://" hangs and shows "This site can't be reached".
        _PageSpec(
            name='qq',
            url='http://news.qq.com/a/20160503/003186.htm'),
        # Using "https://" leads to missing images and scripts on mobile (due
        # to mixed content). The desktop page
        # (http://news.sohu.com/20160503/n447433356.shtml) almost always fails
        # to completely load due to
        # https://github.com/chromium/web-page-replay/issues/74.
        _PageSpec(
            name='sohu',
            url={'mobile': 'http://m.sohu.com/n/447433356/'}),
        _PageSpec(
            name='wikipedia',
            url='https://en.wikipedia.org/wiki/Science'),
    ],

    # Audio and video.
    'media': [
        # No way to disable autoplay on desktop.
        _PageSpec(
            name='youtube',
            url='https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'),
        # The side panel with related videos doesn't show on desktop due to
        # https://github.com/chromium/web-page-replay/issues/74.
        _PageSpec(
            name='dailymotion',
            url=(
                'https://www.dailymotion.com/video/x489k7d_street-performer-shows-off-slinky-skills_fun?autoplay=false')),
        _PageSpec(
            name='google_images',
            url='https://www.google.co.uk/search?tbm=isch&q=love'),
        # No way to disable autoplay on desktop. Album artwork doesn't load due
        # to https://github.com/chromium/web-page-replay/issues/73.
        _PageSpec(
            name='soundcloud',
            url='https://soundcloud.com/lifeofdesiigner/desiigner-panda'),
        _PageSpec(
            name='9gag',
            url='https://www.9gag.com/'),
        _PageSpec(
            name='flickr',
            url='https://www.flickr.com/photos/tags/farm',
            post_load_hook=_WaitUntilFlickrReady),
    ],

    # Online tools (documents, emails, storage, ...).
    'tools': [
        _PageSpec(
            name='docs',
            url=(
                'https://docs.google.com/document/d/1GvzDP-tTLmJ0myRhUAfTYWs3ZUFilUICg8psNHyccwQ/edit?usp=sharing')),
        _PageSpec(
            name='gmail',
            url='https://mail.google.com/mail/',
            login_hook=_LogIntoGoogleAccountAndSetUpGmailSession,
            post_load_hook=_CloseInboxInterstitialAndWaitUntilGmailReady),
        _PageSpec(
            name='maps',
            url='https://www.google.com/maps/place/London,+UK/'),
        _PageSpec(
            name='stackoverflow',
            url=(
                'https://stackoverflow.com/questions/36827659/compiling-an-application-for-use-in-highly-radioactive-environments')),
        _PageSpec(
            name='dropbox',
            url='https://www.dropbox.com',
            login_hook=_LogIntoDropboxAccount),
        _PageSpec(
            name='weather',
            url='https://weather.com/en-GB/weather/today/l/USCA0286:1:US'),
        _PageSpec(
            name='drive',
            url='https://drive.google.com/drive/my-drive',
            login_hook=_LogIntoGoogleAccount),
    ],

    # In-browser games (HTML5 and Flash).
    'games': [
        _PageSpec(
            name='bubbles',
            url=(
                'https://games.cdn.famobi.com/html5games/s/smarty-bubbles/v010/?fg_domain=play.famobi.com&fg_uid=d8f24956-dc91-4902-9096-a46cb1353b6f&fg_pid=4638e320-4444-4514-81c4-d80a8c662371&fg_beat=620'),
            post_load_hook=_WaitUntilBubblesReady),
        # Using "https://" hangs and shows "This site can't be reached".
        _PageSpec(
            name='lazors',
            url='http://www8.games.mobi/games/html5/lazors/lazors.html'),
        # Using "https://" shows "Your connection is not private".
        _PageSpec(
            name='spychase',
            url='http://playstar.mobi/games/spychase/index.php',
            post_load_hook=_WaitUntilSpyChaseReady),
        # Desktop only (requires Flash). Using "https://" causes
        # "404 Not Found" during WPR recording.
        _PageSpec(
            name='miniclip',
            url={'desktop': 'http://www.miniclip.com/games/en/'}),
        # Desktop only (requires Flash).
        _PageSpec(
            name='alphabetty',
            url={'desktop': 'https://king.com/play/alphabetty'}),
    ],
}


DUMP_WAIT_TIME = 3


class _LoadCase(page_module.Page):
  """Generic System Health user story that loads a page and measures memory."""

  def __init__(self, story_set, group, url, page_spec):
    # For example, the name of the story for the |page_spec| with name
    # 'example' from the 'sample' URL |group| will be 'load:sample:example'.
    name = 'load:%s:%s' % (group, page_spec.name)
    super(_LoadCase, self).__init__(
        page_set=story_set, name=name, url=url,
        credentials_path='data/credentials.json',
        grouping_keys={'case': 'load', 'group': group})
    self._page_spec = page_spec

  @property
  def platform(self):
    return self.story_set.PLATFORM

  def _TakeMemoryMeasurement(self, action_runner):
    # TODO(petrcermak): This method is essentially the same as
    # MemoryHealthPage._TakeMemoryMeasurement() in memory_health_story.py.
    # Consider sharing the common code.
    action_runner.Wait(DUMP_WAIT_TIME)
    action_runner.ForceGarbageCollection()
    action_runner.Wait(DUMP_WAIT_TIME)
    tracing_controller = action_runner.tab.browser.platform.tracing_controller
    if not tracing_controller.is_tracing_running:
      return  # Tracing is not running, e.g., when recording a WPR archive.
    if not action_runner.tab.browser.DumpMemory():
      logging.error('Unable to get a memory dump for %s.', self.name)

  def RunNavigateSteps(self, action_runner):
    if self._page_spec.login_hook:
      self._page_spec.login_hook(action_runner, self.credentials_path)
    super(_LoadCase, self).RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    if self._page_spec.post_load_hook:
      self._page_spec.post_load_hook(self.platform, action_runner)
    self._TakeMemoryMeasurement(action_runner)


class _MemorySystemHealthStorySet(story.StorySet):
  """User stories for the System Health Plan.

  See https://goo.gl/Jek2NL.
  """
  PLATFORM = NotImplemented

  def __init__(self):
    super(_MemorySystemHealthStorySet, self).__init__(
        archive_data_file='data/memory_system_health_%s.json' % self.PLATFORM,
        cloud_storage_bucket=story.PARTNER_BUCKET)

    for group_name, page_specs in _SINGLE_PAGE_SPECS.iteritems():
      for page_spec in page_specs:
        url = page_spec.url
        if isinstance(url, dict):
          url = url.get(self.PLATFORM)
          if url is None:
            continue  # URL not supported on the platform.
        self.AddStory(_LoadCase(self, group_name, url, page_spec))


class DesktopMemorySystemHealthStorySet(_MemorySystemHealthStorySet):
  """Desktop user stories for Chrome Memory System Health Plan."""
  PLATFORM = 'desktop'


class MobileMemorySystemHealthStorySet(_MemorySystemHealthStorySet):
  """Mobile user stories for Chrome Memory System Health Plan."""
  PLATFORM = 'mobile'
