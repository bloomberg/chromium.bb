# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughWebglCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(ToughWebglCasesPage, self).__init__(url=url, page_set=page_set)
    self.archive_data_file = 'data/tough_webgl_cases.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.readyState == "complete"'
      }))
    action_runner.RunAction(WaitAction({'seconds': 2}))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 5}))


class Page1(ToughWebglCasesPage):

  """
  Why: Observed performance regression with this demo in M33
  """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='http://montagestudio.com/demos/eco-homes/',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.readyState == "complete"'
      }))
    action_runner.RunAction(WaitAction({'seconds': 15}))


class Page2(ToughWebglCasesPage):

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='http://helloracer.com/racer-s/',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.readyState == "complete"'
      }))
    action_runner.RunAction(WaitAction({'seconds': 10}))


class ToughWebglCasesPageSet(page_set_module.PageSet):

  """
  Description: Self-driven WebGL animation examples
  """

  def __init__(self):
    super(ToughWebglCasesPageSet, self).__init__(
      archive_data_file='data/tough_webgl_cases.json')

    self.AddPage(Page1(self))
    self.AddPage(Page2(self))
    urls_list = [
      # pylint: disable=C0301
      'http://www.khronos.org/registry/webgl/sdk/demos/google/nvidia-vertex-buffer-object/index.html',
      # pylint: disable=C0301
      'http://www.khronos.org/registry/webgl/sdk/demos/google/san-angeles/index.html',
      # pylint: disable=C0301
      'http://www.khronos.org/registry/webgl/sdk/demos/google/particles/index.html',
      'http://www.khronos.org/registry/webgl/sdk/demos/webkit/Earth.html',
      # pylint: disable=C0301
      'http://www.khronos.org/registry/webgl/sdk/demos/webkit/ManyPlanetsDeep.html',
      'http://webglsamples.googlecode.com/hg/aquarium/aquarium.html',
      'http://webglsamples.googlecode.com/hg/blob/blob.html',
      # pylint: disable=C0301
      'http://webglsamples.googlecode.com/hg/dynamic-cubemap/dynamic-cubemap.html'
    ]
    for url in urls_list:
      self.AddPage(ToughWebglCasesPage(url, self))
