# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class PolymerPage(page_module.Page):

  def __init__(self, url, page_set):
    super(PolymerPage, self).__init__(
      url=url,
      page_set=page_set)
    self.archive_data_file = "data/polymer.json"
    self.script_to_evaluate_on_commit = '''
      document.addEventListener("polymer-ready", function() {
        window.__polymer_ready = true;
      });
    '''

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.RunAction(WaitAction(
      { 'javascript': "window.__polymer_ready" }))


class PolymerCalculatorPage(PolymerPage):

  def __init__(self, page_set):
    super(PolymerCalculatorPage, self).__init__(
      url='http://localhost:8000/components/paper-calculator/demo.html',
      page_set=page_set)

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
            var outer = document.querySelector("body /deep/ #outerPanels");
            outer.opened || outer.wideMode;
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


class PolymerShadowPage(PolymerPage):

  def __init__(self, page_set):
    super(PolymerShadowPage, self).__init__(
      url='http://localhost:8000/components/paper-shadow/demo.html',
      page_set=page_set)
    self.archive_data_file = 'data/polymer.json'

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(JavascriptAction(
      {
        'expression': "document.getElementById('fab').scrollIntoView()"
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 5
      }))
    self.AnimateShadow(action_runner, 'card')
    self.AnimateShadow(action_runner, 'fab')

  def AnimateShadow(self, action_runner, eid):
    for i in range(1, 6):
      action_runner.RunAction(JavascriptAction(
        {
          'expression': '''
            document.getElementById("{0}").z = {1}
          '''.format(eid, i)
        }))
      action_runner.RunAction(WaitAction(
        {
          'seconds': 1
        }))


class PolymerPageSet(page_set_module.PageSet):

  def __init__(self):
    super(PolymerPageSet, self).__init__(
      user_agent_type='mobile',
      archive_data_file='data/polymer.json')

    self.AddPage(PolymerCalculatorPage(self))
    self.AddPage(PolymerShadowPage(self))
