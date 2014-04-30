# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class PolymerCalculatorPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, page_set):
    super(PolymerCalculatorPage, self).__init__(
      url='http://localhost:8000/components/paper-calculator/demo.html',
      page_set=page_set)
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/polymer.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))

  def RunSmoothness(self, action_runner):
    self.TapButton(action_runner)
    self.SlidePanel(action_runner)

  def TapButton(self, action_runner):
    action_runner.RunAction(TapAction(
      {
        'element_function': '''
          function(callback) {
            callback(
              document.querySelector(
                'body /deep/ #outerPanels'
              ).querySelector(
                '#standard'
              ).shadowRoot.querySelector(
                'paper-calculator-key[label="5"]'
              )
            );
          }''',
        'wait_after': { 'seconds': 2 }
      }))

  def SlidePanel(self, action_runner):
    action_runner.RunAction(SwipeAction(
      {
        'left_start_percentage': 0.1,
        'distance': 300,
        'direction': 'left',
        'wait_after': {
          'javascript': '''
            (o = document.querySelector(
              "body /deep/ #outerPanels"
            )), o.opened || o.wideMode
          '''
        },
        'top_start_percentage': 0.2,
        'element_function': '''
          function(callback) {
            callback(
              document.querySelector(
                'body /deep/ #outerPanels'
              ).querySelector(
                '#advanced'
              ).shadowRoot.querySelector(
                '.handle-bar'
              )
            );
          }''',
        'speed': 5000
      }))


class PolymerPageSet(page_set_module.PageSet):

  def __init__(self):
    super(PolymerPageSet, self).__init__(
      user_agent_type='mobile',
      archive_data_file='data/polymer.json')

    self.AddPage(PolymerCalculatorPage(self))
