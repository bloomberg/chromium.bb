# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities used in unit tests, within this directory."""

import devtools_monitor
import loading_trace
import page_track


class FakeRequestTrack(devtools_monitor.Track):
  def __init__(self, events):
    super(FakeRequestTrack, self).__init__(None)
    self._events = [self._RewriteEvent(e) for e in events]

  def GetEvents(self):
    return self._events

  def _RewriteEvent(self, event):
    # This modifies the instance used across tests, so this method
    # must be idempotent.
    event.timing = event.timing._replace(request_time=event.timestamp)
    return event


class FakePageTrack(devtools_monitor.Track):
  def __init__(self, events):
    super(FakePageTrack, self).__init__(None)
    self._events = events

  def GetEvents(self):
    return self._events

  def GetMainFrameId(self):
    event = self._events[0]
    # Make sure our laziness is not an issue here.
    assert event['method'] == page_track.PageTrack.FRAME_STARTED_LOADING
    return event['frame_id']


def LoadingTraceFromEvents(requests, page_events=None):
  """Returns a LoadingTrace instance from a list of requests and page events."""
  request_track = FakeRequestTrack(requests)
  page = FakePageTrack(page_events if page_events else [])
  return loading_trace.LoadingTrace(None, None, page, request_track, None)
