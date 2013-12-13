# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os

from metrics import Metric


class SpeedIndexMetric(Metric):
  """The speed index metric is one way of measuring page load speed.

  It is meant to approximate user perception of page load speed, and it
  is based on the amount of time that it takes to paint to the visual
  portion of the screen. It includes paint events that occur after the
  onload event, and it doesn't include time loading things off-screen.

  This speed index metric is based on WebPageTest.org (WPT).
  For more info see: http://goo.gl/e7AH5l
  """
  def __init__(self):
    super(SpeedIndexMetric, self).__init__()
    self._impl = None
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
    self._impl = (VideoSpeedIndexImpl(tab) if tab.video_capture_supported else
                  PaintRectSpeedIndexImpl(tab))
    self._impl.Start()
    self._script_is_loaded = False
    self._is_finished = False

  def Stop(self, _, tab):
    """Stop timeline recording."""
    assert self._impl, 'Must call Start() before Stop()'
    assert self.IsFinished(tab), 'Must wait for IsFinished() before Stop()'
    self._impl.Stop()

  # Optional argument chart_name is not in base class Metric.
  # pylint: disable=W0221
  def AddResults(self, tab, results, chart_name=None):
    """Calculate the speed index and add it to the results."""
    index = self._impl.CalculateSpeedIndex()
    # Release the tab so that it can be disconnected.
    self._impl = None
    results.Add('speed_index', 'ms', index, chart_name=chart_name)

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
    # not be available here. The script should only be re-loaded once per page
    # so that variables in the script get reset only for a new page.
    if not self._script_is_loaded:
      tab.ExecuteJavaScript(self._js)
      self._script_is_loaded = True

    time_since_last_response_ms = tab.EvaluateJavaScript(
        "window.timeSinceLastResponseAfterLoadMs()")
    self._is_finished = time_since_last_response_ms > 2000
    return self._is_finished


class SpeedIndexImpl(object):

  def __init__(self, tab):
    """Constructor.

    Args:
      tab: The telemetry.core.Tab object for which to calculate SpeedIndex.
    """
    self.tab = tab

  def Start(self):
    raise NotImplementedError()

  def Stop(self):
    raise NotImplementedError()

  def GetTimeCompletenessList(self):
    """Returns a list of time to visual completeness tuples.

    In the WPT PHP implementation, this is also called 'visual progress'.
    """
    raise NotImplementedError()

  def CalculateSpeedIndex(self):
    """Calculate the speed index.

    The speed index number conceptually represents the number of milliseconds
    that the page was "visually incomplete". If the page were 0% complete for
    1000 ms, then the score would be 1000; if it were 0% complete for 100 ms
    then 90% complete (ie 10% incomplete) for 900 ms, then the score would be
    1.0*100 + 0.1*900 = 190.

    Returns:
      A single number, milliseconds of visual incompleteness.
    """
    time_completeness_list = self.GetTimeCompletenessList()
    prev_completeness = 0.0
    speed_index = 0.0
    prev_time = time_completeness_list[0][0]
    for time, completeness in time_completeness_list:
      # Add the incemental value for the interval just before this event.
      elapsed_time = time - prev_time
      incompleteness = (1.0 - prev_completeness)
      speed_index += elapsed_time * incompleteness

      # Update variables for next iteration.
      prev_completeness = completeness
      prev_time = time
    return speed_index


class VideoSpeedIndexImpl(SpeedIndexImpl):

  def __init__(self, tab):
    super(VideoSpeedIndexImpl, self).__init__(tab)
    assert self.tab.video_capture_supported
    self._time_completeness_list = None

  def Start(self):
    # TODO(tonyg): Bitrate is arbitrary here. Experiment with screen capture
    # overhead vs. speed index accuracy and set the bitrate appropriately.
    self.tab.StartVideoCapture(min_bitrate_mbps=4)

  def Stop(self):
    histograms = [(time, bitmap.ColorHistogram())
                  for time, bitmap in self.tab.StopVideoCapture()]

    start_histogram = histograms[0][1]
    final_histogram = histograms[-1][1]

    def Difference(hist1, hist2):
      return (abs(a - b) for a, b in zip(hist1, hist2))

    full_difference = list(Difference(start_histogram, final_histogram))
    total = float(sum(full_difference))

    def FrameProgress(histogram):
      difference = Difference(start_histogram, histogram)
      # Each color bucket is capped at the full difference, so that progress
      # does not exceed 100%.
      return sum(min(a, b) for a, b in zip(difference, full_difference))

    self._time_completeness_list = [(time, FrameProgress(hist) / total)
                                    for time, hist in histograms]

  def GetTimeCompletenessList(self):
    assert self._time_completeness_list, 'Must call Stop() first.'
    return self._time_completeness_list


