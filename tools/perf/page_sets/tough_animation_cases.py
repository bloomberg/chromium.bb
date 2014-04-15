# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughAnimationCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set, need_measurement_ready):
    super(ToughAnimationCasesPage, self).__init__(url=url, page_set=page_set)
    self.archive_data_file = 'data/tough_animation_cases.json'
    self._need_measurement_ready = need_measurement_ready

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    if self._need_measurement_ready:
      action_runner.RunAction(WaitAction({"javascript": "measurementReady"}))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({"seconds": 10}))

class ToughAnimationCasesPageSet(page_set_module.PageSet):

  """
  Description: A collection of animation performance tests
  """

  def __init__(self):
    super(ToughAnimationCasesPageSet, self).__init__(
      archive_data_file='data/tough_animation_cases.json')

    urls_list_one = [
      # Why: Tests the balls animation implemented with SVG animations.
      'file://tough_animation_cases/balls_svg_animations.html',
      # Why: Tests the balls animation implemented with Javascript and canvas.
      'file://tough_animation_cases/balls_javascript_canvas.html',
      # Why: Tests the balls animation implemented with Javascript and CSS.
      'file://tough_animation_cases/balls_javascript_css.html',
      # Why: Tests the balls animation implemented with CSS keyframe animations.
      'file://tough_animation_cases/balls_css_keyframe_animations.html',
      # Why: Tests the balls animation implemented with transforms and CSS
      # keyframe animations to be run on the compositor thread.
      # pylint: disable=C0301
      'file://tough_animation_cases/balls_css_keyframe_animations_composited_transform.html',
      # Why: Tests the balls animation implemented with CSS transitions on 2
      # properties.
      'file://tough_animation_cases/balls_css_transition_2_properties.html',
      # Why: Tests the balls animation implemented with CSS transitions on 40
      # properties.
      'file://tough_animation_cases/balls_css_transition_40_properties.html',
      # Why: Tests the balls animation implemented with CSS transitions on all
      # animatable properties.
      'file://tough_animation_cases/balls_css_transition_all_properties.html',
      # pylint: disable=C0301
      'file://tough_animation_cases/overlay_background_color_css_transitions.html'
    ]

    for url in urls_list_one:
      self.AddPage(ToughAnimationCasesPage(url, self,
                                           need_measurement_ready=True))

    urls_list_two = [
      # Why: Tests various keyframed animations.
      'file://tough_animation_cases/keyframed_animations.html',
      # Why: Tests various transitions.
      'file://tough_animation_cases/transform_transitions.html',
      # Why: Login page is slow because of ineffecient transform operations.
      'http://ie.microsoft.com/testdrive/performance/robohornetpro/',
      # Why: JS execution blocks CSS transition unless initial transform is set.
      'file://tough_animation_cases/transform_transition_js_block.html'
    ]

    for url in urls_list_two:
      self.AddPage(ToughAnimationCasesPage(url, self,
                                           need_measurement_ready=False))
