# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets import webgl_supported_shared_state

from telemetry.page import shared_page_state
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughWebglCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughWebglCasesPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState))
    self.archive_data_file = 'data/tough_webgl_cases.json'


  def RunNavigateSteps(self, action_runner):
    super(ToughWebglCasesPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(5)


class ToughWebglCasesPageSet(page_set_module.PageSet):

  """
  Description: Self-driven WebGL animation examples
  """

  def __init__(self):
    super(ToughWebglCasesPageSet, self).__init__(
      archive_data_file='data/tough_webgl_cases.json',
      bucket=page_set_module.PUBLIC_BUCKET)

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
      self.AddUserStory(ToughWebglCasesPage(url, self))
