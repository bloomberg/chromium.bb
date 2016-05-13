# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import report
import test_utils
import user_satisfied_lens_unittest


class LoadingReportTestCase(unittest.TestCase):
  MILLI_TO_MICRO = 1000
  _FIRST_REQUEST_TIME = 15
  _CONTENTFUL_PAINT = 120
  _TEXT_PAINT = 30
  _SIGNIFICANT_PAINT = 50
  _DURATION = 400
  _REQUEST_OFFSET = 5

  @classmethod
  def _MakeTrace(cls):
    trace_creator = test_utils.TraceCreator()
    requests = [trace_creator.RequestAt(cls._FIRST_REQUEST_TIME),
                trace_creator.RequestAt(
                    cls._FIRST_REQUEST_TIME + cls._REQUEST_OFFSET,
                    cls._DURATION)]
    requests[0].timing.receive_headers_end = 0
    requests[1].timing.receive_headers_end = 0
    requests[0].encoded_data_length = 128
    requests[1].encoded_data_length = 1024
    trace = trace_creator.CreateTrace(
        requests,
        [{'ts': cls._CONTENTFUL_PAINT * cls.MILLI_TO_MICRO, 'ph': 'I',
          'cat': 'blink.user_timing',
          'name': 'firstContentfulPaint',
          'args': {'frame': 1}},
         {'ts': cls._TEXT_PAINT * cls.MILLI_TO_MICRO, 'ph': 'I',
          'cat': 'blink.user_timing',
          'name': 'firstPaint',
          'args': {'frame': 1}},
         {'ts': 90 * cls.MILLI_TO_MICRO, 'ph': 'I',
          'cat': 'blink',
          'name': 'FrameView::synchronizedPaint'},
         {'ts': cls._SIGNIFICANT_PAINT * cls.MILLI_TO_MICRO, 'ph': 'I',
          'cat': 'foobar', 'name': 'biz',
          'args': {'counters': {
              'LayoutObjectsThatHadNeverHadLayout': 10
          }}}], 1)
    return trace

  def testGenerateReport(self):
    trace = self._MakeTrace()
    loading_report = report.LoadingReport(trace).GenerateReport()
    self.assertEqual(trace.url, loading_report['url'])
    self.assertEqual(self._TEXT_PAINT - self._FIRST_REQUEST_TIME,
                     loading_report['first_text_ms'])
    self.assertEqual(self._SIGNIFICANT_PAINT - self._FIRST_REQUEST_TIME,
                     loading_report['significant_paint_ms'])
    self.assertEqual(self._CONTENTFUL_PAINT - self._FIRST_REQUEST_TIME,
                     loading_report['contentful_paint_ms'])
    self.assertEqual(self._REQUEST_OFFSET + self._DURATION,
                     loading_report['plt_ms'])
    self.assertAlmostEqual(0.3, loading_report['contentful_byte_frac'])
    self.assertAlmostEqual(0.14, loading_report['significant_byte_frac'], 2)


if __name__ == '__main__':
  unittest.main()
