# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.page import page_test
from telemetry.timeline.model import TimelineModel
from telemetry.util import statistics
from telemetry.value import scalar


class TaskExecutionTime(page_test.PageTest):

  _TIME_OUT_IN_SECONDS = 60
  _NUMBER_OF_RESULTS_TO_DISPLAY = 10
  _BROWSER_THREADS = ['Chrome_ChildIOThread',
                      'Chrome_IOThread']
  _RENDERER_THREADS = ['Chrome_ChildIOThread',
                       'Chrome_IOThread',
                       'CrRendererMain']
  _CATEGORIES = ['benchmark',
                 'blink',
                 'blink.console',
                 'blink_gc',
                 'cc',
                 'gpu',
                 'ipc',
                 'toplevel',
                 'v8',
                 'webkit.console']

  def __init__(self):
    super(TaskExecutionTime, self).__init__('RunPageInteractions')
    self._renderer_process = None
    self._browser_process = None
    self._results = None

  def WillNavigateToPage(self, page, tab):
    category_filter = tracing_category_filter.TracingCategoryFilter()

    for category in self._CATEGORIES:
      category_filter.AddIncludedCategory(category)

    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True

    tab.browser.platform.tracing_controller.Start(
        options, category_filter, self._TIME_OUT_IN_SECONDS)

  def DidRunActions(self, page, tab):
    trace_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = TimelineModel(trace_data)

    self._renderer_process = timeline_model.GetRendererProcessFromTabId(tab.id)
    self._browser_process = timeline_model.browser_process

  def ValidateAndMeasurePage(self, page, tab, results):
    self._results = results

    for thread in self._BROWSER_THREADS:
      self._AddTasksToResults(self._browser_process, thread)

    for thread in self._RENDERER_THREADS:
      self._AddTasksToResults(self._renderer_process, thread)

  def _AddTasksToResults(self, process, thread_name):
    if process is None:
      return

    tasks = TaskExecutionTime._GetTasksForThread(process, thread_name)

    sorted_tasks = sorted(
        tasks,
        key=lambda slice: slice.median_self_duration,
        reverse=True)

    for task in sorted_tasks[:self.GetExpectedResultCount()]:
      self._results.AddValue(scalar.ScalarValue(
          self._results.current_page,
          task.key,
          'ms',
          task.median_self_duration,
          description='Slowest tasks'))

  @staticmethod
  def _GetTasksForThread(process, target_thread):
    task_dictionary = {}
    depth = 1

    for thread in process.threads.values():
      if thread.name != target_thread:
        continue
      for parent, task_slice in enumerate(thread.IterAllSlices()):
        _ProcessTasksForThread(
            task_dictionary,
            process.name + ':' + thread.name,
            task_slice,
            depth,
            parent)

    return task_dictionary.values()

  @staticmethod
  def GetExpectedResultCount():
    return TaskExecutionTime._NUMBER_OF_RESULTS_TO_DISPLAY


def _ProcessTasksForThread(dictionary, thread_name, task_slice, depth, parent):
  if task_slice.self_thread_time is None:
    # Early out if this slice is a TRACE_EVENT_INSTANT, as it has no duration.
    return

  self_duration = task_slice.self_thread_time

  reported_name = thread_name + ':'

  if 'src_func' in task_slice.args:
    # Data contains the name of the timed function, use it as the name.
    reported_name += task_slice.args['src_func']
  elif 'line' in task_slice.args:
    # Data contains IPC class and line numbers, use these as the name.
    reported_name += 'IPC_Class_' + str(task_slice.args['class'])
    reported_name += ':Line_' + str(task_slice.args['line'])
  else:
    # Fallback to use the name of the task slice.
    reported_name += task_slice.name.lower()

  # Replace any '.'s with '_'s as V8 uses them and it confuses the dashboard.
  reported_name = reported_name.replace('.', '_')

  if reported_name in dictionary:
    # The dictionary contains an entry for this (e.g. from an earlier slice),
    # add the new duration so we can calculate a median value later on.
    dictionary[reported_name].Update(self_duration, depth, parent)
  else:
    # This is a new task so create a new entry for it
    dictionary[reported_name] = SliceData(
        reported_name,
        self_duration,
        depth,
        parent)

  # Process sub slices recursively.
  for sub_slice in task_slice.sub_slices:
    _ProcessTasksForThread(
        dictionary,
        thread_name,
        sub_slice,
        depth + 1,
        parent)


class SliceData:

  def __init__(self, key, self_duration, depth, parent):
    self.key = key
    self.self_durations = [self_duration]
    self.tree_location = [(depth, parent)]

  def Update(self, self_duration, depth, parent):
    self.self_durations.append(self_duration)
    self.tree_location.append((depth, parent))

  @property
  def median_self_duration(self):
    return statistics.Median(self.self_durations)
