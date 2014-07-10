# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


def _CreateXpathFunction(xpath):
  return ('document.evaluate("%s",'
                             'document,'
                             'null,'
                             'XPathResult.FIRST_ORDERED_NODE_TYPE,'
                             'null)'
          '.singleNodeValue' % re.escape(xpath))


class GmailComposeDiscardPage(page_module.Page):

  """ Why: Compose and discard a new email """

  def __init__(self, page_set):
    super(GmailComposeDiscardPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')

  def ComposeClick(self, action_runner):
    action_runner.ExecuteJavaScript('''
      var button=document.evaluate('//div[text()="COMPOSE"]',
          document,null,XPathResult.FIRST_ORDERED_NODE_TYPE,null)
          .singleNodeValue;
      var mousedownevent=new MouseEvent('mousedown',true,true,window,0,0,0,0,0,
        false,false,false,false,0,null);
      var mouseupevent=new MouseEvent('mouseup',true,true,window,0,0,0,0,0,
        false,false,false,false,0,null);
      button.dispatchEvent(mousedownevent);
      button.dispatchEvent(mouseupevent);''')

  def RunEndure(self, action_runner):
    action_runner.WaitForElement(
        element_function=_CreateXpathFunction('//div[text()="COMPOSE"]'))
    self.ComposeClick(action_runner)
    action_runner.Wait(1)
    action_runner.WaitForElement(
        'div[class~="oh"][data-tooltip="Discard draft"]')
    action_runner.ClickElement('div[class~="oh"][data-tooltip="Discard draft"]')
    action_runner.Wait(1)


class GmailComposeDiscardPageSet(page_set_module.PageSet):

  """
  Description: Gmail endure test: compose and discard an email.
  """

  def __init__(self):
    super(GmailComposeDiscardPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop')

    self.AddPage(GmailComposeDiscardPage(self))
