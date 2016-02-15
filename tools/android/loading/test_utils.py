# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities used in unit tests, within this directory."""

import devtools_monitor
import loading_trace
import page_track
import request_track
import tracing


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


def MakeRequest(
    url, source_url, start_time, headers_time, end_time,
    magic_content_type=False, initiator_type='other'):
  assert initiator_type in ('other', 'parser')
  timing = request_track.TimingAsList(request_track.TimingFromDict({
      # connectEnd should be ignored.
      'connectEnd': (end_time - start_time) / 2,
      'receiveHeadersEnd': headers_time - start_time,
      'loadingFinished': end_time - start_time,
      'requestTime': start_time / 1000.0}))
  rq = request_track.Request.FromJsonDict({
      'timestamp': start_time / 1000.0,
      'request_id': str(MakeRequest._next_request_id),
      'url': 'http://' + str(url),
      'initiator': {'type': initiator_type, 'url': 'http://' + str(source_url)},
      'response_headers': {'Content-Type':
                           'null' if not magic_content_type
                           else 'magic-debug-content' },
      'timing': timing
  })
  MakeRequest._next_request_id += 1
  return rq


MakeRequest._next_request_id = 0


def LoadingTraceFromEvents(requests, page_events=None, trace_events=None):
  """Returns a LoadingTrace instance from a list of requests and page events."""
  request = FakeRequestTrack(requests)
  page_event_track = FakePageTrack(page_events if page_events else [])
  if trace_events:
    tracing_track = tracing.TracingTrack(None)
    tracing_track.Handle('Tracing.dataCollected',
                         {'params': {'value': [e for e in trace_events]}})
  else:
    tracing_track = None
  return loading_trace.LoadingTrace(
      None, None, page_event_track, request, tracing_track)