class PaintRectSpeedIndexImpl(SpeedIndexImpl):

  def __init__(self, tab):
    super(PaintRectSpeedIndexImpl, self).__init__(tab)

  def Start(self):
    self.tab.StartTimelineRecording()

  def Stop(self):
    self.tab.StopTimelineRecording()

  def GetTimeCompletenessList(self):
    events = self.tab.timeline_model.GetAllEvents()
    viewport = self._GetViewportSize()
    paint_events = self._IncludedPaintEvents(events)
    time_area_dict = self._TimeAreaDict(paint_events, viewport)
    total_area = sum(time_area_dict.values())
    assert total_area > 0.0, 'Total paint event area must be greater than 0.'
    completeness = 0.0
    time_completeness_list = []

    # TODO(tonyg): This sets the start time to the start of the first paint
    # event. That can't be correct. The start time should be navigationStart.
    # Since the previous screen is not cleared at navigationStart, we should
    # probably assume the completeness is 0 until the first paint and add the
    # time of navigationStart as the start. We need to confirm what WPT does.
    time_completeness_list.append(
        (self.tab.timeline_model.GetAllEvents()[0].start, completeness))

    for time, area in sorted(time_area_dict.items()):
      completeness += float(area) / total_area
      # Visual progress is rounded to the nearest percentage point as in WPT.
      time_completeness_list.append((time, round(completeness, 2)))
    return time_completeness_list

  def _GetViewportSize(self):
    """Returns dimensions of the viewport."""
    return self.tab.EvaluateJavaScript(
        '[ window.innerWidth, window.innerHeight ]')

  def _IncludedPaintEvents(self, events):
    """Get all events that are counted in the calculation of the speed index.

    There's one category of paint event that's filtered out: paint events
    that occur before the first 'ResourceReceiveResponse' and 'Layout' events.

    Previously in the WPT speed index, paint events that contain children paint
    events were also filtered out.
    """
    def FirstLayoutTime(events):
      """Get the start time of the first layout after a resource received."""
      has_received_response = False
      for event in events:
        if event.name == 'ResourceReceiveResponse':
          has_received_response = True
        elif has_received_response and event.name == 'Layout':
          return event.start
      assert False, 'There were no layout events after resource receive events.'

    first_layout_time = FirstLayoutTime(events)
    paint_events = [e for e in events
                    if e.start >= first_layout_time and e.name == 'Paint']
    return paint_events

  def _TimeAreaDict(self, paint_events, viewport):
    """Make a dict from time to adjusted area value for events at that time.

    The adjusted area value of each paint event is determined by how many paint
    events cover the same rectangle, and whether it's a full-window paint event.
    "Adjusted area" can also be thought of as "points" of visual completeness --
    each rectangle has a certain number of points and these points are
    distributed amongst the paint events that paint that rectangle.

    Args:
      paint_events: A list of paint events
      viewport: A tuple (width, height) of the window.

    Returns:
      A dictionary of times of each paint event (in milliseconds) to the
      adjusted area that the paint event is worth.
    """
    width, height = viewport
    fullscreen_area = width * height

    def ClippedArea(rectangle):
      """Returns rectangle area clipped to viewport size."""
      _, x0, y0, x1, y1 = rectangle
      clipped_width = max(0, min(width, x1) - max(0, x0))
      clipped_height = max(0, min(height, y1) - max(0, y0))
      return clipped_width * clipped_height

    grouped = self._GroupEventByRectangle(paint_events)
    event_area_dict = collections.defaultdict(int)

    for rectangle, events in grouped.items():
      # The area points for each rectangle are divided up among the paint
      # events in that rectangle.
      area = ClippedArea(rectangle)
      update_count = len(events)
      adjusted_area = float(area) / update_count

      # Paint events for the largest-area rectangle are counted as 50%.
      if area == fullscreen_area:
        adjusted_area /= 2

      for event in events:
        # The end time for an event is used for that event's time.
        event_time = event.end
        event_area_dict[event_time] += adjusted_area

    return event_area_dict

  def _GetRectangle(self, paint_event):
    """Get the specific rectangle on the screen for a paint event.

    Each paint event belongs to a frame (as in html <frame> or <iframe>).
    This, together with location and dimensions, comprises a rectangle.
    In the WPT source, this 'rectangle' is also called a 'region'.
    """
    def GetBox(quad):
      """Gets top-left and bottom-right coordinates from paint event.

      In the timeline data from devtools, paint rectangle dimensions are
      represented x-y coordinates of four corners, clockwise from the top-left.
      See: function WebInspector.TimelinePresentationModel.quadFromRectData
      in file src/out/Debug/obj/gen/devtools/TimelinePanel.js.
      """
      x0, y0, _, _, x1, y1, _, _ = quad
      return (x0, y0, x1, y1)

    assert paint_event.name == 'Paint'
    frame = paint_event.args['frameId']
    return (frame,) + GetBox(paint_event.args['data']['clip'])

  def _GroupEventByRectangle(self, paint_events):
    """Group all paint events according to the rectangle that they update."""
    result = collections.defaultdict(list)
    for event in paint_events:
      assert event.name == 'Paint'
      result[self._GetRectangle(event)].append(event)
    return result
