# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a loading report.

When executed as a script, takes a trace filename and print the report.
"""

from content_classification_lens import ContentClassificationLens
from loading_graph_view import LoadingGraphView
import loading_trace
import metrics
from network_activity_lens import NetworkActivityLens
from user_satisfied_lens import (
    FirstTextPaintLens, FirstContentfulPaintLens, FirstSignificantPaintLens)


class LoadingReport(object):
  """Generates a loading report from a loading trace."""
  def __init__(self, trace, ad_rules=None, tracking_rules=None):
    """Constructor.

    Args:
      trace: (LoadingTrace) a loading trace.
      ad_rules: ([str]) List of ad filtering rules.
      tracking_rules: ([str]) List of tracking filtering rules.
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
    self._load_end_msec = self._ComputePlt(trace)
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
    self._ad_report = self._AdRequestsReport(
        trace, ad_rules or [], tracking_rules or [])

    graph = LoadingGraphView.FromTrace(trace)
    self._contentful_inversion = graph.GetInversionsAtTime(
        self._contentful_paint_msec)
    self._significant_inversion = graph.GetInversionsAtTime(
        self._significant_paint_msec)
    self._transfer_size = metrics.TotalTransferSize(trace)[1]

  def GenerateReport(self):
    """Returns a report as a dict."""
    report = {
        'url': self.trace.url,
        'first_text_ms': self._text_msec - self._navigation_start_msec,
        'contentful_paint_ms': (self._contentful_paint_msec
                                - self._navigation_start_msec),
        'significant_paint_ms': (self._significant_paint_msec
                                 - self._navigation_start_msec),
        'plt_ms': self._load_end_msec - self._navigation_start_msec,
        'contentful_byte_frac': self._contentful_byte_frac,
        'significant_byte_frac': self._significant_byte_frac,

        # Take the first (earliest) inversions.
        'contentful_inversion': (self._contentful_inversion[0].url
                                 if self._contentful_inversion
                                 else None),
        'significant_inversion': (self._significant_inversion[0].url
                                  if self._significant_inversion
                                  else None),
        'transfer_size': self._transfer_size}
    report.update(self._ad_report)
    return report

  @classmethod
  def FromTraceFilename(cls, filename, ad_rules_filename,
                        tracking_rules_filename):
    """Returns a LoadingReport from a trace filename."""
    trace = loading_trace.LoadingTrace.FromJsonFile(filename)
    return LoadingReport(trace, ad_rules_filename, tracking_rules_filename)

  @classmethod
  def _AdRequestsReport(cls, trace, ad_rules, tracking_rules):
    has_rules = bool(ad_rules) or bool(tracking_rules)
    requests = trace.request_track.GetEvents()
    result = {
        'request_count': len(requests),
        'ad_requests': 0 if ad_rules else None,
        'tracking_requests': 0 if tracking_rules else None,
        'ad_or_tracking_requests': 0 if has_rules else None,
        'ad_or_tracking_initiated_requests': 0 if has_rules else None,
        'ad_or_tracking_initiated_transfer_size': 0 if has_rules else None}
    content_classification_lens = ContentClassificationLens(
        trace, ad_rules, tracking_rules)
    if not has_rules:
      return result
    for request in trace.request_track.GetEvents():
      is_ad = content_classification_lens.IsAdRequest(request)
      is_tracking = content_classification_lens.IsTrackingRequest(request)
      if ad_rules:
        result['ad_requests'] += int(is_ad)
      if tracking_rules:
        result['tracking_requests'] += int(is_tracking)
      result['ad_or_tracking_requests'] += int(is_ad or is_tracking)
    ad_tracking_requests = content_classification_lens.AdAndTrackingRequests()
    result['ad_or_tracking_initiated_requests'] = len(ad_tracking_requests)
    result['ad_or_tracking_initiated_transfer_size'] = metrics.TransferSize(
        ad_tracking_requests)[1]
    return result

  @classmethod
  def _ComputePlt(cls, trace):
    mark_load_events = trace.tracing_track.GetMatchingEvents(
        'devtools.timeline', 'MarkLoad')
    # Some traces contain several load events for the main frame.
    main_frame_load_events = filter(
        lambda e: e.args['data']['isMainFrame'], mark_load_events)
    if main_frame_load_events:
      return max(e.start_msec for e in main_frame_load_events)
    # Main frame onLoad() didn't finish. Take the end of the last completed
    # request.
    return max(r.end_msec or -1 for r in trace.request_track.GetEvents())


def _Main(args):
  assert len(args) == 4, 'Usage: report.py trace.json ad_rules tracking_rules'
  trace_filename = args[1]
  ad_rules = open(args[2]).readlines()
  tracking_rules = open(args[3]).readlines()
  report = LoadingReport.FromTraceFilename(
      trace_filename, ad_rules, tracking_rules)
  print json.dumps(report.GenerateReport(), indent=2)


if __name__ == '__main__':
  import sys
  import json

  _Main(sys.argv)
