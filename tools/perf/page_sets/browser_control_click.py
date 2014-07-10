# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class BrowserControlClickPage(page_module.Page):

  """ Why: Use a JavaScript .click() call to attach and detach a DOM tree
  from a basic document.
  """

  def __init__(self, page_set):
    super(BrowserControlClickPage, self).__init__(
        url='file://endure/browser_control_click.html',
        page_set=page_set,
        name='browser_control_click')
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement('#attach')

  def RunEndure(self, action_runner):
    action_runner.ClickElement('#attach')
    action_runner.Wait(0.5)
    action_runner.ClickElement('#detach')
    action_runner.Wait(0.5)


class BrowserControlClickPageSet(page_set_module.PageSet):

  """ Chrome Endure control test for the browser. """

  def __init__(self):
    super(BrowserControlClickPageSet, self).__init__(
        user_agent_type='desktop')

    self.AddPage(BrowserControlClickPage(self))
