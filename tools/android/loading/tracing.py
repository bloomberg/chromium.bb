# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Monitor tracing events on chrome via chrome remote debugging."""

import devtools_monitor

class TracingTrack(devtools_monitor.Track):
  def __init__(self, connection, categories=None, fetch_stream=False):
    """Initialize this TracingTrack.

    Args:
      connection: a DevToolsConnection.
      categories: None, or a string, or list of strings, of tracing categories
        to filter.

      fetch_stream: if true, use a websocket stream to fetch tracing data rather
        than dataCollected events. It appears based on very limited testing that
        a stream is slower than the default reporting as dataCollected events.
    """
    super(TracingTrack, self).__init__(connection)
    connection.RegisterListener('Tracing.dataCollected', self)
    params = {}
    if categories:
      params['categories'] = (categories if type(categories) is str
                              else ','.join(categories))
    if fetch_stream:
      params['transferMode'] = 'ReturnAsStream'

    connection.SyncRequestNoResponse('Tracing.start', params)
    self._events = []

  def Handle(self, method, event):
    self._events.append(event)

  def GetEvents(self):
    return self._events
