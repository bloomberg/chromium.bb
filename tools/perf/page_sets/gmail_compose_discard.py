# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GmailComposeDiscardPage(page_module.PageWithDefaultRunNavigate):

  """ Why: Compose and discard a new email """

  def __init__(self, page_set):
    super(GmailComposeDiscardPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': (
          'window.gmonkey !== undefined &&'
          'document.getElementById("gb") !== null')
      }))

  def ComposeClick(self, action_runner):
    action_runner.RunAction(JavascriptAction({
      'expression': '''
      var button=document.evaluate('//div[text()="COMPOSE"]',
        document,null,XPathResult.FIRST_ORDERED_NODE_TYPE,null)
          .singleNodeValue;
      var mousedownevent=new MouseEvent('mousedown',true,true,window,0,0,0,0,0,
        false,false,false,false,0,null);
      var mouseupevent=new MouseEvent('mouseup',true,true,window,0,0,0,0,0,
        false,false,false,false,0,null);
      button.dispatchEvent(mousedownevent);
      button.dispatchEvent(mouseupevent);'''
    }))

  def RunEndure(self, action_runner):
    action_runner.RunAction(WaitAction(
      {
        'xpath': '//div[text()="COMPOSE"]',
        'condition': 'element'
      }))
    self.ComposeClick(action_runner)
    action_runner.RunAction(WaitAction({"seconds": 1}))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="oh"][data-tooltip="Discard draft"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="oh"][data-tooltip="Discard draft"]'
      }))
    action_runner.RunAction(WaitAction({'seconds': 1}))


class GmailComposeDiscardPageSet(page_set_module.PageSet):

  """
  Description: Gmail endure test: compose and discard an email.
  """

  def __init__(self):
    super(GmailComposeDiscardPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_compose_discard.json')

    self.AddPage(GmailComposeDiscardPage(self))
