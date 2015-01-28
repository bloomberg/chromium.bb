# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SimplePage(page_module.Page):
  def __init__(self, url, page_set, credentials='', name=''):
    super(SimplePage, self).__init__(
        url, page_set=page_set, name=name,
        credentials_path='data/credentials.json')
    self.credentials = credentials

  def RunPageInteractions(self, action_runner):
    pass

class Google(SimplePage):
  def __init__(self, page_set):
    super(Google, self).__init__(
      url='https://www.google.com/#hl=en&q=barack+obama', page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(Google, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class Gmail(SimplePage):
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


class GoogleCalendar(SimplePage):
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


class Youtube(SimplePage):
  def __init__(self, page_set):
    super(Youtube, self).__init__(
      url='http://www.youtube.com',
      page_set=page_set,
      credentials='google')

  def RunNavigateSteps(self, action_runner):
    super(Youtube, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Facebook(SimplePage):
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
      user_agent_type='desktop',
      bucket=page_set_module.PARTNER_BUCKET)

    # top google property; a google tab is often open
    self.AddUserStory(Google(self))

    # productivity, top google properties
    # TODO(dominikg): fix crbug.com/386152
    #self.AddUserStory(Gmail(self))

    # productivity, top google properties
    self.AddUserStory(GoogleCalendar(self))

    # #3 (Alexa global)
    self.AddUserStory(Youtube(self))

    # top social, Public profile
    self.AddUserStory(Facebook(self))

    # #6 (Alexa) most visited worldwide,Picked an interesting page
    self.AddUserStory(SimplePage('http://en.wikipedia.org/wiki/Wikipedia',
                                  self, name='Wikipedia'))

    # #1 world commerce website by visits; #3 commerce in the US by time spent
    self.AddUserStory(SimplePage('http://www.amazon.com', self))

    # #4 Alexa
    self.AddUserStory(SimplePage('http://www.yahoo.com/', self))

    # #16 Alexa
    self.AddUserStory(SimplePage('http://www.bing.com/', self))

    # #20 Alexa
    self.AddUserStory(SimplePage('http://www.ask.com/', self))
