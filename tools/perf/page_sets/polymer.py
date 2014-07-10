# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
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
    action_runner.WaitForJavaScriptCondition(
        'window.__polymer_ready')


class PolymerCalculatorPage(PolymerPage):

  def __init__(self, page_set):
    super(PolymerCalculatorPage, self).__init__(
      url='http://localhost:8000/components/paper-calculator/demo.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    self.TapButton(action_runner)
    self.SlidePanel(action_runner)

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
    action_runner.Wait(2)
    interaction.End()

  def SlidePanel(self, action_runner):
    interaction = action_runner.BeginInteraction(
        'Action_SwipeAction', is_smooth=True)
    action_runner.SwipeElement(
        left_start_ratio=0.1, top_start_ratio=0.2,
        direction='left', distance=300, speed_in_pixels_per_second=5000,
        element_function='''
            document.querySelector(
              'body /deep/ #outerPanels'
            ).querySelector(
              '#advanced'
            ).shadowRoot.querySelector(
              '.handle-bar'
            )''')
    action_runner.WaitForJavaScriptCondition('''
        var outer = document.querySelector("body /deep/ #outerPanels");
        outer.opened || outer.wideMode;''')
    interaction.End()


class PolymerShadowPage(PolymerPage):

  def __init__(self, page_set):
    super(PolymerShadowPage, self).__init__(
      url='http://localhost:8000/components/paper-shadow/demo.html',
      page_set=page_set)
    self.archive_data_file = 'data/polymer.json'

  def RunSmoothness(self, action_runner):
    action_runner.ExecuteJavaScript(
        "document.getElementById('fab').scrollIntoView()")
    action_runner.Wait(5)
    self.AnimateShadow(action_runner, 'card')
    self.AnimateShadow(action_runner, 'fab')

  def AnimateShadow(self, action_runner, eid):
    for i in range(1, 6):
      action_runner.ExecuteJavaScript(
          'document.getElementById("{0}").z = {1}'.format(eid, i))
      action_runner.Wait(1)


class PolymerPageSet(page_set_module.PageSet):

  def __init__(self):
    super(PolymerPageSet, self).__init__(
      user_agent_type='mobile',
      archive_data_file='data/polymer.json',
      bucket=page_set_module.INTERNAL_BUCKET)

    self.AddPage(PolymerCalculatorPage(self))
    self.AddPage(PolymerShadowPage(self))
