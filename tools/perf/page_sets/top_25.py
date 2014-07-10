# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


def _GetCurrentLocation(action_runner):
  return action_runner.EvaluateJavaScript('document.location.href')


def _WaitForLocationChange(action_runner, old_href):
  action_runner.WaitForJavaScriptCondition(
      'document.location.href != "%s"' % old_href)


class Top25Page(page_module.Page):

  def __init__(self, url, page_set, name=''):
    super(Top25Page, self).__init__(url=url, page_set=page_set, name=name)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/top_25.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()

  def RunRepaint(self, action_runner):
    action_runner.RepaintContinuously(seconds=5)


class GoogleWebSearchPage(Top25Page):

  """ Why: top google property; a google tab is often open """

  def __init__(self, page_set):
    super(GoogleWebSearchPage, self).__init__(
      url='https://www.google.com/#hl=en&q=barack+obama',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='Next')

  def RunStressMemory(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Next')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Next')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Next')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Next')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Next')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Previous')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Previous')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Previous')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Previous')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Previous')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Previous')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Images')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(text='Images')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.WaitForElement(text='Images')


class GmailPage(Top25Page):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(GmailPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')

  def RunStressMemory(self, action_runner):
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        'a[href="https://mail.google.com/mail/u/0/?shva=1#starred"]')
    _WaitForLocationChange(action_runner, old_href)
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        'a[href="https://mail.google.com/mail/u/0/?shva=1#inbox"]')
    _WaitForLocationChange(action_runner, old_href)

  def RunSmoothness(self, action_runner):
    action_runner.ExecuteJavaScript('''
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });''')
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        element_function='window.__scrollableElementForTelemetry')
    interaction.End()


