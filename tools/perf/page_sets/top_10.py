# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page.actions.navigate import NavigateAction
from telemetry.page.actions.javascript import JavascriptAction
from telemetry.page.actions.scroll import ScrollAction
from telemetry.page.actions.wait import WaitAction
from telemetry.page.page_set import PageSet
from telemetry.page.page import Page


class SimpleScrollPage(Page):
  def __init__(self, url='', credentials=''):
    super(SimpleScrollPage, self).__init__(url, '')
    self.credentials = credentials

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())


class Google(SimpleScrollPage):
  def __init__(self):
    super(Google, self).__init__(
      'https://www.google.com/#hl=en&q=barack+obama')

  def RunNavigateSteps(self, action_runner):
    super(Google, self).RunNavigateSteps(action_runner)
    action_runner.RunAction(WaitAction(
      {'condition': 'element', 'text': 'Next'}))


class Gmail(SimpleScrollPage):
  def __init__(self):
    super(Gmail, self).__init__(
      'https://mail.google.com/mail/', credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(Gmail, self).RunNavigateSteps(action_runner)
    action_runner.RunAction(WaitAction(
      {'javascript' : 'window.gmonkey !== undefined &&'
       'document.getElementById("gb") !== null'}))


class GoogleCalendar(SimpleScrollPage):
  def __init__(self):
    super(GoogleCalendar, self).__init__(
      'https://www.google.com/calendar/', credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GoogleCalendar, self).RunNavigateSteps(action_runner)
    action_runner.RunAction(JavascriptAction(
      { 'expression' :
       '(function() { var elem = document.createElement("meta");'
      'elem.name="viewport";'
      'elem.content="initial-scale=1";'
      'document.body.appendChild(elem); })();'}))
    action_runner.RunAction(WaitAction({'seconds' : 2}))
    action_runner.RunAction(WaitAction({
      'condition' : 'element', 'selector' : 'div[class~="navForward"]'}))


class Youtube(SimpleScrollPage):
  def __init__(self):
    super(Youtube, self).__init__(
      'http://www.youtube.com', credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(Youtube, self).RunNavigateSteps(action_runner)
    action_runner.RunAction(WaitAction({'seconds' : 2}))


class Facebook(SimpleScrollPage):
  def __init__(self):
    super(Facebook, self).__init__(
      'http://www.facebook.com/barackobama', credentials='facebook')
    self.name = "Facebook"

  def RunNavigateSteps(self, action_runner):
    super(Facebook, self).RunNavigateSteps(action_runner)
    action_runner.RunAction(WaitAction(
      {'condition': 'element', 'text': 'About'}))


class Top10PageSet(PageSet):
  def __init__(self):
    super(Top10PageSet, self).__init__(
      description='10 Pages chosen from Alexa top sites',
      archive_data_file='data/top_10.json',
      credentials_path='data/credentials.json',
      user_agent_type='desktop')

    # top google property; a google tab is often open
    self.AddPage(Google())

    # productivity, top google properties
    self.AddPage(Gmail())

    # productivity, top google properties
    self.AddPage(GoogleCalendar())

    # #3 (Alexa global)
    self.AddPage(Youtube())

    # top social, Public profile
    self.AddPage(Facebook())

    # #6 (Alexa) most visited worldwide,Picked an interesting page
    wikipedia_page = SimpleScrollPage('http://en.wikipedia.org/wiki/Wikipedia')
    wikipedia_page.name = "Wikipedia"
    self.AddPage(wikipedia_page)

    # #1 world commerce website by visits; #3 commerce in the US by time spent
    self.AddPage(SimpleScrollPage('http://www.amazon.com'))

    # #4 Alexa
    self.AddPage(SimpleScrollPage('http://www.yahoo.com/'))

    # #16 Alexa
    self.AddPage(SimpleScrollPage('http://www.bing.com/'))

    # #20 Alexa
    self.AddPage(SimpleScrollPage('http://www.ask.com/'))
