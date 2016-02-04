# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from activity_lens import ActivityLens
import tracing


class ActivityLensTestCast(unittest.TestCase):
  @classmethod
  def _EventsFromRawEvents(cls, raw_events):
    tracing_track = tracing.TracingTrack(None)
    tracing_track.Handle(
        'Tracing.dataCollected', {'params': {'value': raw_events}})
    return tracing_track.GetEvents()

  def setUp(self):
    self.tracing_track = tracing.TracingTrack(None)

  def testGetRendererMainThread(self):
    first_renderer_tid = 12345
    second_renderer_tid = 123456
    raw_events =  [
        {u'args': {u'name': u'CrBrowserMain'},
         u'cat': u'__metadata',
         u'name': u'thread_name',
         u'ph': u'M',
         u'pid': 123,
         u'tid': 123,
         u'ts': 0},
        {u'args': {u'name': u'CrRendererMain'},
         u'cat': u'__metadata',
         u'name': u'thread_name',
         u'ph': u'M',
         u'pid': 1234,
         u'tid': first_renderer_tid,
         u'ts': 0},
        {u'args': {u'name': u'CrRendererMain'},
         u'cat': u'__metadata',
         u'name': u'thread_name',
         u'ph': u'M',
         u'pid': 12345,
         u'tid': second_renderer_tid,
         u'ts': 0}]
    raw_events += [
        {u'args': {u'data': {}},
         u'cat': u'devtools.timeline,v8',
         u'name': u'FunctionCall',
         u'ph': u'X',
         u'pid': 32723,
         u'tdur': 0,
         u'tid': first_renderer_tid,
         u'ts': 251427174674,
         u'tts': 5107725}] * 100
    raw_events += [
        {u'args': {u'data': {}},
         u'cat': u'devtools.timeline,v8',
         u'name': u'FunctionCall',
         u'ph': u'X',
         u'pid': 1234,
         u'tdur': 0,
         u'tid': second_renderer_tid,
         u'ts': 251427174674,
         u'tts': 5107725}] * 150
    events = self._EventsFromRawEvents(raw_events)
    self.assertEquals(second_renderer_tid,
                      ActivityLens._GetRendererMainThreadId(events))

  def testThreadBusiness(self):
    raw_events =  [
        {u'args': {},
         u'cat': u'toplevel',
         u'dur': 200 * 1000,
         u'name': u'MessageLoop::RunTask',
         u'ph': u'X',
         u'pid': 123,
         u'tid': 123,
         u'ts': 0,
         u'tts': 56485},
        {u'args': {},
         u'cat': u'toplevel',
         u'dur': 8 * 200,
         u'name': u'MessageLoop::NestedSomething',
         u'ph': u'X',
         u'pid': 123,
         u'tid': 123,
         u'ts': 0,
         u'tts': 0}]
    events = self._EventsFromRawEvents(raw_events)
    self.assertEquals(200, ActivityLens._ThreadBusiness(events, 0, 1000))
    # Clamping duration.
    self.assertEquals(100, ActivityLens._ThreadBusiness(events, 0, 100))
    self.assertEquals(50, ActivityLens._ThreadBusiness(events, 25, 75))

  def testScriptExecuting(self):
    url = u'http://example.com/script.js'
    raw_events = [
        {u'args': {u'data': {u'scriptName': url}},
         u'cat': u'devtools.timeline,v8',
         u'dur': 250 * 1000,
         u'name': u'FunctionCall',
         u'ph': u'X',
         u'pid': 123,
         u'tdur': 247,
         u'tid': 123,
         u'ts': 0,
         u'tts': 0},
        {u'args': {u'data': {}},
         u'cat': u'devtools.timeline,v8',
         u'dur': 350 * 1000,
         u'name': u'EvaluateScript',
         u'ph': u'X',
         u'pid': 123,
         u'tdur': 247,
         u'tid': 123,
         u'ts': 0,
         u'tts': 0}]
    events = self._EventsFromRawEvents(raw_events)
    self.assertEquals(2, len(ActivityLens._ScriptsExecuting(events, 0, 1000)))
    self.assertTrue(None in ActivityLens._ScriptsExecuting(events, 0, 1000))
    self.assertEquals(
        350, ActivityLens._ScriptsExecuting(events, 0, 1000)[None])
    self.assertTrue(url in ActivityLens._ScriptsExecuting(events, 0, 1000))
    self.assertEquals(250, ActivityLens._ScriptsExecuting(events, 0, 1000)[url])
    # Aggreagates events.
    raw_events.append({u'args': {u'data': {}},
                       u'cat': u'devtools.timeline,v8',
                       u'dur': 50 * 1000,
                       u'name': u'EvaluateScript',
                       u'ph': u'X',
                       u'pid': 123,
                       u'tdur': 247,
                       u'tid': 123,
                       u'ts': 0,
                       u'tts': 0})
    events = self._EventsFromRawEvents(raw_events)
    self.assertEquals(
        350 + 50, ActivityLens._ScriptsExecuting(events, 0, 1000)[None])

  def testParsing(self):
    css_url = u'http://example.com/style.css'
    html_url = u'http://example.com/yeah.htnl'
    raw_events = [
        {u'args': {u'data': {u'styleSheetUrl': css_url}},
         u'cat': u'blink,devtools.timeline',
         u'dur': 400 * 1000,
         u'name': u'ParseAuthorStyleSheet',
         u'ph': u'X',
         u'pid': 32723,
         u'tdur': 49721,
         u'tid': 32738,
         u'ts': 0,
         u'tts': 216148},
        {u'args': {u'beginData': {u'url': html_url}},
         u'cat': u'devtools.timeline',
         u'dur': 42 * 1000,
         u'name': u'ParseHTML',
         u'ph': u'X',
         u'pid': 32723,
         u'tdur': 49721,
         u'tid': 32738,
         u'ts': 0,
         u'tts': 5032310},]
    events = self._EventsFromRawEvents(raw_events)
    self.assertEquals(2, len(ActivityLens._Parsing(events, 0, 1000)))
    self.assertTrue(css_url in ActivityLens._Parsing(events, 0, 1000))
    self.assertEquals(400, ActivityLens._Parsing(events, 0, 1000)[css_url])
    self.assertTrue(html_url in ActivityLens._Parsing(events, 0, 1000))
    self.assertEquals(42, ActivityLens._Parsing(events, 0, 1000)[html_url])


if __name__ == '__main__':
  unittest.main()
