# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SimpleScrollPage(page_module.Page):
  def __init__(self, url, page_set, credentials='', name=''):
    super(SimpleScrollPage, self).__init__(url, page_set=page_set, name=name)
    self.credentials = credentials

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()

class Google(SimpleScrollPage):
  def __init__(self, page_set):
    super(Google, self).__init__(
      url='https://www.google.com/#hl=en&q=barack+obama', page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(Google, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class Gmail(SimpleScrollPage):
  def __init__(self, page_set):
    super(Gmail, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set,
      credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(Gmail, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendar(SimpleScrollPage):
  def __init__(self, page_set):
    super(GoogleCalendar, self).__init__(
      url='https://www.google.com/calendar/',
      page_set=page_set,
      credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(GoogleCalendar, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript('''
        (function() { var elem = document.createElement("meta");
          elem.name="viewport";
          elem.content="initial-scale=1";
          document.body.appendChild(elem);
        })();''')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')


class Youtube(SimpleScrollPage):
  def __init__(self, page_set):
    super(Youtube, self).__init__(
      url='http://www.youtube.com',
      page_set=page_set,
      credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(Youtube, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Facebook(SimpleScrollPage):
  def __init__(self, page_set):
    super(Facebook, self).__init__(
      url='http://www.facebook.com/barackobama',
      page_set=page_set,
      credentials='facebook',
      name='Facebook')

  def RunNavigateSteps(self, action_runner):
    super(Facebook, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='About')


class Top10PageSet(page_set_module.PageSet):
  """10 Pages chosen from Alexa top sites"""

  def __init__(self):
    super(Top10PageSet, self).__init__(
      archive_data_file='data/top_10.json',
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      bucket=page_set_module.PARTNER_BUCKET)

    # top google property; a google tab is often open
    self.AddPage(Google(self))

    # productivity, top google properties
    # TODO(dominikg): fix crbug.com/386152
    #self.AddPage(Gmail(self))

    # productivity, top google properties
    self.AddPage(GoogleCalendar(self))

    # #3 (Alexa global)
    self.AddPage(Youtube(self))

    # top social, Public profile
    self.AddPage(Facebook(self))

    # #6 (Alexa) most visited worldwide,Picked an interesting page
    self.AddPage(SimpleScrollPage('http://en.wikipedia.org/wiki/Wikipedia',
                                  self, name='Wikipedia'))

    # #1 world commerce website by visits; #3 commerce in the US by time spent
    self.AddPage(SimpleScrollPage('http://www.amazon.com', self))

    # #4 Alexa
    self.AddPage(SimpleScrollPage('http://www.yahoo.com/', self))

    # #16 Alexa
    self.AddPage(SimpleScrollPage('http://www.bing.com/', self))

    # #20 Alexa
    self.AddPage(SimpleScrollPage('http://www.ask.com/', self))
