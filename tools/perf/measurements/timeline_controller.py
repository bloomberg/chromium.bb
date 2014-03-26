# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import itertools

from telemetry.core.timeline.model import TimelineModel

# All tracing categories not disabled-by-default
DEFAULT_TRACE_CATEGORIES = None

# Categories for absolute minimum overhead tracing. This contains no
# sub-traces of thread tasks, so it's only useful for capturing the
# cpu-time spent on threads (as well as needed benchmark traces)
MINIMAL_TRACE_CATEGORIES = ("toplevel,"
                            "benchmark,"
                            "webkit.console,"
                            "trace_event_overhead")


class TimelineController(object):
  def __init__(self):
    super(TimelineController, self).__init__()
    self.trace_categories = DEFAULT_TRACE_CATEGORIES
    self._model = None
    self._renderer_process = None
    self._actions = []
    self._action_ranges = []

  def Start(self, page, tab):
    """Starts gathering timeline data.

    """
    # Resets these member variables incase this object is reused.
    self._model = None
    self._renderer_process = None
    self._actions = []
    self._action_ranges = []
    if not tab.browser.supports_tracing:
      raise Exception('Not supported')
    if self.trace_categories:
      categories = [self.trace_categories] + \
          page.GetSyntheticDelayCategories()
    else:
      categories = page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(categories))

  def Stop(self, tab):
    timeline_data = tab.browser.StopTracing()
    self._model = TimelineModel(timeline_data)
    self._renderer_process = self._model.GetRendererProcessFromTab(tab)
    self._action_ranges = [ action.GetActiveRangeOnTimeline(self._model)
                            for action in self._actions ]
    # Make sure no action ranges overlap
    for combo in itertools.combinations(self._action_ranges, 2):
      assert not combo[0].Intersects(combo[1])

  def AddActionToIncludeInMetric(self, action):
    self._actions.append(action)

  @property
  def model(self):
    return self._model

  @property
  def renderer_process(self):
    return self._renderer_process

  @property
  def action_ranges(self):
    return self._action_ranges
