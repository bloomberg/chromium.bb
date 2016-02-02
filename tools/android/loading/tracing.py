# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Monitor tracing events on chrome via chrome remote debugging."""

import bisect
import itertools

import devtools_monitor


class TracingTrack(devtools_monitor.Track):
  """Grabs and processes trace event messages.

  See https://goo.gl/Qabkqk for details on the protocol.
  """
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
    if connection:
      connection.RegisterListener('Tracing.dataCollected', self)
    params = {}
    if categories:
      params['categories'] = (categories if type(categories) is str
                              else ','.join(categories))
    if fetch_stream:
      params['transferMode'] = 'ReturnAsStream'

    if connection:
      connection.SyncRequestNoResponse('Tracing.start', params)
    self._events = []

    self._event_msec_index = None
    self._event_lists = None
    self._base_msec = None

  def Handle(self, method, event):
    for e in event['params']['value']:
      event = Event(e)
      self._events.append(event)
      if self._base_msec is None or event.start_msec < self._base_msec:
        self._base_msec = event.start_msec
    # Just invalidate our indices rather than trying to be fancy and
    # incrementally update.
    self._event_msec_index = None
    self._event_lists = None

  def GetFirstEventMillis(self):
    """Find the canonical start time for this track.

    Returns:
      The millisecond timestamp of the first request.
    """
    return self._base_msec

  def GetEvents(self):
    return self._events

  def EventsAt(self, msec):
    """Gets events active at a timestamp.

    Args:
      msec: tracing milliseconds to query. Tracing milliseconds appears to be
        since chrome startup (ie, arbitrary epoch).

    Returns:
      List of events active at that timestamp. Instantaneous (ie, instant,
      sample and counter) events are never included. Event end times are
      exclusive, so that an event ending at the usec parameter will not be
      returned.
      TODO(mattcary): currently live objects are included. If this is too big we
      may break that out into a separate index.
    """
    self._IndexEvents()
    idx = bisect.bisect_right(self._event_msec_index, msec) - 1
    if idx < 0:
      return []
    events = self._event_lists[idx]
    assert events.start_msec <= msec
    if not events or events.end_msec < msec:
      return []
    return events.event_list

  def ToJsonDict(self):
    return {'events': [e.ToJsonDict() for e in self._events]}

  @classmethod
  def FromJsonDict(cls, json_dict):
    assert 'events' in json_dict
    events = [Event(e) for e in json_dict['events']]
    tracing_track = TracingTrack(None)
    tracing_track._events = events
    tracing_track._base_msec = events[0].start_msec if events else 0
    for e in events[1:]:
      if e.type == 'M':
        continue  # No timestamp for metadata events.
      assert e.start_msec > 0
      if e.start_msec < tracing_track._base_msec:
        tracing_track._base_msec = e.start_msec
    return tracing_track

  def EventsEndingBetween(self, start_msec, end_msec):
    """Gets the list of events whose end lies in a range.

    Args:
      start_msec: the start of the range to query, in milliseconds, inclusive.
      end_msec: the end of the range to query, in milliseconds, inclusive.

    Returns:
      List of events whose end time lies in the range. Note that although the
      range is inclusive at both ends, an ending timestamp is considered to be
      exclusive of the actual event. An event ending at 10 msec will be returned
      for a range [10, 14] as well as [8, 10], though the event is considered to
      end the instant before 10 msec. In practice this is only important when
      considering how events overlap; an event ending at 10 msec does not
      overlap with one starting at 10 msec and so may unambiguously share ids,
      etc.
    """
    self._IndexEvents()
    low_idx = bisect.bisect_left(self._event_msec_index, start_msec) - 1
    high_idx = bisect.bisect_right(self._event_msec_index, end_msec)
    matched_events = []
    for i in xrange(max(0, low_idx), high_idx):
      if self._event_lists[i]:
        for e in self._event_lists[i].event_list:
          assert e.end_msec is not None
          if e.end_msec >= start_msec and e.end_msec <= end_msec:
            matched_events.append(e)
    return matched_events

  def _IndexEvents(self):
    """Computes index for in-flight events.

    Creates a list of timestamps where events start or end, and tracks the
    current set of in-flight events at the instant after each timestamp. To do
    this we have to synthesize ending events for complete events, as well as
    join and track the nesting of async, flow and other spanning events.

    Events such as instant and counter events that aren't indexable are skipped.
    """
    if self._event_msec_index is not None:
      return  # Already indexed.

    if not self._events:
      raise devtools_monitor.DevToolsConnectionException('No events to index')

    self._event_msec_index = []
    self._event_lists = []
    synthetic_events = []
    for e in self._events:
      synthetic_events.extend(e.Synthesize())
    synthetic_events.sort(key=lambda e: e.start_msec)
    current_events = set()
    next_idx = 0
    spanning_events = self._SpanningEvents()
    while next_idx < len(synthetic_events):
      current_msec = synthetic_events[next_idx].start_msec
      while next_idx < len(synthetic_events):
        event = synthetic_events[next_idx]
        assert event.IsIndexable()
        if event.start_msec > current_msec:
          break
        matched_event = spanning_events.Match(event)
        if matched_event is not None:
          event = matched_event
        if not event.synthetic and (
            event.end_msec is None or event.end_msec >= current_msec):
          current_events.add(event)
        next_idx += 1
      current_events -= set([
          e for e in current_events
          if e.end_msec is not None and e.end_msec <= current_msec])
      self._event_msec_index.append(current_msec)
      self._event_lists.append(self._EventList(current_events))
    if spanning_events.HasPending():
      raise devtools_monitor.DevToolsConnectionException(
          'Pending spanning events: %s' %
          '\n'.join([str(e) for e in spanning_events.PendingEvents()]))

  class _SpanningEvents(object):
    def __init__(self):
      self._duration_stack = []
      self._async_stacks = {}
      self._objects = {}
      self._MATCH_HANDLER = {
          'B': self._DurationBegin,
          'E': self._DurationEnd,
          'b': self._AsyncStart,
          'e': self._AsyncEnd,
          'S': self._AsyncStart,
          'F': self._AsyncEnd,
          'N': self._ObjectCreated,
          'D': self._ObjectDestroyed,
          'X': self._Ignore,
          'M': self._Ignore,
          None: self._Ignore,
          }

    def Match(self, event):
      return self._MATCH_HANDLER.get(
          event.type, self._Unsupported)(event)

    def HasPending(self):
      return (self._duration_stack or
              self._async_stacks or
              self._objects)

    def PendingEvents(self):
      return itertools.chain(
          (e for e in self._duration_stack),
          (o for o in self._objects),
          itertools.chain.from_iterable((
              (e for e in s) for s in self._async_stacks.itervalues())))

    def _AsyncKey(self, event):
      return (event.tracing_event['cat'], event.id)

    def _Ignore(self, _event):
      return None

    def _Unsupported(self, event):
      raise devtools_monitor.DevToolsConnectionException(
          'Unsupported spanning event type: %s' % event)

    def _DurationBegin(self, event):
      self._duration_stack.append(event)
      return None

    def _DurationEnd(self, event):
      if not self._duration_stack:
        raise devtools_monitor.DevToolsConnectionException(
            'Unmatched duration end: %s' % event)
      start = self._duration_stack.pop()
      start.SetClose(event)
      return start

    def _AsyncStart(self, event):
      key = self._AsyncKey(event)
      self._async_stacks.setdefault(key, []).append(event)
      return None

    def _AsyncEnd(self, event):
      key = self._AsyncKey(event)
      if key not in self._async_stacks:
        raise devtools_monitor.DevToolsConnectionException(
            'Unmatched async end %s: %s' % (key, event))
      stack = self._async_stacks[key]
      start = stack.pop()
      if not stack:
        del self._async_stacks[key]
      start.SetClose(event)
      return start

    def _ObjectCreated(self, event):
      # The tracing event format has object deletion timestamps being exclusive,
      # that is the timestamp for a deletion my equal that of the next create at
      # the same address. This asserts that does not happen in practice as it is
      # inconvenient to handle that correctly here.
      if event.id in self._objects:
        raise devtools_monitor.DevToolsConnectionException(
            'Multiple objects at same address: %s, %s' %
            (event, self._objects[event.id]))
      self._objects[event.id] = event
      return None

    def _ObjectDestroyed(self, event):
      if event.id not in self._objects:
        raise devtools_monitor.DevToolsConnectionException(
            'Missing object creation for %s' % event)
      start = self._objects[event.id]
      del self._objects[event.id]
      start.SetClose(event)
      return start

  class _EventList(object):
    def __init__(self, events):
      self._events = [e for e in events]
      if self._events:
        self._start_msec = min(e.start_msec for e in self._events)
        # Event end times may be changed after this list is created so the end
        # can't be cached.
      else:
        self._start_msec = self._end_msec = None

    @property
    def event_list(self):
      return self._events

    @property
    def start_msec(self):
      return self._start_msec

    @property
    def end_msec(self):
      return max(e.end_msec for e in self._events)

    def __nonzero__(self):
      return bool(self._events)


