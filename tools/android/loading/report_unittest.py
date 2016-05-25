# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import metrics
import report
import test_utils
import user_satisfied_lens_unittest


class LoadingReportTestCase(unittest.TestCase):
  MILLI_TO_MICRO = 1000
  _NAVIGATION_START_TIME = 12
  _FIRST_REQUEST_TIME = 15
  _CONTENTFUL_PAINT = 120
  _TEXT_PAINT = 30
  _SIGNIFICANT_PAINT = 50
  _DURATION = 400
  _REQUEST_OFFSET = 5
  _LOAD_END_TIME = 1280
  _MAIN_FRAME_ID = 1
  _FIRST_REQUEST_DATA_LENGTH = 128
  _SECOND_REQUEST_DATA_LENGTH = 1024
  _TOPLEVEL_EVENT_OFFSET = 10
  _TOPLEVEL_EVENT_DURATION = 100
  _SCRIPT_EVENT_DURATION = 50
  _PARSING_EVENT_DURATION = 60

  def setUp(self):
    self.trace_creator = test_utils.TraceCreator()
    self.requests = [
        self.trace_creator.RequestAt(self._FIRST_REQUEST_TIME, frame_id=1),
        self.trace_creator.RequestAt(
            self._NAVIGATION_START_TIME + self._REQUEST_OFFSET, self._DURATION)]
    self.requests[0].timing.receive_headers_end = 0
    self.requests[1].timing.receive_headers_end = 0
    self.requests[0].encoded_data_length = self._FIRST_REQUEST_DATA_LENGTH
    self.requests[1].encoded_data_length = self._SECOND_REQUEST_DATA_LENGTH

    self.trace_events = [
        {'args': {'name': 'CrRendererMain'}, 'cat': '__metadata',
         'name': 'thread_name', 'ph': 'M', 'pid': 1, 'tid': 1, 'ts': 0},
        {'ts': self._NAVIGATION_START_TIME * self.MILLI_TO_MICRO, 'ph': 'R',
         'cat': 'blink.user_timing', 'pid': 1, 'tid': 1,
         'name': 'navigationStart',
         'args': {'frame': 1}},
        {'ts': self._LOAD_END_TIME * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'devtools.timeline', 'pid': 1, 'tid': 1,
         'name': 'MarkLoad',
         'args': {'data': {'isMainFrame': True}}},
        {'ts': self._CONTENTFUL_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink.user_timing', 'pid': 1, 'tid': 1,
         'name': 'firstContentfulPaint',
         'args': {'frame': self._MAIN_FRAME_ID}},
        {'ts': self._TEXT_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink.user_timing', 'pid': 1, 'tid': 1,
         'name': 'firstPaint',
         'args': {'frame': self._MAIN_FRAME_ID}},
        {'ts': 90 * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink', 'pid': 1, 'tid': 1,
         'name': 'FrameView::synchronizedPaint'},
        {'ts': self._SIGNIFICANT_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'foobar', 'name': 'biz', 'pid': 1, 'tid': 1,
         'args': {'counters': {
             'LayoutObjectsThatHadNeverHadLayout': 10}}},
        {'ts': (self._NAVIGATION_START_TIME - self._TOPLEVEL_EVENT_OFFSET)
         * self.MILLI_TO_MICRO,
         'pid': 1, 'tid': 1, 'ph': 'X',
         'dur': self._TOPLEVEL_EVENT_DURATION * self.MILLI_TO_MICRO,
         'cat': 'toplevel', 'name': 'MessageLoop::RunTask'},
        {'ts': self._NAVIGATION_START_TIME * self.MILLI_TO_MICRO,
         'pid': 1, 'tid': 1, 'ph': 'X',
         'dur': self._PARSING_EVENT_DURATION * self.MILLI_TO_MICRO,
         'cat': 'devtools.timeline', 'name': 'ParseHTML',
         'args': {'beginData': {'url': ''}}},
        {'ts': self._NAVIGATION_START_TIME * self.MILLI_TO_MICRO,
         'pid': 1, 'tid': 1, 'ph': 'X',
         'dur': self._SCRIPT_EVENT_DURATION * self.MILLI_TO_MICRO,
         'cat': 'devtools.timeline', 'name': 'EvaluateScript',
         'args': {'data': {'scriptName': ''}}}]

  def _MakeTrace(self):
    trace = self.trace_creator.CreateTrace(
        self.requests, self.trace_events, self._MAIN_FRAME_ID)
    return trace

  def testGenerateReport(self):
    trace = self._MakeTrace()
    loading_report = report.LoadingReport(trace).GenerateReport()
    self.assertEqual(trace.url, loading_report['url'])
    self.assertEqual(self._TEXT_PAINT - self._NAVIGATION_START_TIME,
                     loading_report['first_text_ms'])
    self.assertEqual(self._SIGNIFICANT_PAINT - self._NAVIGATION_START_TIME,
                     loading_report['significant_paint_ms'])
    self.assertEqual(self._CONTENTFUL_PAINT - self._NAVIGATION_START_TIME,
                     loading_report['contentful_paint_ms'])
    self.assertAlmostEqual(self._LOAD_END_TIME - self._NAVIGATION_START_TIME,
                           loading_report['plt_ms'])
    self.assertAlmostEqual(0.34, loading_report['contentful_byte_frac'], 2)
    self.assertAlmostEqual(0.1844, loading_report['significant_byte_frac'], 2)
    self.assertIsNone(loading_report['contentful_inversion'])
    self.assertIsNone(loading_report['significant_inversion'])
    self.assertIsNone(loading_report['ad_requests'])
    self.assertIsNone(loading_report['ad_or_tracking_requests'])
    self.assertIsNone(loading_report['ad_or_tracking_initiated_requests'])
    self.assertIsNone(loading_report['ad_or_tracking_initiated_transfer_size'])
    self.assertEqual(
        self._FIRST_REQUEST_DATA_LENGTH + self._SECOND_REQUEST_DATA_LENGTH
        + metrics.HTTP_OK_LENGTH * 2,
        loading_report['transfer_size'])

  def testInversion(self):
    self.requests[0].timing.loading_finished = 4 * (
        self._REQUEST_OFFSET + self._DURATION)
    self.requests[1].initiator['type'] = 'parser'
    self.requests[1].initiator['url'] = self.requests[0].url
    for e in self.trace_events:
      if e['name'] == 'firstContentfulPaint':
        e['ts'] = self.MILLI_TO_MICRO * (
            self._FIRST_REQUEST_TIME +  self._REQUEST_OFFSET +
            self._DURATION + 1)
        break
    loading_report = report.LoadingReport(self._MakeTrace()).GenerateReport()
    self.assertEqual(self.requests[0].url,
                     loading_report['contentful_inversion'])
    self.assertEqual(None, loading_report['significant_inversion'])

  def testPltNoLoadEvents(self):
    trace = self._MakeTrace()
    # Change the MarkLoad events.
    for e in trace.tracing_track.GetEvents():
      if e.name == 'MarkLoad':
        e.tracing_event['name'] = 'dummy'
    loading_report = report.LoadingReport(trace).GenerateReport()
    self.assertAlmostEqual(self._REQUEST_OFFSET + self._DURATION,
                           loading_report['plt_ms'])

  def testAdTrackingRules(self):
    ad_domain = 'i-ve-got-the-best-ads.com'
    self.requests[0].url = 'http://www.' + ad_domain
    trace = self._MakeTrace()
    loading_report = report.LoadingReport(
        trace, [ad_domain], []).GenerateReport()
    self.assertEqual(1, loading_report['ad_requests'])
    self.assertEqual(1, loading_report['ad_or_tracking_requests'])
    self.assertEqual(1, loading_report['ad_or_tracking_initiated_requests'])
    self.assertIsNone(loading_report['tracking_requests'])
    self.assertEqual(
        self._FIRST_REQUEST_DATA_LENGTH + metrics.HTTP_OK_LENGTH,
        loading_report['ad_or_tracking_initiated_transfer_size'])

  def testThreadBusyness(self):
    loading_report = report.LoadingReport(self._MakeTrace()).GenerateReport()
    self.assertAlmostEqual(
        1., loading_report['activity_significant_paint_frac'])
    self.assertAlmostEqual(
        float(self._TOPLEVEL_EVENT_DURATION - self._TOPLEVEL_EVENT_OFFSET)
        / (self._CONTENTFUL_PAINT - self._NAVIGATION_START_TIME),
        loading_report['activity_contentful_paint_frac'])
    self.assertAlmostEqual(
        float(self._TOPLEVEL_EVENT_DURATION - self._TOPLEVEL_EVENT_OFFSET)
        / (self._LOAD_END_TIME - self._NAVIGATION_START_TIME),
        loading_report['activity_load_frac'])

  def testActivityBreakdown(self):
    loading_report = report.LoadingReport(self._MakeTrace()).GenerateReport()
    load_time = float(self._LOAD_END_TIME - self._NAVIGATION_START_TIME)
    contentful_time = float(
        self._CONTENTFUL_PAINT - self._NAVIGATION_START_TIME)

    self.assertAlmostEqual(self._SCRIPT_EVENT_DURATION / load_time,
                           loading_report['script_load_frac'])
    self.assertAlmostEqual(
        (self._PARSING_EVENT_DURATION - self._SCRIPT_EVENT_DURATION)
        / load_time,
        loading_report['parsing_load_frac'])

    self.assertAlmostEqual(1., loading_report['script_significant_frac'])
    self.assertAlmostEqual(0., loading_report['parsing_significant_frac'])

    self.assertAlmostEqual(self._SCRIPT_EVENT_DURATION / contentful_time,
                           loading_report['script_contentful_frac'])
    self.assertAlmostEqual(
        (self._PARSING_EVENT_DURATION - self._SCRIPT_EVENT_DURATION)
        / contentful_time, loading_report['parsing_contentful_frac'])


if __name__ == '__main__':
  unittest.main()
