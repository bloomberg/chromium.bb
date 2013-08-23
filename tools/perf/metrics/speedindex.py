# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os

from metrics import Metric

class SpeedIndexMetric(Metric):
  """The speed index metric is one way of measuring page load speed.

  It is meant to approximate user perception of page load speed --
  it's based on the amount of time that it takes to paint to the visual
  portion of the screen -- it includes paint events done after the load
  event, and it doesn't include time loading things off-screen

  This speed index metric is based on the one in WebPagetest:
  http://goo.gl/e7AH5l
  """
  def __init__(self):
    super(SpeedIndexMetric, self).__init__()
    self._script_is_loaded = False
    self._is_finished = False
    with open(os.path.join(os.path.dirname(__file__), 'speedindex.js')) as f:
      self._js = f.read()

  def Start(self, _, tab):
    """Start recording events.

    This method should be called in the WillNavigateToPage method of
    a PageMeasurement, so that all the events can be captured. If it's called
    in DidNavigateToPage, that will be too late.
    """
    tab.StartTimelineRecording()

  def Stop(self, _, tab):
    """Stop timeline recording."""
    assert self.IsFinished(tab)
    tab.StopTimelineRecording()

  def AddResults(self, tab, results):
    """Calculate the speed index and add it to the results."""
    events = tab.timeline_model.GetAllEvents()
    assert len(events) > 0
    results.Add('speed_index', 'ms', _SpeedIndex(events))

  def IsFinished(self, tab):
    """Decide whether the timeline recording should be stopped.

    When the timeline recording is stopped determines which paint events
    are used in the speed index metric calculation. In general, the recording
    should continue if there has just been some data received, because
    this suggests that painting may continue.

    A page may repeatedly request resources in an infinite loop; a timeout
    should be placed in any measurement that uses this metric, e.g.:
      def IsDone():
        return self._speedindex.IsFinished(tab)
      util.WaitFor(IsDone, 60)

    Returns:
      True if 2 seconds have passed since last resource received, false
      otherwise.
    """
    if self._is_finished:
      return True

    # The script that provides the function window.timeSinceLastResponseMs()
    # needs to be loaded for this function, but it must be loaded AFTER
    # the Start method is called, because if the Start method is called in
    # the PageMeasurement's WillNavigateToPage function, then it will
    # not be available here. The script should only be loaded once, so that
    # variables in the script don't get reset.
    if not self._script_is_loaded:
      tab.ExecuteJavaScript(self._js)
      self._script_is_loaded = True

    time_since_last_response_ms = tab.EvaluateJavaScript(
        "window.timeSinceLastResponseAfterLoadMs()")
    self._is_finished = time_since_last_response_ms > 2000
    return self._is_finished


def _SpeedIndex(events):
  """Calculate the speed index of a page load from a list of events.

  Args:
    events: A flat list of telemetry.core.timeline.slice.Slice objects
      as returned by telemetry.core.timeline.model.GetAllEvents().

  Returns:
    A single integer which represents the total time in milliseconds
    that the page was "visually incomplete". If the page were 100% incomplete
    (i.e. nothing painted) for 1000 ms, then the score would be 1000; if it
    were empty for 100 ms, then 90% complete (10% incomplete) for 900 ms, then
    the score would be 100 + 0.1*900 = 190.
  """
  paint_events = _GetPaintEvents(events)
  time_area_pairs = _GetTimeAreaPairs(paint_events)
  time_completeness_pairs = _GetTimeCompletenessPairs(time_area_pairs)
  speed_index = 0
  prev_time = _StartTime(events)
  completeness = 0.0
  for this_time, this_completeness in time_completeness_pairs:
    # Add the value for the time interval up to this event
    time_interval = this_time - prev_time
    speed_index += time_interval * (1 - completeness)

    # Update variables for next iteration
    completeness = this_completeness
    prev_time = this_time
  return speed_index


def _GetPaintEvents(timeline_events):
  """Get the paint events in the time range relevant to the speed index."""
  start = _StartTime(timeline_events)
  return [event for event in timeline_events
          if event.name == 'Paint' and event.start >= start]


def _StartTime(timeline_events):
  """Get the time of the earliest Paint event that should be considered.

  Before there are any ResourceReceiveResponse or Layout events,
  there may be Paint events. These paint events probably should be ignored,
  because they probably aren't actual elements of the page being displayed.

  Returns:
    If there's a ResourceReceiveResponse event then a Layout event, return the
    start time of the Layout event. Otherwise, return the time of the start
    of the first event.
  """
  has_received_response = False
  assert len(timeline_events) > 0
  for event in timeline_events:
    if event.name == "ResourceReceiveResponse":
      has_received_response = True
    elif has_received_response and event.name == "Layout":
      return event.start
  return timeline_events[0].start


def _GetTimeAreaPairs(paint_events):
  """Make a list of pairs associating event time and adjusted area value.

  The area value of a paint event is determined by how many paint events paint
  the same rectangle, and whether it is painting full window.

  Args:
    paint_events: A list of Paint events

  Returns:
    A list of pairs of the time of each paint event (in milliseconds) and the
    adjusted area that the paint event is worth.
  """
  def _RectangleArea(rectangle):
    return rectangle[3] * rectangle[4]
  grouped = _GroupEventByRectangle(paint_events)
  fullscreen_area = max([_RectangleArea(rect) for rect in grouped.keys()])
  event_area_pairs = []
  for rectangle in grouped:
    area = _RectangleArea(rectangle)
    # Paint events that are considered "full-screen" paint events
    # are discounted because they're often just painting a white background.
    # They are not completely disregarded though, because if we're painting
    # a colored background, it should be counted.
    if _RectangleArea(rectangle) == fullscreen_area:
      area /= 2
    area_per_event = area / len(grouped[rectangle])
    for event in grouped[rectangle]:
      end_time = event.end
      event_area_pairs.append((end_time, area_per_event))
  return event_area_pairs


def _GroupEventByRectangle(paint_events):
  """Group all paint events according to the rectangle that they update."""
  result = collections.defaultdict(list)
  for event in paint_events:
    assert event.name == 'Paint'
    result[_GetRectangle(event)].append(event)
  return result


def _GetRectangle(paint_event):
  """Get the specific rectangle on the screen for a paint event.

  Each paint event belongs to a frame (as in html <frame> or <iframe>).
  This, together with location and dimensions, comprises a rectangle.
  """
  assert paint_event.name == 'Paint'
  frame = paint_event.args['frameId']
  x, y, width, height = _GetBox(paint_event.args['data']['clip'])
  return (frame, x, y, width, height)


def _GetBox(quad):
  """Convert the "clip" data in a paint event to coordinates and dimensions.

  In the timeline data from devtools, paint rectangle dimensions are
  represented x-y coordinates of four corners, clockwise from the top-left.
  See: function WebInspector.TimelinePresentationModel.quadFromRectData
  in file src/out/Debug/obj/gen/devtools/TimelinePanel.js.
  """
  x0, y0, _, _, x1, y1, _, _ = quad
  width, height = (x1 - x0), (y1 - y0)
  return (x0, y0, width, height)


def _GetTimeCompletenessPairs(time_area_pairs):
  """Make a list associating time with the % completeness at that time.

  The list returned will also be ordered by time.
  """
  total_area = float(sum([area for (_, area) in time_area_pairs]))
  time_completeness_pairs = []
  completeness = 0
  for event_time, area in sorted(time_area_pairs):
    completeness_impact = float(area) / total_area
    completeness += completeness_impact
    time_completeness_pairs.append((event_time, completeness))
  return time_completeness_pairs

