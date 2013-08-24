# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from metrics import loading
from metrics import smoothness
from metrics.gpu_rendering_stats import GpuRenderingStats
from telemetry.page import page_measurement

class DidNotScrollException(page_measurement.MeasurementFailure):
  def __init__(self):
    super(DidNotScrollException, self).__init__('Page did not scroll')

class MissingDisplayFrameRate(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRate, self).__init__(
        'Missing display frame rate metrics: ' + name)

class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self.force_enable_threaded_compositing = False
    self._metrics = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected, not just FPS')

  def CustomizeBrowserOptions(self, options):
    smoothness.SmoothnessMetrics.CustomizeBrowserOptions(options)
    if self.force_enable_threaded_compositing:
      options.extra_browser_args.append('--enable-threaded-compositing')

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunAction(self, page, tab, action):
    tab.browser.StartTracing('webkit,cc,benchmark', 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()
    self._metrics = smoothness.SmoothnessMetrics(tab)
    if action.CanBeBound():
      self._metrics.BindToAction(action)
    else:
      self._metrics.Start()

  def DidRunAction(self, page, tab, action):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    if not action.CanBeBound():
      self._metrics.Stop()
    tab.browser.StopTracing()

  def FindTimelineMarker(self, timeline):
    events = [s for
              s in timeline.GetAllEventsOfName(
                  smoothness.TIMELINE_MARKER)
              if s.parent_slice == None]
    if len(events) != 1:
      raise LookupError, 'timeline marker not found'
    return events[0]

  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = self._metrics.deltas

    if not (rendering_stats_deltas['numFramesSentToScreen'] > 0):
      raise DidNotScrollException()

    loading.LoadingMetric().AddResults(tab, results)

    smoothness.CalcFirstPaintTimeResults(results, tab)

    timeline = tab.browser.GetTraceResultAndReset().AsTimelineModel()
    timeline_marker = self.FindTimelineMarker(timeline)
    benchmark_stats = GpuRenderingStats(timeline_marker,
                                        rendering_stats_deltas,
                                        self._metrics.is_using_gpu_benchmarking)
    smoothness.CalcResults(benchmark_stats, results)

    if self.options.report_all_results:
      for k, v in rendering_stats_deltas.iteritems():
        results.Add(k, '', v)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRate(r.name)
        results.Add(r.name, r.unit, r.value)
