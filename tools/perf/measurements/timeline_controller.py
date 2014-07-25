# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import smooth_gesture_util

from telemetry.core.backends.chrome import tracing_backend
from telemetry.timeline.model import TimelineModel
from telemetry.page.actions import action_runner
from telemetry.web_perf import timeline_interaction_record as tir_module


RUN_SMOOTH_ACTIONS = 'RunSmoothAllActions'


class TimelineController(object):
  def __init__(self):
    super(TimelineController, self).__init__()
    self.trace_categories = tracing_backend.DEFAULT_TRACE_CATEGORIES
    self._model = None
    self._renderer_process = None
    self._smooth_records = []
    self._interaction = None

  def SetUp(self, page, tab):
    """Starts gathering timeline data.

    """
    # Resets these member variables incase this object is reused.
    self._model = None
    self._renderer_process = None
    if not tab.browser.supports_tracing:
      raise Exception('Not supported')
    if self.trace_categories:
      categories = [self.trace_categories] + \
          page.GetSyntheticDelayCategories()
    else:
      categories = page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(categories))

  def Start(self, tab):
    # Start the smooth marker for all actions.
    runner = action_runner.ActionRunner(tab)
    self._interaction = runner.BeginInteraction(
        RUN_SMOOTH_ACTIONS, is_smooth=True)

  def Stop(self, tab):
    # End the smooth marker for all actions.
    self._interaction.End()
    # Stop tracing.
    timeline_data = tab.browser.StopTracing()
    self._model = TimelineModel(timeline_data)
    self._renderer_process = self._model.GetRendererProcessFromTabId(tab.id)
    renderer_thread = self.model.GetRendererThreadFromTabId(tab.id)

    run_smooth_actions_record = None
    self._smooth_records = []
    for event in renderer_thread.async_slices:
      if not tir_module.IsTimelineInteractionRecord(event.name):
        continue
      r = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
      if r.label == RUN_SMOOTH_ACTIONS:
        assert run_smooth_actions_record is None, (
          'TimelineController cannot issue more than 1 %s record' %
          RUN_SMOOTH_ACTIONS)
        run_smooth_actions_record = r
      elif r.is_smooth:
        self._smooth_records.append(
          smooth_gesture_util.GetAdjustedInteractionIfContainGesture(
            self.model, r))

    # If there is no other smooth records, we make measurements on time range
    # marked by timeline_controller itself.
    # TODO(nednguyen): when crbug.com/239179 is marked fixed, makes sure that
    # page sets are responsible for issueing the markers themselves.
    if len(self._smooth_records) == 0 and run_smooth_actions_record:
      self._smooth_records = [run_smooth_actions_record]


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
  def smooth_records(self):
    return self._smooth_records
