# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class BrowserControlPage(page_module.Page):
  """ Why: Continually attach and detach a DOM tree from a basic document. """

  def __init__(self, page_set):
    super(BrowserControlPage, self).__init__(
      url='file://endure/browser_control.html',
      page_set=page_set,
      name='browser_control')
    self.user_agent_type = 'desktop'

  def RunEndure(self, action_runner):
    action_runner.Wait(2)


class BrowserControlPageSet(page_set_module.PageSet):

  """ Chrome Endure control test for the browser. """

  def __init__(self):
    super(BrowserControlPageSet, self).__init__(
      user_agent_type='desktop')

    self.AddPage(BrowserControlPage(self))
