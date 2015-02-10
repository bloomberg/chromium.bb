# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

_SCROLL_LIST_ITEM = ('.scroll-list-item .%s')
_NUM_REPEATS_FOR_EMAIL_OPEN = 40
_OPEN_CLOSE_WAIT_TIME = 0.3


class InboxPage(page_module.Page):
  """A class defining inbox page for telemetry."""

  def __init__(self, page_set):
    super(InboxPage, self).__init__(
        url=('http://localhost/?mark_read_on_open=false&'
             'jsmode=du&preload_anim=True&welcome=0'),
        page_set=page_set,
        credentials_path='data/inbox_credentials.json',
        name='Inbox')
    self.user_agent_type = 'desktop'
    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self, 180)

  def RunPageInteractions(self, action_runner):
    self.OpenCloseConv(action_runner)

  def _WaitForItemOpen(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        'BT_AutoTestHelper.getInstance().getOpenItemId() && '
        'BT_AutoTestHelper.getInstance().'
        'getOpenItemLoadedRenderComplete() && '
        'BT_AutoTestHelper.getInstance().getOutstandingRenderCount() '
        '== 0')

  def _WaitForItemClose(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        '!BT_AutoTestHelper.getInstance().getOpenItemId() && '
        'BT_AutoTestHelper.getInstance().getOutstandingRenderCount() '
        '== 0')

  def _ClickToOpenItem(self, action_runner):
    action_runner.MouseClick(_SCROLL_LIST_ITEM % 'summItemSection_')

  def _ClickToCloseItem(self, action_runner):
    action_runner.MouseClick(_SCROLL_LIST_ITEM % 'fullItemHeader_')

  def _WaitForInitialSync(self, action_runner):
    # This wait is to make sure that the actual background sync starts
    # properly and we are  not capturing the initial default synced status.
    action_runner.Wait(120)
    action_runner.WaitForJavaScriptCondition(
        'window.BT_events && '
        'BT_events.ITEMS_LOADED && BT_events.INITIAL_SYNC_COMPLETE', 90)

  def _OpenAndCloseEmail(self, action_runner):
    self._ClickToOpenItem(action_runner)
    self._WaitForItemOpen(action_runner)
    action_runner.Wait(_OPEN_CLOSE_WAIT_TIME)
    self._ClickToCloseItem(action_runner)
    self._WaitForItemClose(action_runner)
    action_runner.Wait(_OPEN_CLOSE_WAIT_TIME)

  def OpenCloseConv(self, action_runner):
    # Wait for the page to load and start background sync
    self._WaitForInitialSync(action_runner)
    for _ in range(0, _NUM_REPEATS_FOR_EMAIL_OPEN):
      self._OpenAndCloseEmail(action_runner)
    action_runner.Wait(10)


class InboxPageSet(page_set_module.PageSet):

  """Inbox page set."""

  def __init__(self):
    super(InboxPageSet, self).__init__(
        user_agent_type='desktop',
        archive_data_file='data/inbox_data.json',
        bucket=page_set_module.INTERNAL_BUCKET
        )

    self.AddUserStory(InboxPage(self))
