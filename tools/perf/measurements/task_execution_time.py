# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.page import page_test
from telemetry.timeline.model import TimelineModel
from telemetry.util import statistics
from telemetry.value import scalar

# Set up named tuple to use as dictionary key.
TaskKey = namedtuple('TaskKey', 'name section')


class TaskExecutionTime(page_test.PageTest):

  IDLE_SECTION_TRIGGER = 'SingleThreadIdleTaskRunner::RunTask'
  IDLE_SECTION = 'IDLE'

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
                 'renderer.scheduler',
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
      self._AddTasksFromThreadToResults(self._browser_process, thread)

    for thread in self._RENDERER_THREADS:
      self._AddTasksFromThreadToResults(self._renderer_process, thread)

  def _AddTasksFromThreadToResults(self, process, thread_name):
    if process is None:
      return

    tasks = TaskExecutionTime._GetTasksForThread(process, thread_name)

    # Pull out all the unique sections used by the returned tasks.
    sections = set([key.section for key in tasks.iterkeys()])

    # Remove sections we don't care about (currently just Idle tasks).
    if TaskExecutionTime.IDLE_SECTION in sections:
      sections.remove(TaskExecutionTime.IDLE_SECTION)

    # Create top N list for each section. Note: This nested loop (n-squared)
    # solution will become inefficient as the number of sections grows.
    # Consider a refactor to pre-split the tasks into section buckets if this
    # happens and performance becomes an isssue.
    for section in sections:
      task_list = [tasks[key] for key in tasks if key.section == section]
      self._AddSlowestTasksToResults(task_list)

  def _AddSlowestTasksToResults(self, tasks):
    sorted_tasks = sorted(
        tasks,
        key=lambda slice: slice.median_self_duration,
        reverse=True)

    for task in sorted_tasks[:self.GetExpectedResultCount()]:
      self._results.AddValue(scalar.ScalarValue(
          self._results.current_page,
          task.name,
          'ms',
          task.median_self_duration,
          description='Slowest tasks'))

  @staticmethod
  def _GetTasksForThread(process, target_thread):
    task_durations = {}

    for thread in process.threads.values():
      if thread.name != target_thread:
        continue
      for task_slice in thread.IterAllSlices():
        _ProcessTasksForThread(
            task_durations,
            process.name + ':' + thread.name,
            task_slice)

    return task_durations

  @staticmethod
  def GetExpectedResultCount():
    return TaskExecutionTime._NUMBER_OF_RESULTS_TO_DISPLAY


def _ProcessTasksForThread(
    task_durations,
    thread_name,
    task_slice,
    section=None):
  if task_slice.self_thread_time is None:
    # Early out if this slice is a TRACE_EVENT_INSTANT, as it has no duration.
    return

  # Note: By setting a different section below we split off this task into
  # a different sorting bucket. Too add extra granularity (e.g. tasks executed
  # during page loading) add logic to set a different section name here. The
  # section name is set before the slice's data is recorded so the triggering
  # event will be included in its own section (i.e. the idle trigger will be
  # recorded as an idle event).

  if task_slice.name == TaskExecutionTime.IDLE_SECTION_TRIGGER:
    section = TaskExecutionTime.IDLE_SECTION

  # Add the thread name and section (e.g. 'Idle') to the test name
  # so it is human-readable.
  reported_name = thread_name + ':'
  if section:
    reported_name += section + ':'

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

  # Use the name and section as the key to bind identical entries. The section
  # is tracked separately so results can be easily split up across section
  # boundaries (e.g. Idle Vs Not-idle) for top-10 generation even if the name
  # of the task is identical.
  dictionary_key = TaskKey(reported_name, section)
  self_duration = task_slice.self_thread_time

  if dictionary_key in task_durations:
    # Task_durations already contains an entry for this (e.g. from an earlier
    # slice), add the new duration so we can calculate a median value later on.
    task_durations[dictionary_key].Update(self_duration)
  else:
    # This is a new task so create a new entry for it.
    task_durations[dictionary_key] = SliceNameAndDurations(
        reported_name,
        self_duration)

  # Process sub slices recursively, passing the current section down.
  for sub_slice in task_slice.sub_slices:
    _ProcessTasksForThread(
        task_durations,
        thread_name,
        sub_slice,
        section)


class SliceNameAndDurations:

  def __init__(self, name, self_duration):
    self.name = name
    self.self_durations = [self_duration]

  def Update(self, self_duration):
    self.self_durations.append(self_duration)

  @property
  def median_self_duration(self):
    return statistics.Median(self.self_durations)