class Event(object):
  """Wraps a tracing event."""
  CLOSING_EVENTS = {'E': 'B',
                    'e': 'b',
                    'F': 'S',
                    'D': 'N'}
  def __init__(self, tracing_event, synthetic=False):
    """Creates Event.

    Intended to be created only by TracingTrack.

    Args:
      tracing_event: JSON tracing event, as defined in https://goo.gl/Qabkqk.
      synthetic: True if the event is synthetic. This is only used for indexing
        internal to TracingTrack.
    """
    if not synthetic and tracing_event['ph'] in ['s', 't', 'f']:
      raise devtools_monitor.DevToolsConnectionException(
          'Unsupported event: %s' % tracing_event)
    if not synthetic and tracing_event['ph'] in ['p']:
      raise devtools_monitor.DevToolsConnectionException(
          'Deprecated event: %s' % tracing_event)

    self._tracing_event = tracing_event
    # Note tracing event times are in microseconds.
    self._start_msec = tracing_event['ts'] / 1000.0
    self._end_msec = None
    self._synthetic = synthetic
    if self.type == 'X':
      # Some events don't have a duration.
      duration = (tracing_event['dur']
                  if 'dur' in tracing_event else tracing_event['tdur'])
      self._end_msec = self.start_msec + duration / 1000.0

  @property
  def start_msec(self):
    return self._start_msec

  @property
  def end_msec(self):
    return self._end_msec

  @property
  def type(self):
    if self._synthetic:
      return None
    return self._tracing_event['ph']

  @property
  def args(self):
    return self._tracing_event.get('args', {})

  @property
  def id(self):
    return self._tracing_event.get('id')

  @property
  def tracing_event(self):
    return self._tracing_event

  @property
  def synthetic(self):
    return self._synthetic

  def __str__(self):
    return ''.join([str(self._tracing_event),
                    '[%s,%s]' % (self.start_msec, self.end_msec)])

  def IsIndexable(self):
    """True iff the event can be indexed by time."""
    return self._synthetic or self.type not in [
        'I', 'P', 'c', 'C',
        'n', 'T', 'p',  # TODO(mattcary): ?? instant types of async events.
        'O',            # TODO(mattcary): ?? object snapshot
        ]

  def Synthesize(self):
    """Expand into synthetic events.

    Returns:
      A list of events, possibly some synthetic, whose start times are all
      interesting for purposes of indexing. If the event is not indexable the
      set may be empty.
    """
    if not self.IsIndexable():
      return []
    if self.type == 'X':
      # Tracing event timestamps are microseconds!
      return [self, Event({'ts': self.end_msec * 1000}, synthetic=True)]
    return [self]

  def SetClose(self, closing):
    """Close a spanning event.

    Args:
      closing: The closing event.

    Raises:
      devtools_monitor.DevToolsConnectionException if closing can't property
      close this event.
    """
    if self.type != self.CLOSING_EVENTS.get(closing.type):
      raise devtools_monitor.DevToolsConnectionException(
        'Bad closing: %s --> %s' % (self, closing))
    if self.type in ['b', 'S'] and (
        self.tracing_event['cat'] != closing.tracing_event['cat'] or
        self.id != closing.id):
      raise devtools_monitor.DevToolsConnectionException(
        'Bad async closing: %s --> %s' % (self, closing))
    self._end_msec = closing.start_msec
    if 'args' in closing.tracing_event:
      self.tracing_event.setdefault(
          'args', {}).update(closing.tracing_event['args'])

  def ToJsonDict(self):
    return self._tracing_event

  @classmethod
  def FromJsonDict(cls, json_dict):
    return Event(json_dict)
