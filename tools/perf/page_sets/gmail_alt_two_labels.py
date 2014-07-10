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


class GmailAltTwoLabelsPage(page_module.Page):

  """ Why: Alternate between Inbox and Sent Mail """

  def __init__(self, page_set):
    super(GmailAltTwoLabelsPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set,
      name='gmail_alt_two_labels')

    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/gmail_alt_two_labels.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined && '
        'document.getElementById("gb") !== null')

  def RunEndure(self, action_runner):
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        'a[href="https://mail.google.com/mail/u/0/?shva=1#sent"]')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.Wait(1)
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        'a[href="https://mail.google.com/mail/u/0/?shva=1#inbox"]')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.Wait(1)


class GmailAltTwoLabelsPageSet(page_set_module.PageSet):

  """ Chrome Endure test for GMail. """

  def __init__(self):
    super(GmailAltTwoLabelsPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_alt_two_labels.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(GmailAltTwoLabelsPage(self))