class GoogleCalendarPage(Top25Page):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(GoogleCalendarPage, self).__init__(
      url='https://www.google.com/calendar/',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ExecuteJavaScript('''
        (function() {
          var elem = document.createElement('meta');
          elem.name='viewport';
          elem.content='initial-scale=1';
          document.body.appendChild(elem);
        })();''')
    action_runner.Wait(1)

  def RunStressMemory(self, action_runner):
    action_runner.ClickElement('div[class~="navForward"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ClickElement('div[class~="navForward"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ClickElement('div[class~="navForward"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ClickElement('div[class~="navForward"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navBack"]')
    action_runner.ClickElement('div[class~="navBack"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navBack"]')
    action_runner.ClickElement('div[class~="navBack"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navBack"]')
    action_runner.ClickElement('div[class~="navBack"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navBack"]')
    action_runner.ClickElement('div[class~="navBack"]')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navBack"]')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(selector='#scrolltimedeventswk')
    interaction.End()


class GoogleImageSearchPage(Top25Page):

  """ Why: tough image case; top google properties """

  def __init__(self, page_set):
    super(GoogleImageSearchPage, self).__init__(
      url='https://www.google.com/search?q=cats&tbm=isch',
      page_set=page_set)

    self.credentials = 'google'


class GoogleDocPage(Top25Page):

  """ Why: productivity, top google properties; Sample doc in the link """

  def __init__(self, page_set):
    super(GoogleDocPage, self).__init__(
      # pylint: disable=C0301
      url='https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view',
      page_set=page_set,
      name='Docs  (1 open document tab)')

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(selector='.kix-appview-editor')
    interaction.End()


class GooglePlusPage(Top25Page):

  """ Why: social; top google property; Public profile; infinite scrolls """

  def __init__(self, page_set):
    super(GooglePlusPage, self).__init__(
      url='https://plus.google.com/110031535020051778989/posts',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='Home')

  def RunStressMemory(self, action_runner):
    action_runner.ClickElement(text='Home')
    action_runner.Wait(2)
    action_runner.WaitForElement(text='Profile')
    action_runner.ClickElement(text='Profile')
    action_runner.Wait(2)
    action_runner.WaitForElement(text='Explore')
    action_runner.ClickElement(text='Explore')
    action_runner.Wait(2)
    action_runner.WaitForElement(text='Events')
    action_runner.ClickElement(text='Events')
    action_runner.Wait(2)
    action_runner.WaitForElement(text='Communities')
    action_runner.ClickElement(text='Communities')
    action_runner.Wait(2)
    action_runner.WaitForElement(text='Home')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class YoutubePage(Top25Page):

  """ Why: #3 (Alexa global) """

  def __init__(self, page_set):
    super(YoutubePage, self).__init__(
      url='http://www.youtube.com',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)


class BlogspotPage(Top25Page):

  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """

  def __init__(self, page_set):
    super(BlogspotPage, self).__init__(
      url='http://googlewebmastercentral.blogspot.com/',
      page_set=page_set,
      name='Blogger')

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='accessibility')

  def RunStressMemory(self, action_runner):
    action_runner.ClickElement(text='accessibility')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(text='advanced')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(text='beginner')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(text='Home')
    action_runner.WaitForNavigate()


class WordpressPage(Top25Page):

  """ Why: #18 (Alexa global), Picked an interesting post """

  def __init__(self, page_set):
    super(WordpressPage, self).__init__(
      # pylint: disable=C0301
      url='http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/',
      page_set=page_set,
      name='Wordpress')

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(
        # pylint: disable=C0301
        'a[href="http://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]')

  def RunStressMemory(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(
        # pylint: disable=C0301
        'a[href="http://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(text='Features')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()
    action_runner.ClickElement(text='News')
    action_runner.WaitForNavigate()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class FacebookPage(Top25Page):

  """ Why: top social,Public profile """

  def __init__(self, page_set):
    super(FacebookPage, self).__init__(
      url='http://www.facebook.com/barackobama',
      page_set=page_set,
      name='Facebook')
    self.credentials = 'facebook'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='About')

  def RunStressMemory(self, action_runner):
    action_runner.ClickElement(text='About')
    action_runner.WaitForNavigate()
    action_runner.ClickElement(text='The Audacity of Hope')
    action_runner.WaitForNavigate()
    action_runner.ClickElement(text='Back to Barack Obama\'s Timeline')
    action_runner.WaitForNavigate()
    action_runner.ClickElement(text='About')
    action_runner.WaitForNavigate()
    action_runner.ClickElement(text='Elected to U.S. Senate')
    action_runner.WaitForNavigate()
    action_runner.ClickElement(text='Home')
    action_runner.WaitForNavigate()

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class LinkedinPage(Top25Page):

  """ Why: #12 (Alexa global),Public profile """

  def __init__(self, page_set):
    super(LinkedinPage, self).__init__(
      url='http://www.linkedin.com/in/linustorvalds',
      page_set=page_set,
      name='LinkedIn')


class WikipediaPage(Top25Page):

  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page """

  def __init__(self, page_set):
    super(WikipediaPage, self).__init__(
      url='http://en.wikipedia.org/wiki/Wikipedia',
      page_set=page_set,
      name='Wikipedia (1 tab)')


class TwitterPage(Top25Page):

  """ Why: #8 (Alexa global),Picked an interesting page """

  def __init__(self, page_set):
    super(TwitterPage, self).__init__(
      url='https://twitter.com/katyperry',
      page_set=page_set,
      name='Twitter')

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class PinterestPage(Top25Page):

  """ Why: #37 (Alexa global) """

  def __init__(self, page_set):
    super(PinterestPage, self).__init__(
      url='http://pinterest.com',
      page_set=page_set,
      name='Pinterest')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class ESPNPage(Top25Page):

  """ Why: #1 sports """

  def __init__(self, page_set):
    super(ESPNPage, self).__init__(
      url='http://espn.go.com',
      page_set=page_set,
      name='ESPN')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(left_start_ratio=0.1)
    interaction.End()


class WeatherDotComPage(Top25Page):

  """ Why: #7 (Alexa news); #27 total time spent,Picked interesting page """

  def __init__(self, page_set):
    super(WeatherDotComPage, self).__init__(
      # pylint: disable=C0301
      url='http://www.weather.com/weather/right-now/Mountain+View+CA+94043',
      page_set=page_set,
      name='Weather.com')


class YahooGamesPage(Top25Page):

  """ Why: #1 games according to Alexa (with actual games in it) """

  def __init__(self, page_set):
    super(YahooGamesPage, self).__init__(
      url='http://games.yahoo.com',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)


class Top25PageSet(page_set_module.PageSet):

  """ Pages hand-picked for 2012 CrOS scrolling tuning efforts. """

  def __init__(self):
    super(Top25PageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/top_25.json',
      bucket=page_set_module.PARTNER_BUCKET)

    self.AddPage(GoogleWebSearchPage(self))
    self.AddPage(GmailPage(self))
    self.AddPage(GoogleCalendarPage(self))
    self.AddPage(GoogleImageSearchPage(self))
    self.AddPage(GoogleDocPage(self))
    self.AddPage(GooglePlusPage(self))
    self.AddPage(YoutubePage(self))
    self.AddPage(BlogspotPage(self))
    self.AddPage(WordpressPage(self))
    self.AddPage(FacebookPage(self))
    self.AddPage(LinkedinPage(self))
    self.AddPage(WikipediaPage(self))
    self.AddPage(TwitterPage(self))
    self.AddPage(PinterestPage(self))
    self.AddPage(ESPNPage(self))
    self.AddPage(WeatherDotComPage(self))
    self.AddPage(YahooGamesPage(self))

    other_urls = [
      # Why: #1 news worldwide (Alexa global)
      'http://news.yahoo.com',
      # Why: #2 news worldwide
      'http://www.cnn.com',
      # Why: #1 world commerce website by visits; #3 commerce in the US by time
      # spent
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
      self.AddPage(Top25Page(url, self))
