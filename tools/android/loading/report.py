# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a loading report.

When executed as a script, takes a trace filename and print the report.
"""

from loading_graph_view import LoadingGraphView
import loading_trace
from network_activity_lens import NetworkActivityLens
from user_satisfied_lens import (
    FirstTextPaintLens, FirstContentfulPaintLens, FirstSignificantPaintLens)


class LoadingReport(object):
  """Generates a loading report from a loading trace."""
  def __init__(self, trace):
    """Constructor.

    Args:
      trace: (LoadingTrace) a loading trace.
    """
    self.trace = trace
    self._text_msec = FirstTextPaintLens(self.trace).SatisfiedMs()
    self._contentful_paint_msec = (
        FirstContentfulPaintLens(self.trace).SatisfiedMs())
    self._significant_paint_msec = (
        FirstSignificantPaintLens(self.trace).SatisfiedMs())
    navigation_start_events = trace.tracing_track.GetMatchingEvents(
        'blink.user_timing', 'navigationStart')
    self._navigation_start_msec = min(
        e.start_msec for e in navigation_start_events)
    # TODO(lizeb): This is not PLT. Should correlate with
    # RenderFrameImpl::didStopLoading.
    self._max_msec = max(
        r.end_msec or -1 for r in self.trace.request_track.GetEvents())

    network_lens = NetworkActivityLens(self.trace)
    if network_lens.total_download_bytes > 0:
      self._contentful_byte_frac = (
          network_lens.DownloadedBytesAt(self._contentful_paint_msec)
          / float(network_lens.total_download_bytes))
      self._significant_byte_frac = (
          network_lens.DownloadedBytesAt(self._significant_paint_msec)
          / float(network_lens.total_download_bytes))
    else:
      self._contentful_byte_frac = float('Nan')
      self._significant_byte_frac = float('Nan')

    graph = LoadingGraphView.FromTrace(trace)
    self._contentful_inversion = graph.GetInversionsAtTime(
        self._contentful_paint_msec)
    self._significant_inversion = graph.GetInversionsAtTime(
        self._significant_paint_msec)

  def GenerateReport(self):
    """Returns a report as a dict."""
    return {
        'url': self.trace.url,
        'first_text_ms': self._text_msec - self._navigation_start_msec,
        'contentful_paint_ms': (self._contentful_paint_msec
                                - self._navigation_start_msec),
        'significant_paint_ms': (self._significant_paint_msec
                                 - self._navigation_start_msec),
        'plt_ms': self._max_msec - self._navigation_start_msec,
        'contentful_byte_frac': self._contentful_byte_frac,
        'significant_byte_frac': self._significant_byte_frac,

        # Take the first (earliest) inversions.
        'contentful_inversion': (self._contentful_inversion[0].url
                                 if self._contentful_inversion
                                 else None),
        'significant_inversion': (self._significant_inversion[0].url
                                  if self._significant_inversion
                                  else None)
    }

  @classmethod
  def FromTraceFilename(cls, filename):
    """Returns a LoadingReport from a trace filename."""
    trace = loading_trace.LoadingTrace.FromJsonFile(filename)
    return LoadingReport(trace)


if __name__ == '__main__':
  import sys
  import json

  trace_filename = sys.argv[1]
  print json.dumps(
      LoadingReport.FromTraceFilename(trace_filename).GenerateReport(),
      indent=2)
