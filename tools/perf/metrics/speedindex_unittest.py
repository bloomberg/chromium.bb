# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.core.timeline import model
from metrics import speedindex

# pylint: disable=W0212
# (Disabled because these tests test private methods in speed_index_metric)

_RAW_EVENTS =  [
  { "type": "Paint",
    "startTime": 100,
    "endTime": 110,
    "frameId": "1.1",
    "data": {"clip": [0, 0, 1000, 0, 1000, 500, 0, 500]},
    "children": [],
  },
  { "type": "ResourceReceiveResponse",
    "startTime": 120,
    "endTime": 130,
    "frameId": "1.1",
    "data": {},
    "children": [],
  },
  { "type": "Layout",
    "startTime": 140,
    "endTime": 150,
    "frameId": "1.1",
    "data": {},
    "children": [],
  },
  { "type": "Paint",
    "startTime": 300,
    "endTime": 400,
    "frameId": "1.1",
    "data": {"clip": [375, 300, 380, 300, 380, 400, 375, 400]},
    "children": [],
  },
  { "type": "Paint",
    "startTime": 400,
    "endTime": 500,
    "frameId": "1.1",
    "data": {"clip": [0, 0, 500, 0, 500, 100, 0, 100]},
    "children": [],
  }
]

_RAW_EVENTS_SAME_RECTANGLE =  [
  { "type": "Paint",
    "startTime": 0,
    "endTime": 100,
    "frameId": "1.1",
    "data": {"clip": [0, 0, 500, 0, 500, 100, 0, 100]},
    "children": [],
  },
  { "type": "Paint",
    "startTime": 400,
    "endTime": 500,
    "frameId": "1.1",
    "data": {"clip": [ 0, 0, 500, 0, 500, 100, 0, 100]},
    "children": [],
  }
]

_EVENTS = model.TimelineModel(event_data=_RAW_EVENTS,
    shift_world_to_zero=False).GetAllEvents()

_EVENTS_ONLY_PAINT = [e for e in _EVENTS if e.name == 'Paint']

_EVENTS_SAME_RECTANGLE = model.TimelineModel(
  event_data=_RAW_EVENTS_SAME_RECTANGLE,
  shift_world_to_zero=False).GetAllEvents()


class _StartTimeTest(unittest.TestCase):
  def testWithAllEvents(self):
    start_time = speedindex._StartTime(_EVENTS)
    self.assertEquals(140, start_time)

  def testWithNoLayoutEvents(self):
    start_time = speedindex._StartTime(_EVENTS_SAME_RECTANGLE)
    self.assertEquals(0, start_time)


class _GetRectangleTest(unittest.TestCase):
  def testGetRectangleBig(self):
    rectangle = speedindex._GetRectangle(_EVENTS[0])
    self.assertEquals(("1.1", 0, 0, 1000, 500), rectangle)

  def testGetRectangleSmall(self):
    rectangle = speedindex._GetRectangle(_EVENTS[3])
    self.assertEquals(("1.1", 375, 300, 5, 100), rectangle)


class _GroupEventByRectangleTest(unittest.TestCase):
  def testNotGrouped(self):
    grouped = speedindex._GroupEventByRectangle(_EVENTS_ONLY_PAINT)
    self.assertEquals(3, len(grouped.keys()))
    self.assertEquals(1, len(grouped.values()[0]))

  def testGrouped(self):
    grouped = speedindex._GroupEventByRectangle(_EVENTS_SAME_RECTANGLE)
    self.assertEquals(1, len(grouped.keys()))
    self.assertEquals(2, len(grouped.values()[0]))


class _GetTimeAreaPairsTest(unittest.TestCase):
  def testAdjusted(self):
    time_area_pairs = (speedindex._GetTimeAreaPairs(_EVENTS_ONLY_PAINT))
    total_area = sum([area for _, area in time_area_pairs])
    expected_total_area = (1000*500/2) + (5*100) + (500*100)
    self.assertEquals(expected_total_area, total_area)

  def testAdjustedWithSameRectangle(self):
    time_area_pairs = (speedindex._GetTimeAreaPairs(_EVENTS_SAME_RECTANGLE))
    total_area = sum([area for _, area in time_area_pairs])
    expected_total_area = (500*100/2)
    self.assertEquals(expected_total_area, total_area)


class _GetTimeCompletenessPairsTest(unittest.TestCase):
  def setUp(self):
    time_area_pairs = (speedindex._GetTimeAreaPairs(_EVENTS_ONLY_PAINT))
    self.time_completeness_pairs = (speedindex.
                                    _GetTimeCompletenessPairs(time_area_pairs))

  def testEndsWithOne(self):
    self.assertEquals(1.0, self.time_completeness_pairs[-1][1])

  def testMonotonicallyIncreasing(self):
    prev_completeness = 0
    for _, completeness in self.time_completeness_pairs:
      self.assertTrue(prev_completeness <= completeness)
      prev_completeness = completeness


class _SpeedIndexTest(unittest.TestCase):
  def testOne(self):
    area1, area2 = 5 * 100, 500 * 100 / 2
    total_area = float(area1 + area2)
    completeness1 = area1 / total_area
    interval1 = 1.0 * (400 - 140)  # before first paint event
    interval2 = (1.0 - completeness1) * (500 - 400)
    expected = interval1 + interval2
    self.assertEquals(expected, speedindex._SpeedIndex(_EVENTS))

  def testTwo(self):
    interval1 = 1.0 * (100 - 0)  # before first paint event
    interval2 = 0.5 * (500 - 100)  # between paint event 1 and 2
    expected = interval1 + interval2
    self.assertEquals(expected, speedindex._SpeedIndex(_EVENTS_SAME_RECTANGLE))


if __name__ == "__main__":
  unittest.main()

