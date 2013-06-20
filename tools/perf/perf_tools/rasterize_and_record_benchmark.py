# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import json
import StringIO

from perf_tools import smoothness_metrics
from telemetry.page import page_measurement

#TODO(ernstm,etc): remove this when crbug.com/173327
#(timeline model in telemetry) is done.
class StatsCollector(object):
  def __init__(self, events):
    """
    Utility class for collecting rendering stats from traces.

    events -- An iterable object containing trace events
    """
    if not hasattr(events, '__iter__'):
      raise Exception, 'events must be iteraable.'
    self.events = events
    self.pids = None
    self.tids = None
    self.index = 0
    self.total_best_rasterize_time = 0
    self.total_best_record_time = 0
    self.total_pixels_rasterized = 0
    self.total_pixels_recorded = 0

  def __len__(self):
    return len(self.events)

  def __getitem__(self, i):
    return self.events[i]

  def __setitem__(self, i, v):
    self.events[i] = v

  def __repr__(self):
    return "[%s]" % ",\n ".join([repr(e) for e in self.events])

  def seekToNextEvent(self):
    event = self.events[self.index]
    self.index += 1
    return event

  def gatherRasterStats(self):
    best_rasterize_time = float('inf')
    pixels_rasterized = 0
    while self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "RasterWorkerPoolTaskImpl::RunRasterOnThread":
        break
      elif event["name"] == "Picture::Raster":
        if event["ph"] == "B":
          rasterize_time_start = event["ts"]
        else:
          rasterize_duration = event["ts"] - rasterize_time_start
          best_rasterize_time = min(best_rasterize_time, rasterize_duration)
          pixels_rasterized += event["args"]["num_pixels_rasterized"]
    if best_rasterize_time == float('inf'):
      best_rasterize_time = 0
    return best_rasterize_time, pixels_rasterized

  def gatherRecordStats(self):
    best_record_time = float('inf')
    pixels_recorded = 0
    while self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "PicturePile::Update recording loop":
        break
      elif event["name"] == "Picture::Record":
        if event["ph"] == "B":
          record_time_start = event["ts"]
          width = event["args"]["width"]
          height = event["args"]["height"]
          pixels_recorded += width*height
        else:
          record_duration = event["ts"] - record_time_start
          best_record_time = min(best_record_time, record_duration)
    if best_record_time == float('inf'):
      best_record_time = 0
    return best_record_time, pixels_recorded

  def seekToMeasureNextFrameEvent(self):
    while self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "measureNextFrame":
        break

  def seekToStartEvent(self):
    while self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "LayerTreeHost::UpdateLayers":
        frame_number = event["args"]["commit_number"]
        return frame_number

  def seekToStopEvent(self):
    while self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "PendingTree" and event["ph"] == "F":
        break

  def gatherRenderingStats(self):
    self.seekToMeasureNextFrameEvent()
    frame_number = self.seekToStartEvent()
    start_event_index = self.index
    self.seekToStopEvent()
    stop_event_index = self.index
    self.index = start_event_index

    while self.index < stop_event_index and self.index < len(self.events):
      event = self.seekToNextEvent()
      if event["name"] == "RasterWorkerPoolTaskImpl::RunRasterOnThread" \
           and event["ph"] == "B":
        source_frame_number = event["args"]["metadata"]["source_frame_number"]
        if source_frame_number == frame_number:
          best_rasterize_time, pixels_rasterized = self.gatherRasterStats()
          self.total_best_rasterize_time += best_rasterize_time
          self.total_pixels_rasterized += pixels_rasterized
      elif event["name"] == "PicturePile::Update recording loop" \
           and event ["ph"] == "B":
        if self.index < stop_event_index:
          best_record_time, pixels_recorded = self.gatherRecordStats()
          self.total_best_record_time += best_record_time
          self.total_pixels_recorded += pixels_recorded

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

    tab.browser.StartTracing('webkit,cc')
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

    stream = StringIO.StringIO()
    tab.browser.GetTraceResultAndReset().Serialize(stream)
    events = json.loads(stream.getvalue())
    if 'traceEvents' in events:
      events = events['traceEvents']
    collector = StatsCollector(events)
    collector.gatherRenderingStats()

    rendering_stats = self._metrics.end_values

    results.Add('best_rasterize_time', 'seconds',
                collector.total_best_rasterize_time / 1000000.0,
                data_type='unimportant')
    results.Add('best_record_time', 'seconds',
                collector.total_best_record_time / 1000000.0,
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
