# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class KeyHitTestCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(KeyHitTestCasesPage, self).__init__(
        url=url, page_set=page_set, credentials_path = 'data/credentials.json')
    self.user_agent_type = 'mobile'

  def RunNavigateSteps(self, action_runner):
    super(KeyHitTestCasesPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(2)
    for _ in xrange(100):
        self.TapButton(action_runner)


class PaperCalculatorHitTest(KeyHitTestCasesPage):

  def __init__(self, page_set):
    super(PaperCalculatorHitTest, self).__init__(
      # Generated from https://github.com/zqureshi/paper-calculator
      # vulcanize --inline --strip paper-calculator/demo.html
      url='file://key_hit_test_cases/paper-calculator-no-rendering.html',
      page_set=page_set)

  def TapButton(self, action_runner):
    interaction = action_runner.BeginInteraction(
        'Action_TapAction', is_smooth=True)
    action_runner.TapElement(element_function='''
        document.querySelector(
            'body /deep/ #outerPanels'
        ).querySelector(
            '#standard'
        ).shadowRoot.querySelector(
            'paper-calculator-key[label="5"]'
        )''')
    interaction.End()


class KeyHitTestCasesPageSet(page_set_module.PageSet):

  def __init__(self):
    super(KeyHitTestCasesPageSet, self).__init__(
      user_agent_type='mobile')

    self.AddUserStory(PaperCalculatorHitTest(self))
