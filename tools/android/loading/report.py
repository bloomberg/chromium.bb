# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a loading report.

When executed as a script, takes a trace filename and print the report.
"""

from activity_lens import ActivityLens
from content_classification_lens import ContentClassificationLens
from loading_graph_view import LoadingGraphView
import loading_trace
import metrics
from network_activity_lens import NetworkActivityLens
import prefetch_view
import request_dependencies_lens
from user_satisfied_lens import (
    FirstTextPaintLens, FirstContentfulPaintLens, FirstSignificantPaintLens)


def _ComputeCpuBusyness(activity, load_start, satisfied_end):
  """Generates a breakdown of CPU activity between |load_start| and
  |satisfied_end|."""
  duration = float(satisfied_end - load_start)
  result = {
      'activity_frac': (
          activity.MainRendererThreadBusyness(load_start, satisfied_end)
          / duration),
  }

  activity_breakdown = activity.ComputeActivity(load_start, satisfied_end)
  result['parsing_frac'] = (
      sum(activity_breakdown['parsing'].values()) / duration)
  result['script_frac'] = (
      sum(activity_breakdown['script'].values()) / duration)
  return result


class PerUserLensReport(object):
  """Generates a variety of metrics relative to a passed in user lens."""

  def __init__(self, trace, user_lens, activity_lens, network_lens,
               navigation_start_msec, preloaded_requests):
    self._navigation_start_msec = navigation_start_msec

    self._satisfied_msec = user_lens.SatisfiedMs()

    graph = LoadingGraphView.FromTrace(trace)
    self._inversions = graph.GetInversionsAtTime(self._satisfied_msec)

    self._byte_frac = self._GenerateByteFrac(network_lens)

    self._requests = len(user_lens.CriticalRequests())
    self._preloaded_requests = len(set(preloaded_requests) &
                                   set(user_lens.CriticalRequests()))

    self._cpu_busyness = _ComputeCpuBusyness(activity_lens,
                                             navigation_start_msec,
                                             self._satisfied_msec)

  def GenerateReport(self):
    report = {}

    report['ms'] = self._satisfied_msec - self._navigation_start_msec
    report['byte_frac'] = self._byte_frac
    report['requests'] = self._requests
    report['preloaded_requests'] = self._preloaded_requests

    # Take the first (earliest) inversion.
    report['inversion'] = self._inversions[0].url if self._inversions else None

    report.update(self._cpu_busyness)
    return report

  def _GenerateByteFrac(self, network_lens):
    if not network_lens.total_download_bytes:
      return float('Nan')
    byte_frac = (network_lens.DownloadedBytesAt(self._satisfied_msec)
          / float(network_lens.total_download_bytes))
    return byte_frac


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

    navigation_start_events = trace.tracing_track.GetMatchingEvents(
        'blink.user_timing', 'navigationStart')
    self._navigation_start_msec = min(
        e.start_msec for e in navigation_start_events)
    self._load_end_msec = self._ComputePlt(trace)

    dependencies_lens = request_dependencies_lens.RequestDependencyLens(
        self.trace)
    requests = self.trace.request_track.GetEvents()
    preloaded_requests = \
       prefetch_view.PrefetchSimulationView.PreloadedRequests(
           requests[0], dependencies_lens, self.trace)
    self._requests = len(requests)
    self._preloaded_requests = len(preloaded_requests)
    self._dns_requests, self._dns_cost_msec = metrics.DnsRequestsAndCost(trace)

    self._user_lens_reports = {}
    first_text_paint_lens = FirstTextPaintLens(self.trace)
    first_contentful_paint_lens = FirstContentfulPaintLens(self.trace)
    first_significant_paint_lens = FirstSignificantPaintLens(self.trace)
    activity = ActivityLens(trace)
    network_lens = NetworkActivityLens(self.trace)
    for key, user_lens in [['first_text', first_text_paint_lens],
                           ['contentful', first_contentful_paint_lens],
                           ['significant', first_significant_paint_lens]]:
      self._user_lens_reports[key] = PerUserLensReport(self.trace,
          user_lens, activity, network_lens, self._navigation_start_msec,
          preloaded_requests)

    self._transfer_size = metrics.TotalTransferSize(trace)[1]

    self._cpu_busyness = _ComputeCpuBusyness(activity,
                                             self._navigation_start_msec,
                                             self._load_end_msec)

    content_lens = ContentClassificationLens(
        trace, ad_rules or [], tracking_rules or [])
    has_ad_rules = bool(ad_rules)
    has_tracking_rules = bool(tracking_rules)
    self._ad_report = self._AdRequestsReport(
        trace, content_lens, has_ad_rules, has_tracking_rules)
    self._ads_cost = self._AdsAndTrackingCpuCost(
        self._navigation_start_msec, self._load_end_msec, content_lens,
        activity, has_tracking_rules or has_ad_rules)

  def GenerateReport(self):
    """Returns a report as a dict."""
    # NOTE: When changing the return value here, also update the schema
    # (bigquery_schema.json) accordingly. See cloud/frontend/README.md for
    # details.
    report = {
        'url': self.trace.url,
        'plt_ms': self._load_end_msec - self._navigation_start_msec,
        'requests': self._requests,
        'preloaded_requests': self._preloaded_requests,
        'transfer_size': self._transfer_size,
        'dns_requests': self._dns_requests,
        'dns_cost_ms': self._dns_cost_msec}

    for user_lens_type, user_lens_report in self._user_lens_reports.iteritems():
      for key, value in user_lens_report.GenerateReport().iteritems():
        report[user_lens_type + '_' + key] = value

    report.update(self._cpu_busyness)
    report.update(self._ad_report)
    report.update(self._ads_cost)
    return report

  @classmethod
  def FromTraceFilename(cls, filename, ad_rules_filename,
                        tracking_rules_filename):
    """Returns a LoadingReport from a trace filename."""
    trace = loading_trace.LoadingTrace.FromJsonFile(filename)
    return LoadingReport(trace, ad_rules_filename, tracking_rules_filename)

  @classmethod
  def _AdRequestsReport(
      cls, trace, content_lens, has_ad_rules, has_tracking_rules):
    requests = trace.request_track.GetEvents()
    has_rules = has_ad_rules or has_tracking_rules
    result = {
        'ad_requests': 0 if has_ad_rules else None,
        'tracking_requests': 0 if has_tracking_rules else None,
        'ad_or_tracking_requests': 0 if has_rules else None,
        'ad_or_tracking_initiated_requests': 0 if has_rules else None,
        'ad_or_tracking_initiated_transfer_size': 0 if has_rules else None}
    if not has_rules:
      return result
    for request in requests:
      is_ad = content_lens.IsAdRequest(request)
      is_tracking = content_lens.IsTrackingRequest(request)
      if has_ad_rules:
        result['ad_requests'] += int(is_ad)
      if has_tracking_rules:
        result['tracking_requests'] += int(is_tracking)
      result['ad_or_tracking_requests'] += int(is_ad or is_tracking)
    ad_tracking_requests = content_lens.AdAndTrackingRequests()
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

  @classmethod
  def _AdsAndTrackingCpuCost(
      cls, start_msec, end_msec, content_lens, activity, has_rules):
    """Returns the CPU cost associated with Ads and tracking between timestamps.

    Can return an overestimate, as execution slices are tagged by URL, and not
    by requests.

    Args:
      start_msec: (float)
      end_msec: (float)
      content_lens: (ContentClassificationLens)
      activity: (ActivityLens)

    Returns:
      {'ad_and_tracking_script_frac': float,
       'ad_and_tracking_parsing_frac': float}
    """
    result = {'ad_or_tracking_script_frac': None,
              'ad_or_tracking_parsing_frac': None}
    if not has_rules:
      return result

    duration = float(end_msec - start_msec)
    requests = content_lens.AdAndTrackingRequests()
    urls = {r.url for r in requests}
    cpu_breakdown = activity.ComputeActivity(start_msec, end_msec)
    result['ad_or_tracking_script_frac'] = sum(
            value for (url, value) in cpu_breakdown['script'].items()
            if url in urls) / duration
    result['ad_or_tracking_parsing_frac'] = sum(
            value for (url, value) in cpu_breakdown['parsing'].items()
            if url in urls) / duration
    return result


def _Main(args):
  if len(args) not in (2, 4):
    print 'Usage: report.py trace.json (ad_rules tracking_rules)'
    sys.exit(1)
  trace_filename = args[1]
  ad_rules = None
  tracking_rules = None
  if len(args) == 4:
    ad_rules = open(args[2]).readlines()
    tracking_rules = open(args[3]).readlines()
  report = LoadingReport.FromTraceFilename(
      trace_filename, ad_rules, tracking_rules)
  print json.dumps(report.GenerateReport(), indent=2, sort_keys=True)


if __name__ == '__main__':
  import sys
  import json

  _Main(sys.argv)
