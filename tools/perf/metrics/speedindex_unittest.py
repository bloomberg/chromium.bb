# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These tests access private methods in the speedindex module.
# pylint: disable=W0212

import json
import os
import unittest

from telemetry.core.timeline import model
from metrics import speedindex

# Sample timeline data in the json format provided by devtools.
# The sample events will be used in several tests below.
_TEST_DIR = os.path.join(os.path.dirname(__file__), 'unittest_data')
_SAMPLE_DATA = json.load(open(os.path.join(_TEST_DIR, 'sample_timeline.json')))
_SAMPLE_EVENTS = model.TimelineModel(event_data=_SAMPLE_DATA).GetAllEvents()


class IncludedPaintEventsTest(unittest.TestCase):
  def testNumberPaintEvents(self):
    # In the sample data, there's one event that occurs before the layout event,
    # and one paint event that's not a leaf paint event.
    events = speedindex._IncludedPaintEvents(_SAMPLE_EVENTS)
    self.assertEquals(len(events), 5)


class TimeAreaDictTest(unittest.TestCase):
  def testAdjustedAreaDict(self):
    paint_events = speedindex._IncludedPaintEvents(_SAMPLE_EVENTS)
    viewport = 1000, 1000
    time_area_dict = speedindex._TimeAreaDict(paint_events, viewport)
    self.assertEquals(len(time_area_dict), 4)
    # The event that ends at time 100 is a fullscreen; it's discounted by half.
    self.assertEquals(time_area_dict[100], 500000)
    self.assertEquals(time_area_dict[300], 100000)
    self.assertEquals(time_area_dict[400], 200000)
    self.assertEquals(time_area_dict[800], 200000)


class SpeedIndexTest(unittest.TestCase):
  def testWithSampleData(self):
    viewport = 1000, 1000
    # Add up the parts of the speed index for each time interval.
    # Each part is the time interval multiplied by the proportion of the
    # total area value that is not yet painted for that interval.
    parts = []
    parts.append(100 * 1.0)
    parts.append(200 * 0.5)
    parts.append(100 * 0.4)
    parts.append(400 * 0.2)
    expected = sum(parts)  # 330.0
    actual = speedindex._SpeedIndex(_SAMPLE_EVENTS, viewport)
    self.assertEqual(actual, expected)


class WPTComparisonTest(unittest.TestCase):
  """Compare the speed index results with results given by webpagetest.org.

  Given the same timeline data, both this speedindex metric and webpagetest.org
  should both return the same results. Fortunately, webpagetest.org also
  provides timeline data in json format along with the speed index results.
  """

  def _TestJsonTimelineExpectation(self, filename, viewport, expected):
    """Check whether the result for some timeline data is as expected.

    Args:
      filename: Filename of a json file which contains a
      expected: The result expected based on the WPT result.
    """
    file_path = os.path.join(_TEST_DIR, filename)
    with open(file_path) as json_file:
      raw_events = json.load(json_file)
      events = model.TimelineModel(event_data=raw_events).GetAllEvents()
      actual = speedindex._SpeedIndex(events, viewport)
      # The result might differ by 1 or more milliseconds due to rounding,
      # so compare to the nearest 10 milliseconds.
      self.assertAlmostEqual(actual, expected, places=-1)

  def testCern(self):
    # Page: http://info.cern.ch/hypertext/WWW/TheProject.html
    # This page has only one paint event.
    self._TestJsonTimelineExpectation(
        'cern_repeat_timeline.json', (1014, 650), 379.0)

  def testBaidu(self):
    # Page: http://www.baidu.com/
    # This page has several paint events, but no nested paint events.
    self._TestJsonTimelineExpectation(
        'baidu_repeat_timeline.json', (1014, 650), 1761.43)

  def test2ch(self):
    # Page: http://2ch.net/
    # This page has several paint events, including nested paint events.
    self._TestJsonTimelineExpectation(
        '2ch_repeat_timeline.json', (997, 650), 674.58)


if __name__ == "__main__":
  unittest.main()

