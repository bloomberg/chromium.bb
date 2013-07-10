# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from perf_tools import smoothness_metrics
from telemetry.page import page_measurement

class StatsCollector(object):
  def __init__(self, timeline):
    """
    Utility class for collecting rendering stats from timeline model.

    timeline -- The timeline model
    """
    self.timeline = timeline
    self.total_best_rasterize_time = 0
    self.total_best_record_time = 0
    self.total_pixels_rasterized = 0
    self.total_pixels_recorded = 0

  def FindTriggerTime(self):
    measure_next_frame_event = self.timeline.GetAllEventsOfName(
        "measureNextFrame")
    if len(measure_next_frame_event) == 0:
      raise LookupError, 'no measureNextFrame event found'
    return measure_next_frame_event[0].start

  def FindFrameNumber(self, trigger_time):
    start_event = None
    for event in self.timeline.GetAllEventsOfName(
        "LayerTreeHost::UpdateLayers"):
      if event.start > trigger_time:
        if start_event == None:
          start_event = event
        elif event.start < start_event.start:
          start_event = event
    if start_event is None:
      raise LookupError, \
          'no LayterTreeHost::UpdateLayers after measureNextFrame found'
    return start_event.args["commit_number"]

  def GatherRasterizeStats(self, frame_number):
    for event in self.timeline.GetAllEventsOfName(
        "RasterWorkerPoolTaskImpl::RunRasterOnThread"):
      if event.args["data"]["source_frame_number"] == frame_number:
        best_rasterize_time = float("inf")
        for child in event.GetAllSubSlices():
          if child.name == "Picture::Raster":
            best_rasterize_time = min(best_rasterize_time, child.duration)
            if "num_pixels_rasterized" in child.args:
              self.total_pixels_rasterized += \
                  child.args["num_pixels_rasterized"]
        if best_rasterize_time == float('inf'):
          best_rasterize_time = 0
        self.total_best_rasterize_time += best_rasterize_time

  def GatherRecordStats(self, frame_number):
    for event in self.timeline.GetAllEventsOfName(
        "PicturePile::Update recording loop"):
      if event.args["commit_number"] == frame_number:
        best_record_time = float('inf')
        for child in event.GetAllSubSlices():
          if child.name == "Picture::Record":
            best_record_time = min(best_record_time, child.duration)
            self.total_pixels_recorded += \
                child.args["width"] * child.args["height"]
        if best_record_time == float('inf'):
          best_record_time = 0
        self.total_best_record_time += best_record_time

  def GatherRenderingStats(self):
    trigger_time = self.FindTriggerTime()
    frame_number = self.FindFrameNumber(trigger_time)
    self.GatherRasterizeStats(frame_number)
    self.GatherRecordStats(frame_number)

def DivideIfPossibleOrZero(numerator, denominator):
  if denominator == 0:
    return 0
  return numerator / denominator

class RasterizeAndPaintMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndPaintMeasurement, self).__init__('', True)
    self._metrics = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.append('--enable-gpu-benchmarking')
    # Run each raster task 100 times. This allows us to report the time for the
    # best run, effectively excluding cache effects and time when the thread is
    # de-scheduled.
    options.extra_browser_args.append('--slow-down-raster-scale-factor=100')
    # Enable impl-side-painting. Current version of benchmark only works for
    # this mode.
    options.extra_browser_args.append('--enable-impl-side-painting')
    options.extra_browser_args.append('--force-compositing-mode')
    options.extra_browser_args.append('--enable-threaded-compositing')

  def MeasurePage(self, page, tab, results):
    self._metrics = smoothness_metrics.SmoothnessMetrics(tab)

    # Rasterize only what's visible
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    # Wait until the page has loaded and come to a somewhat steady state
    # Empirical wait time for workstation. May need to adjust for other devices
    time.sleep(5)

    # Render one frame before we start gathering a trace. On some pages, the
    # first frame requested has more variance in the number of pixels
    # rasterized.
    tab.ExecuteJavaScript("""
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          window.__rafFired  = true;
        });
    """)

    tab.browser.StartTracing('webkit,cc', 60)
    self._metrics.Start()

    tab.ExecuteJavaScript("""
        console.time("measureNextFrame");
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          window.__rafFired  = true;
        });
    """)
    # Wait until the frame was drawn
    # Empirical wait time for workstation. May need to adjust for other devices
    # TODO(ernstm): replace by call-back.
    time.sleep(5)
    tab.ExecuteJavaScript('console.timeEnd("measureNextFrame")')

    tab.browser.StopTracing()
    self._metrics.Stop()

    timeline = tab.browser.GetTraceResultAndReset().AsTimelineModel()
    collector = StatsCollector(timeline)
    collector.GatherRenderingStats()

    rendering_stats = self._metrics.end_values

    results.Add('best_rasterize_time', 'seconds',
                collector.total_best_rasterize_time / 1.e3,
                data_type='unimportant')
    results.Add('best_record_time', 'seconds',
                collector.total_best_record_time / 1.e3,
                data_type='unimportant')
    results.Add('total_pixels_rasterized', 'pixels',
                collector.total_pixels_rasterized,
                data_type='unimportant')
    results.Add('total_pixels_recorded', 'pixels',
                collector.total_pixels_recorded,
                data_type='unimportant')

    if self.options.report_all_results:
      for k, v in rendering_stats.iteritems():
        results.Add(k, '', v)
