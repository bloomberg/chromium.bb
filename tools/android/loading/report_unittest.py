# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

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

  def setUp(self):
    self.trace_creator = test_utils.TraceCreator()
    self.requests = [self.trace_creator.RequestAt(self._FIRST_REQUEST_TIME),
                     self.trace_creator.RequestAt(
                         self._NAVIGATION_START_TIME + self._REQUEST_OFFSET,
                         self._DURATION)]
    self.requests[0].timing.receive_headers_end = 0
    self.requests[1].timing.receive_headers_end = 0
    self.requests[0].encoded_data_length = 128
    self.requests[1].encoded_data_length = 1024

    self.trace_events = [
        {'ts': self._NAVIGATION_START_TIME * self.MILLI_TO_MICRO, 'ph': 'R',
         'cat': 'blink.user_timing',
         'name': 'navigationStart',
         'args': {'frame': 1}},
        {'ts': self._LOAD_END_TIME * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'devtools.timeline',
         'name': 'MarkLoad',
         'args': {'data': {'isMainFrame': True}}},
        {'ts': self._CONTENTFUL_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink.user_timing',
         'name': 'firstContentfulPaint',
         'args': {'frame': self._MAIN_FRAME_ID}},
        {'ts': self._TEXT_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink.user_timing',
         'name': 'firstPaint',
         'args': {'frame': self._MAIN_FRAME_ID}},
        {'ts': 90 * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'blink',
         'name': 'FrameView::synchronizedPaint'},
        {'ts': self._SIGNIFICANT_PAINT * self.MILLI_TO_MICRO, 'ph': 'I',
         'cat': 'foobar', 'name': 'biz',
         'args': {'counters': {
             'LayoutObjectsThatHadNeverHadLayout': 10
         }}}]

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
    self.assertEqual(None, loading_report['contentful_inversion'])
    self.assertEqual(None, loading_report['significant_inversion'])

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


if __name__ == '__main__':
  unittest.main()
