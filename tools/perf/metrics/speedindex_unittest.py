# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These tests access private methods in the speedindex module.
# pylint: disable=W0212

import json
import os
import unittest

from telemetry.core import bitmap
from telemetry.timeline import inspector_timeline_data
from telemetry.timeline import model
from metrics import speedindex

# Sample timeline data in the json format provided by devtools.
# The sample events will be used in several tests below.
_TEST_DIR = os.path.join(os.path.dirname(__file__), 'unittest_data')
_SAMPLE_DATA = json.load(open(os.path.join(_TEST_DIR, 'sample_timeline.json')))
_SAMPLE_TIMELINE_DATA = inspector_timeline_data.InspectorTimelineData(
    _SAMPLE_DATA)
_SAMPLE_EVENTS = model.TimelineModel(
    timeline_data=_SAMPLE_TIMELINE_DATA).GetAllEvents()


class FakeTimelineModel(object):

  def __init__(self):
    self._events = []

  def SetAllEvents(self, events):
    self._events = events

  def GetAllEvents(self, recursive=True):
    assert recursive == True
    return self._events


class FakeVideo(object):

  def __init__(self, frames):
    self._frames = frames

  def GetVideoFrameIter(self):
    for frame in self._frames:
      yield frame

class FakeBitmap(object):

  def __init__(self, r, g, b):
    self._histogram = bitmap.ColorHistogram(r, g, b, bitmap.WHITE)

  # pylint: disable=W0613
  def ColorHistogram(self, ignore_color=None, tolerance=None):
    return self._histogram


class FakeTab(object):

  def __init__(self, video_capture_result=None):
    self._timeline_model = FakeTimelineModel()
    self._javascript_result = None
    self._video_capture_result = FakeVideo(video_capture_result)

  @property
  def timeline_model(self):
    return self._timeline_model

  @property
  def video_capture_supported(self):
    return self._video_capture_result is not None

  def SetEvaluateJavaScriptResult(self, result):
    self._javascript_result = result

  def EvaluateJavaScript(self, _):
    return self._javascript_result

  def StartVideoCapture(self, min_bitrate_mbps=1):
    assert self.video_capture_supported
    assert min_bitrate_mbps > 0

  def StopVideoCapture(self):
    assert self.video_capture_supported
    return self._video_capture_result

  def Highlight(self, _):
    pass


class IncludedPaintEventsTest(unittest.TestCase):
  def testNumberPaintEvents(self):
    impl = speedindex.PaintRectSpeedIndexImpl()
    # In the sample data, there's one event that occurs before the layout event,
    # and one paint event that's not a leaf paint event.
    events = impl._IncludedPaintEvents(_SAMPLE_EVENTS)
    self.assertEqual(len(events), 5)


class SpeedIndexImplTest(unittest.TestCase):
  def testAdjustedAreaDict(self):
    impl = speedindex.PaintRectSpeedIndexImpl()
    paint_events = impl._IncludedPaintEvents(_SAMPLE_EVENTS)
    viewport = 1000, 1000
    time_area_dict = impl._TimeAreaDict(paint_events, viewport)
    self.assertEqual(len(time_area_dict), 4)
    # The event that ends at time 100 is a fullscreen; it's discounted by half.
    self.assertEqual(time_area_dict[100], 500000)
    self.assertEqual(time_area_dict[300], 100000)
    self.assertEqual(time_area_dict[400], 200000)
    self.assertEqual(time_area_dict[800], 200000)

  def testVideoCompleteness(self):
    frames = [
        (0.0, FakeBitmap([ 0, 0, 0,10], [ 0, 0, 0,10], [ 0, 0, 0,10])),
        (0.1, FakeBitmap([10, 0, 0, 0], [10, 0, 0, 0], [10, 0, 0, 0])),
        (0.2, FakeBitmap([ 0, 0, 2, 8], [ 0, 0, 4, 6], [ 0, 0, 1, 9])),
        (0.3, FakeBitmap([ 0, 3, 2, 5], [ 2, 1, 0, 7], [ 0, 3, 0, 7])),
        (0.4, FakeBitmap([ 0, 0, 1, 0], [ 0, 0, 1, 0], [ 0, 0, 1, 0])),
        (0.5, FakeBitmap([ 0, 4, 6, 0], [ 0, 4, 6, 0], [ 0, 4, 6, 0])),
    ]
    max_distance = 42.

    tab = FakeTab(frames)
    impl = speedindex.VideoSpeedIndexImpl()
    impl.Start(tab)
    impl.Stop(tab)
    time_completeness = impl.GetTimeCompletenessList(tab)
    self.assertEqual(len(time_completeness), 6)
    self.assertEqual(time_completeness[0], (0.0, 0))
    self.assertTimeCompleteness(
        time_completeness[1], 0.1, 1 - (16 + 16 + 16) / max_distance)
    self.assertTimeCompleteness(
        time_completeness[2], 0.2, 1 - (12 + 10 + 13) / max_distance)
    self.assertTimeCompleteness(
        time_completeness[3], 0.3, 1 - (6 + 10 + 8) / max_distance)
    self.assertTimeCompleteness(
        time_completeness[4], 0.4, 1 - (4 + 4 + 4) / max_distance)
    self.assertEqual(time_completeness[5], (0.5, 1))

  def testBlankPage(self):
    frames = [
        (0.0, FakeBitmap([0, 0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 1])),
        (0.1, FakeBitmap([0, 0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 1])),
        (0.2, FakeBitmap([1, 0, 0, 0], [0, 0, 0, 1], [0, 0, 0, 1])),
        (0.3, FakeBitmap([0, 0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 1])),
    ]
    tab = FakeTab(frames)
    impl = speedindex.VideoSpeedIndexImpl()
    impl.Start(tab)
    impl.Stop(tab)
    time_completeness = impl.GetTimeCompletenessList(tab)
    self.assertEqual(len(time_completeness), 4)
    self.assertEqual(time_completeness[0], (0.0, 1.0))
    self.assertEqual(time_completeness[1], (0.1, 1.0))
    self.assertEqual(time_completeness[2], (0.2, 0.0))
    self.assertEqual(time_completeness[3], (0.3, 1.0))

  def assertTimeCompleteness(self, time_completeness, time, completeness):
    self.assertEqual(time_completeness[0], time)
    self.assertAlmostEqual(time_completeness[1], completeness)


class SpeedIndexTest(unittest.TestCase):
  def testWithSampleData(self):
    tab = FakeTab()
    impl = speedindex.PaintRectSpeedIndexImpl()
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
    tab.timeline_model.SetAllEvents(_SAMPLE_EVENTS)
    tab.SetEvaluateJavaScriptResult(viewport)
    actual = impl.CalculateSpeedIndex(tab)
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
    tab = FakeTab()
    impl = speedindex.PaintRectSpeedIndexImpl()
    file_path = os.path.join(_TEST_DIR, filename)
    with open(file_path) as json_file:
      raw_events = json.load(json_file)
      timeline_data = inspector_timeline_data.InspectorTimelineData(raw_events)
      tab.timeline_model.SetAllEvents(
          model.TimelineModel(timeline_data=timeline_data).GetAllEvents())
      tab.SetEvaluateJavaScriptResult(viewport)
      actual = impl.CalculateSpeedIndex(tab)
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
