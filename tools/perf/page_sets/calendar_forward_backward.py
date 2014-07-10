# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class CalendarForwardBackwardPage(page_module.Page):

  """ Why: Click forward(4x) and backwards(4x) repeatedly """

  def __init__(self, page_set):
    super(CalendarForwardBackwardPage, self).__init__(
      url='https://www.google.com/calendar/',
      page_set=page_set,
      name='calendar_forward_backward')
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/calendar_forward_backward.json'

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

  def RunEndure(self, action_runner):
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
    action_runner.WaitForElement('div[class~="navForward"]')


class CalendarForwardBackwardPageSet(page_set_module.PageSet):

  """ Chrome Endure test for Google Calendar. """

  def __init__(self):
    super(CalendarForwardBackwardPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/calendar_forward_backward.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(CalendarForwardBackwardPage(self))
