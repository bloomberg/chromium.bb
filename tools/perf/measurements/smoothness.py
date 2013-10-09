# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from metrics import smoothness
from metrics.rendering_stats import RenderingStats
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
    self._metrics = None
    self._trace_result = None

  def CustomizeBrowserOptions(self, options):
    smoothness.SmoothnessMetrics.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunAction(self, page, tab, action):
    # TODO(ermst): Remove "webkit" category after Blink r157377 is picked up by
    # the reference builds.
    tab.browser.StartTracing('webkit,webkit.console,benchmark', 60)
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
    self._trace_result = tab.browser.StopTracing()

  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = self._metrics.deltas

    # TODO(ernstm): remove numFramesSentToScreen when RenderingStats
    # cleanup CL was picked up by the reference build.
    if 'frameCount' in rendering_stats_deltas:
      frame_count = rendering_stats_deltas.get('frameCount', 0)
    else:
      frame_count = rendering_stats_deltas.get('numFramesSentToScreen', 0)

    if not (frame_count > 0):
      raise DidNotScrollException()

    timeline = self._trace_result.AsTimelineModel()
    smoothness_marker = smoothness.FindTimelineMarker(timeline,
        smoothness.TIMELINE_MARKER)
    # TODO(dominikg): remove try..except once CL 23532057 has been rolled to the
    # reference builds?
    try:
      gesture_marker = smoothness.FindTimelineMarker(timeline,
          smoothness.SYNTHETIC_GESTURE_MARKER)
    except smoothness.MissingTimelineMarker:
      logging.warning(
        'No gesture marker found in timeline; using smoothness marker instead.')
      gesture_marker = smoothness_marker
    benchmark_stats = RenderingStats(smoothness_marker,
                                     gesture_marker,
                                     rendering_stats_deltas,
                                     self._metrics.is_using_gpu_benchmarking)
    smoothness.CalcResults(benchmark_stats, results)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRate(r.name)
        results.Add(r.name, r.unit, r.value)
