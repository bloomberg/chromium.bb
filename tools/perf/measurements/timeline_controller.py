# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core.timeline.model import TimelineModel
from telemetry.page.actions import action_runner
from telemetry.web_perf import timeline_interaction_record as tir_module

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
    self._action_ranges = []
    self._interaction_record = None

  def Start(self, page, tab):
    """Starts gathering timeline data.

    """
    # Resets these member variables incase this object is reused.
    self._model = None
    self._renderer_process = None
    self._action_ranges = []
    if not tab.browser.supports_tracing:
      raise Exception('Not supported')
    if self.trace_categories:
      categories = [self.trace_categories] + \
          page.GetSyntheticDelayCategories()
    else:
      categories = page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(categories))
    # Start the smooth marker for all actions.
    runner = action_runner.ActionRunner(None, tab)
    runner.BeginInteraction('RunActions', [tir_module.IS_SMOOTH])

  def Stop(self, tab):
    # End the smooth marker for all actions.
    runner = action_runner.ActionRunner(None, tab)
    runner.EndInteraction('RunActions', [tir_module.IS_SMOOTH])
    # Stop tracing.
    timeline_data = tab.browser.StopTracing()
    self._model = TimelineModel(timeline_data)
    self._renderer_process = self._model.GetRendererProcessFromTab(tab)
    renderer_thread = self.model.GetRendererThreadFromTab(tab)
    records = [tir_module.TimelineInteractionRecord.FromEvent(event) for
               event in renderer_thread.async_slices if
               tir_module.IsTimelineInteractionRecord(event.name)]
    self._action_ranges = [r.GetBounds() for r in records]
    if records:
      assert len(records) == 1
      self._interaction_record = records[0]


  def CleanUp(self, tab):
    if tab.browser.is_tracing_running:
      tab.browser.StopTracing()

  @property
  def model(self):
    return self._model

  @property
  def renderer_process(self):
    return self._renderer_process

  @property
  def action_ranges(self):
    return self._action_ranges

  @property
  def interaction_record(self):
    return self._interaction_record
