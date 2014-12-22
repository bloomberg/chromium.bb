# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.timeline.model import TimelineModel
from telemetry.page import page_test
from telemetry.util import statistics
from telemetry.value import scalar

_CATEGORIES = ['webkit.console',
               'blink.console',
               'benchmark',
               'toplevel',
               'blink',
               'blink_gc',
               'cc',
               'v8']

class TaskExecutionTime(page_test.PageTest):

  _TIME_OUT_IN_SECONDS = 60
  _NUMBER_OF_RESULTS_TO_DISPLAY = 10

  def __init__(self):
    super(TaskExecutionTime, self).__init__('RunSmoothness')
    self._renderer_thread = None

  def WillNavigateToPage(self, page, tab):
    category_filter = tracing_category_filter.TracingCategoryFilter()

    for category in _CATEGORIES:
      category_filter.AddIncludedCategory(category)

    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True

    tab.browser.platform.tracing_controller.Start(
        options, category_filter, self._TIME_OUT_IN_SECONDS)

  def DidRunActions(self, page, tab):
    trace_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = TimelineModel(trace_data)
    self._renderer_thread = timeline_model.GetRendererThreadFromTabId(tab.id)

  def ValidateAndMeasurePage(self, page, tab, results):
    tasks = TaskExecutionTime.GetTasks(self._renderer_thread.parent)

    sorted_tasks = sorted(tasks,
      key=lambda slice: slice.median_self_duration, reverse=True)

    for task in sorted_tasks[:self.GetExpectedResultCount()]:
      results.AddValue(scalar.ScalarValue(
        results.current_page,
        task.key,
        'ms',
        task.median_self_duration,
        description = 'Slowest tasks'))

  @staticmethod
  def GetTasks(process):
    task_dictionary = {}
    depth = 1
    for parent, task_slice in enumerate(
          process.IterAllSlicesOfName('MessageLoop::RunTask')):
      ProcessTasksForSlice(task_dictionary, task_slice, depth, parent)
    # Return dictionary flattened into a list
    return task_dictionary.values()

  @staticmethod
  def GetExpectedResultCount():
    return TaskExecutionTime._NUMBER_OF_RESULTS_TO_DISPLAY


class SliceData:
  def __init__(self, key, self_duration, total_duration, depth, parent):
    self.key = key
    self.count = 1

    self.self_durations = [self_duration]
    self.min_self_duration = self_duration
    self.max_self_duration = self_duration

    self.total_durations = [total_duration]
    self.min_total_duration = total_duration
    self.max_total_duration = total_duration

    self.tree_location = [(depth, parent)]

  def Update(self, self_duration, total_duration, depth, parent):
    self.count += 1

    self.self_durations.append(self_duration)
    self.min_self_duration = min(self.min_self_duration, self_duration)
    self.max_self_duration = max(self.max_self_duration, self_duration)

    self.total_durations.append(total_duration)
    self.min_total_duration = min(self.min_total_duration, total_duration)
    self.max_total_duration = max(self.max_total_duration, total_duration)

    self.tree_location.append((depth, parent))

  @property
  def median_self_duration(self):
    return statistics.Median(self.self_durations)


def ProcessTasksForSlice(dictionary, task_slice, depth, parent):
  # Deal with TRACE_EVENT_INSTANTs that have no duration
  self_duration = task_slice.self_thread_time or 0.0
  total_duration = task_slice.thread_duration or 0.0

  # There is at least one task that is tracked as both uppercase and lowercase,
  # forcing the name to lowercase coalesces both.
  key = task_slice.name.lower()
  if key in dictionary:
    dictionary[key].Update(
        self_duration, total_duration, depth, parent)
  else:
    dictionary[key] = SliceData(
        key, self_duration, total_duration, depth, parent)

  # Process sub slices recursively
  for sub_slice in task_slice.sub_slices:
    ProcessTasksForSlice(dictionary, sub_slice, depth + 1, parent)
