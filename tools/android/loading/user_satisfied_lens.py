# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Identifies key events related to user satisfaction.

Several lenses are defined, for example FirstTextPaintLens and
FirstSignificantPaintLens.
"""
import logging
import operator

import common_util


class _UserSatisfiedLens(object):
  """A base class for all user satisfaction metrics.

  All of these work by identifying a user satisfaction event from the trace, and
  then building a set of request ids whose loading is needed to achieve that
  event. Subclasses need only provide the time computation. The base class will
  use that to construct the request ids.
  """
  _ATTRS = ['_satisfied_msec', '_event_msec', '_postload_msec',
            '_critical_request_ids']

  def __init__(self, trace):
    """Initialize the lens.

    Args:
      trace: (LoadingTrace) the trace to use in the analysis.
    """
    self._satisfied_msec = None
    self._event_msec = None
    self._postload_msec = None
    self._critical_request_ids = None
    if trace is None:
      return
    self._CalculateTimes(trace.tracing_track)
    critical_requests = self._RequestsBefore(
        trace.request_track, self._satisfied_msec)
    self._critical_request_ids = set(rq.request_id for rq in critical_requests)
    if critical_requests:
      last_load = max(rq.end_msec for rq in critical_requests)
    else:
      last_load = float('inf')
    self._postload_msec = self._event_msec - last_load

  def CriticalRequests(self):
    """Request ids of critical requests.

    Returns:
      A set of request ids (as strings) of an estimate of all requests that are
      necessary for the user satisfaction defined by this class.
    """
    return self._critical_request_ids

  def PostloadTimeMsec(self):
    """Return postload time.

    The postload time is an estimate of the amount of time needed by chrome to
    transform the critical results into the satisfying event.

    Returns:
      Postload time in milliseconds.
    """
    return self._postload_msec

  def ToJsonDict(self):
    return common_util.SerializeAttributesToJsonDict({}, self, self._ATTRS)

  @classmethod
  def FromJsonDict(cls, json_dict):
    result = cls(None)
    return common_util.DeserializeAttributesFromJsonDict(
        json_dict, result, cls._ATTRS)

  def _CalculateTimes(self, tracing_track):
    """Subclasses should implement to set _satisfied_msec and _event_msec."""
    raise NotImplementedError

  @classmethod
  def _RequestsBefore(cls, request_track, time_ms):
    return [rq for rq in request_track.GetEvents()
            if rq.end_msec <= time_ms]


class _FirstEventLens(_UserSatisfiedLens):
  """Helper abstract subclass that defines users first event manipulations."""
  # pylint can't handle abstract subclasses.
  # pylint: disable=abstract-method

  @classmethod
  def _CheckCategory(cls, tracing_track, category):
    assert category in tracing_track.Categories(), (
        'The "%s" category must be enabled.' % category)

  @classmethod
  def _ExtractFirstTiming(cls, times):
    if not times:
      return float('inf')
    if len(times) != 1:
      # TODO(mattcary): in some cases a trace has two first paint events. Why?
      logging.error('%d %s with spread of %s', len(times),
                    str(cls), max(times) - min(times))
    return float(min(times))


class FirstTextPaintLens(_FirstEventLens):
  """Define satisfaction by the first text paint.

  This event is taken directly from a trace.
  """
  _EVENT_CATEGORY = 'blink.user_timing'
  def _CalculateTimes(self, tracing_track):
    self._CheckCategory(tracing_track, self._EVENT_CATEGORY)
    first_paints = [e.start_msec for e in tracing_track.GetEvents()
                    if e.Matches(self._EVENT_CATEGORY, 'firstPaint')]
    self._satisfied_msec = self._event_msec = \
        self._ExtractFirstTiming(first_paints)


class FirstContentfulPaintLens(_FirstEventLens):
  """Define satisfaction by the first contentful paint.

  This event is taken directly from a trace. Internally to chrome it's computed
  by filtering out things like background paint from firstPaint.
  """
  _EVENT_CATEGORY = 'blink.user_timing'
  def _CalculateTimes(self, tracing_track):
    self._CheckCategory(tracing_track, self._EVENT_CATEGORY)
    first_paints = [e.start_msec for e in tracing_track.GetEvents()
                    if e.Matches(self._EVENT_CATEGORY, 'firstContentfulPaint')]
    self._satisfied_msec = self._event_msec = \
       self._ExtractFirstTiming(first_paints)


class FirstSignificantPaintLens(_FirstEventLens):
  """Define satisfaction by the first paint after a big layout change.

  Our satisfaction time is that of the layout change, as all resources must have
  been loaded to compute the layout. Our event time is that of the next paint as
  that is the observable event.
  """
  _FIRST_LAYOUT_COUNTER = 'LayoutObjectsThatHadNeverHadLayout'
  _EVENT_CATEGORY = 'disabled-by-default-blink.debug.layout'
  def _CalculateTimes(self, tracing_track):
    self._CheckCategory(tracing_track, self._EVENT_CATEGORY)
    sync_paint_times = []
    layouts = []  # (layout item count, msec).
    for e in tracing_track.GetEvents():
      # TODO(mattcary): is this the right paint event? Check if synchronized
      # paints appear at the same time as the first*Paint events, above.
      if e.Matches('blink', 'FrameView::synchronizedPaint'):
        sync_paint_times.append(e.start_msec)
      if ('counters' in e.args and
          self._FIRST_LAYOUT_COUNTER in e.args['counters']):
        layouts.append((e.args['counters'][self._FIRST_LAYOUT_COUNTER],
                        e.start_msec))
    assert layouts, 'No layout events'
    layouts.sort(key=operator.itemgetter(0), reverse=True)
    self._satisfied_msec = layouts[0][1]
    self._event_msec = self._ExtractFirstTiming([
        min(t for t in sync_paint_times if t > self._satisfied_msec)])
