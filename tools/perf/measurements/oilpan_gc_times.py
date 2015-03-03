# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from measurements import smoothness_controller
from measurements import timeline_controller
from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.page import page_test
from telemetry.page.actions import action_runner
from telemetry.timeline.model import TimelineModel
from telemetry.util import statistics
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar
from telemetry.value import trace


_CR_RENDERER_MAIN = 'CrRendererMain'
_RUN_SMOOTH_ACTIONS = 'RunSmoothAllActions'
_NAMES_TO_DUMP = ['oilpan_precise_mark',
                  'oilpan_precise_lazy_sweep',
                  'oilpan_precise_complete_sweep',
                  'oilpan_conservative_mark',
                  'oilpan_conservative_lazy_sweep',
                  'oilpan_conservative_complete_sweep',
                  'oilpan_coalesce']

def _AddTracingResults(events, results):
  # Prepare
  values = {}
  for name in _NAMES_TO_DUMP:
    values[name] = []

  # Parse in time line
  gc_type = None
  forced = False
  mark_time = 0
  lazy_sweep_time = 0
  complete_sweep_time = 0
  for event in events:
    duration = event.thread_duration or event.duration
    if event.name == 'ThreadHeap::coalesce':
      values['oilpan_coalesce'].append(duration)
      continue
    if event.name == 'Heap::collectGarbage':
      if not gc_type is None and not forced:
        values['oilpan_%s_mark' % gc_type].append(mark_time)
        values['oilpan_%s_lazy_sweep' % gc_type].append(lazy_sweep_time)
        values['oilpan_%s_complete_sweep' % gc_type].append(complete_sweep_time)

      gc_type = 'precise' if event.args['precise'] else 'conservative'
      forced = event.args['forced']
      mark_time = duration
      lazy_sweep_time = 0
      complete_sweep_time = 0
      continue
    if event.name == 'ThreadHeap::lazySweepPages':
      lazy_sweep_time += duration
      continue
    if event.name == 'ThreadState::completeSweep':
      complete_sweep_time += duration
      continue

  if not gc_type is None and not forced:
    values['oilpan_%s_mark' % gc_type].append(mark_time)
    values['oilpan_%s_lazy_sweep' % gc_type].append(lazy_sweep_time)
    values['oilpan_%s_complete_sweep' % gc_type].append(complete_sweep_time)

  # Dump
  page = results.current_page
  unit = 'ms'
  for name in _NAMES_TO_DUMP:
    if values[name]:
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          page, name, unit, values[name]))
      results.AddValue(scalar.ScalarValue(
          page, name + '_max', unit, max(values[name])))
    results.AddValue(scalar.ScalarValue(
        page, name + '_total', unit, sum(values[name])))

  # Summarize marking time
  total_mark_time = 0
  for gc_type in ['precise', 'conservative']:
    total_mark_time += sum(values['oilpan_%s_mark' % gc_type])
  results.AddValue(
      scalar.ScalarValue(page, 'oilpan_mark', unit, total_mark_time))

  # Summarize sweeping time
  total_sweep_time = 0
  for do_type in ['lazy_sweep', 'complete_sweep']:
    sweep_time = 0
    for gc_type in ['precise', 'conservative']:
      sweep_time += sum(values['oilpan_%s_%s' % (gc_type, do_type)])
    key = 'oilpan_%s' % do_type
    results.AddValue(scalar.ScalarValue(page, key, unit, sweep_time))
    total_sweep_time += sweep_time
  results.AddValue(
      scalar.ScalarValue(page, 'oilpan_sweep', unit, total_sweep_time))

  gc_time = 0
  for key in values:
    gc_time += sum(values[key])
  results.AddValue(scalar.ScalarValue(page, 'oilpan_gc', unit, gc_time))


class _OilpanGCTimesBase(page_test.PageTest):
  def __init__(self, action_name = ''):
    super(_OilpanGCTimesBase, self).__init__(action_name)
    self._timeline_model = None

  def WillNavigateToPage(self, page, tab):
    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    categories = ['webkit.console', 'blink.console', 'blink_gc']
    category_filter = tracing_category_filter.TracingCategoryFilter()
    for c in categories:
      category_filter.AddIncludedCategory(c)
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.Start(options, category_filter,
                                                  timeout=1000)

  def DidRunActions(self, page, tab):
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    self._timeline_model = TimelineModel(timeline_data)

  def ValidateAndMeasurePage(self, page, tab, results):
    threads = self._timeline_model.GetAllThreads()
    for thread in threads:
      if thread.name == _CR_RENDERER_MAIN:
        _AddTracingResults(thread.all_slices, results)

  def CleanUpAfterPage(self, page, tab):
    if tab.browser.platform.tracing_controller.is_tracing_running:
      tab.browser.platform.tracing_controller.Stop()


class OilpanGCTimesForSmoothness(_OilpanGCTimesBase):
  def __init__(self):
    super(OilpanGCTimesForSmoothness, self).__init__('RunPageInteractions')
    self._interaction = None

  def WillRunActions(self, page, tab):
    runner = action_runner.ActionRunner(tab)
    self._interaction = runner.BeginInteraction(_RUN_SMOOTH_ACTIONS,
                                                is_smooth=True)

  def DidRunActions(self, page, tab):
    self._interaction.End()
    super(OilpanGCTimesForSmoothness, self).DidRunActions(page, tab)


class OilpanGCTimesForBlinkPerf(_OilpanGCTimesBase):
  def __init__(self):
    super(OilpanGCTimesForBlinkPerf, self).__init__()
    with open(os.path.join(os.path.dirname(__file__), '..', 'benchmarks',
                           'blink_perf.js'), 'r') as f:
      self._blink_perf_js = f.read()

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._blink_perf_js
    super(OilpanGCTimesForBlinkPerf, self).WillNavigateToPage(page, tab)

  def DidRunActions(self, page, tab):
    tab.WaitForJavaScriptExpression('testRunner.isDone', 600)
    super(OilpanGCTimesForBlinkPerf, self).DidRunActions(page, tab)


class OilpanGCTimesForInternals(_OilpanGCTimesBase):
  def __init__(self):
    super(OilpanGCTimesForInternals, self).__init__()

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    # 'expose-internals-for-testing' can be enabled on content shell.
    assert 'content-shell' in options.browser_type
    options.AppendExtraBrowserArgs(['--expose-internals-for-testing',
                                    '--js-flags=--expose-gc'])
