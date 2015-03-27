# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module


class TopPages(page_module.Page):

  def __init__(self, url, page_set, name='', credentials=None):
    super(TopPages, self).__init__(
        url=url, page_set=page_set, name=name,
        credentials_path='data/credentials.json')
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/top_25.json'
    self.credentials = credentials


class GoogleWebSearchPage(TopPages):

  """ Why: top google property; a google tab is often open """

  def __init__(self, page_set):
    super(GoogleWebSearchPage, self).__init__(
        url='https://www.google.com/#hl=en&q=barack+obama',
        page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(GoogleWebSearchPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GoogleImageSearchPage(TopPages):

  """ Why: tough image case; top google properties """

  def __init__(self, page_set):
    super(GoogleImageSearchPage, self).__init__(
        'https://www.google.com/search?q=cats&tbm=isch',
        page_set=page_set, credentials='google')


class GmailPage(TopPages):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(GmailPage, self).__init__(
        url='https://mail.google.com/mail/',
        page_set=page_set,
        credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GmailPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendarPage(TopPages):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(GoogleCalendarPage, self).__init__(
        url='https://www.google.com/calendar/',
        page_set=page_set,
        credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GoogleCalendarPage, self).RunNavigateSteps(action_runner)
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


class GoogleDocPage(TopPages):

  """ Why: productivity, top google properties; Sample doc in the link """

  def __init__(self, page_set):
    super(GoogleDocPage, self).__init__(
        # pylint: disable=C0301
        url='https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view',
        page_set=page_set,
        name='Docs  (1 open document tab)',
        credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GoogleDocPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')


class GooglePlusPage(TopPages):

  """ Why: social; top google property; Public profile; infinite scrolls """

  def __init__(self, page_set):
    super(GooglePlusPage, self).__init__(
        url='https://plus.google.com/110031535020051778989/posts',
        page_set=page_set,
        credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GooglePlusPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Home')


class YoutubePage(TopPages):

  """ Why: #3 (Alexa global) """

  def __init__(self, page_set):
    super(YoutubePage, self).__init__(
        url='http://www.youtube.com',
        page_set=page_set, credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(YoutubePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class BlogspotPage(TopPages):

  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """

  def __init__(self, page_set):
    super(BlogspotPage, self).__init__(
        url='http://googlewebmastercentral.blogspot.com/',
        page_set=page_set,
        name='Blogger')

  def RunNavigateSteps(self, action_runner):
    super(BlogspotPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='accessibility')


class WordpressPage(TopPages):

  """ Why: #18 (Alexa global), Picked an interesting post """

  def __init__(self, page_set):
    super(WordpressPage, self).__init__(
        # pylint: disable=C0301
        url='http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/',
        page_set=page_set,
        name='Wordpress')

  def RunNavigateSteps(self, action_runner):
    super(WordpressPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(
        # pylint: disable=C0301
        'a[href="http://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]')


class FacebookPage(TopPages):

  """ Why: top social,Public profile """

  def __init__(self, page_set):
    super(FacebookPage, self).__init__(
        url='https://www.facebook.com/barackobama',
        page_set=page_set,
        name='Facebook', credentials='facebook2')

  def RunNavigateSteps(self, action_runner):
    super(FacebookPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Chat')


class LinkedinPage(TopPages):

  """ Why: #12 (Alexa global), Public profile. """

  def __init__(self, page_set):
    super(LinkedinPage, self).__init__(
        url='http://www.linkedin.com/in/linustorvalds', page_set=page_set,
        name='LinkedIn')


class WikipediaPage(TopPages):

  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page. """

  def __init__(self, page_set):
    super(WikipediaPage, self).__init__(
        url='http://en.wikipedia.org/wiki/Wikipedia', page_set=page_set,
        name='Wikipedia (1 tab)')


class TwitterPage(TopPages):

  """ Why: #8 (Alexa global),Picked an interesting page """

  def __init__(self, page_set):
    super(TwitterPage, self).__init__(
        url='https://twitter.com/katyperry',
        page_set=page_set,
        name='Twitter')

  def RunNavigateSteps(self, action_runner):
    super(TwitterPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class PinterestPage(TopPages):

  """ Why: #37 (Alexa global) """

  def __init__(self, page_set):
    super(PinterestPage, self).__init__(
        url='http://pinterest.com',
        page_set=page_set,
        name='Pinterest')


class ESPNPage(TopPages):

  """ Why: #1 sports """

  def __init__(self, page_set):
    super(ESPNPage, self).__init__(
        url='http://espn.go.com',
        page_set=page_set,
        name='ESPN')


class WeatherPage(TopPages):

  """ Why: #7 (Alexa news); #27 total time spent, picked interesting page. """

  def __init__(self, page_set):
    super(WeatherPage, self).__init__(
        url='http://www.weather.com/weather/right-now/Mountain+View+CA+94043',
        page_set=page_set,
        name='Weather.com')


class YahooGamesPage(TopPages):

  """ Why: #1 games according to Alexa (with actual games in it) """

  def __init__(self, page_set):
    super(YahooGamesPage, self).__init__(
        url='http://games.yahoo.com',
        page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(YahooGamesPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
