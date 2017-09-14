# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from contrib.vr_benchmarks.vr_sample_page import VrSamplePage


class WebVrSamplePage(VrSamplePage):

  def __init__(self, page_set, get_parameters):
    super(WebVrSamplePage, self).__init__(
        sample_page='test-slow-render',
        page_set=page_set,
        get_parameters=get_parameters)

  def RunPageInteractions(self, action_runner):
    action_runner.TapElement(selector='canvas[id="webgl-canvas"]')
    action_runner.MeasureMemory(True)


class WebVrSamplePageSet(story.StorySet):
  """A page set using the official WebVR sample with settings tweaked."""

  def __init__(self):
    super(WebVrSamplePageSet, self).__init__()

    # Standard sample app with no changes
    self.AddStory(
        WebVrSamplePage(self, [
            'canvasClickPresents=1',
            'renderScale=1',
        ]))
    # Increased render scale
    self.AddStory(
        WebVrSamplePage(self, [
            'canvasClickPresents=1',
            'renderScale=1.5',
        ]))
    # Default render scale, increased load
    self.AddStory(
        WebVrSamplePage(self, [
            'canvasClickPresents=1',
            'renderScale=1',
            'heavyGpu=1',
            'cubeScale=0.2',
            'workTime=5',
        ]))
    # Further increased load
    self.AddStory(
        WebVrSamplePage(self, [
            'canvasClickPresents=1',
            'renderScale=1',
            'heavyGpu=1',
            'cubeScale=0.3',
            'workTime=10',
        ]))
