# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os
import shutil
import subprocess
import tempfile
import unittest

import pull_sandwich_metrics as puller

_BLINK_CAT = 'blink.user_timing'
_MEM_CAT = 'disabled-by-default-memory-infra'
_START='requestStart'
_LOADS='loadEventStart'
_LOADE='loadEventEnd'
_UNLOAD='unloadEventEnd'

_MINIMALIST_TRACE = {'traceEvents': [
    {'cat': _BLINK_CAT, 'name': _UNLOAD, 'ts': 10, 'args': {'frame': '0'}},
    {'cat': _BLINK_CAT, 'name': _START,  'ts': 20, 'args': {},           },
    {'cat': _MEM_CAT,   'name': 'periodic_interval', 'pid': 1, 'ph': 'v',
        'args': {'dumps': {'allocators': {'malloc': {'attrs': {'size':{
            'units': 'bytes', 'value': '1af2', }}}}}}},
    {'cat': _BLINK_CAT, 'name': _LOADS,  'ts': 35, 'args': {'frame': '0'}},
    {'cat': _BLINK_CAT, 'name': _LOADE,  'ts': 40, 'args': {'frame': '0'}},
    {'cat': _MEM_CAT,   'name': 'periodic_interval', 'pid': 1, 'ph': 'v',
        'args': {'dumps': {'allocators': {'malloc': {'attrs': {'size':{
            'units': 'bytes', 'value': 'd704', }}}}}}},
    {'cat': '__metadata', 'pid': 1, 'name': 'process_name', 'args': {
        'name': 'Browser'}}]}


class PageTrackTest(unittest.TestCase):
  def testGetBrowserPID(self):
    def RunHelper(expected, trace):
      self.assertEquals(expected, puller._GetBrowserPID(trace))

    RunHelper(123, {'traceEvents': [
        {'pid': 354, 'cat': 'whatever0'},
        {'pid': 354, 'cat': 'whatever1'},
        {'pid': 354, 'cat': '__metadata', 'name': 'thread_name'},
        {'pid': 354, 'cat': '__metadata', 'name': 'process_name', 'args': {
            'name': 'Renderer'}},
        {'pid': 123, 'cat': '__metadata', 'name': 'process_name', 'args': {
            'name': 'Browser'}},
        {'pid': 354, 'cat': 'whatever0'}]})

    with self.assertRaises(ValueError):
      RunHelper(123, {'traceEvents': [
          {'pid': 354, 'cat': 'whatever0'},
          {'pid': 354, 'cat': 'whatever1'}]})

  def testGetBrowserDumpEvents(self):
    NAME = 'periodic_interval'

    def RunHelper(trace_events, browser_pid):
      trace_events = copy.copy(trace_events)
      trace_events.append({
          'pid': browser_pid,
          'cat': '__metadata',
          'name': 'process_name',
          'args': {'name': 'Browser'}})
      return puller._GetBrowserDumpEvents({'traceEvents': trace_events})

    TRACE_EVENTS = [
        {'pid': 354, 'ts':  1, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME},
        {'pid': 354, 'ts':  2, 'cat': _MEM_CAT, 'ph': 'V'},
        {'pid': 672, 'ts':  3, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME},
        {'pid': 123, 'ts':  4, 'cat': _MEM_CAT, 'ph': 'v', 'name': 'foo'},
        {'pid': 123, 'ts':  5, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME},
        {'pid': 123, 'ts':  6, 'cat': _MEM_CAT, 'ph': 'V'},
        {'pid': 672, 'ts':  7, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME},
        {'pid': 354, 'ts':  8, 'cat': _MEM_CAT, 'ph': 'v', 'name': 'foo'},
        {'pid': 123, 'ts':  9, 'cat': 'whatever1', 'ph': 'v', 'name': NAME},
        {'pid': 123, 'ts': 10, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME},
        {'pid': 354, 'ts': 11, 'cat': 'whatever0'},
        {'pid': 672, 'ts': 12, 'cat': _MEM_CAT, 'ph': 'v', 'name': NAME}]

    self.assertTrue(_MEM_CAT in puller.CATEGORIES)

    bump_events = RunHelper(TRACE_EVENTS, 123)
    self.assertEquals(2, len(bump_events))
    self.assertEquals(5, bump_events[0]['ts'])
    self.assertEquals(10, bump_events[1]['ts'])

    bump_events = RunHelper(TRACE_EVENTS, 354)
    self.assertEquals(1, len(bump_events))
    self.assertEquals(1, bump_events[0]['ts'])

    bump_events = RunHelper(TRACE_EVENTS, 672)
    self.assertEquals(3, len(bump_events))
    self.assertEquals(3, bump_events[0]['ts'])
    self.assertEquals(7, bump_events[1]['ts'])
    self.assertEquals(12, bump_events[2]['ts'])

    with self.assertRaises(ValueError):
      RunHelper(TRACE_EVENTS, 895)

  def testGetWebPageTrackedEvents(self):
    self.assertTrue(_BLINK_CAT in puller.CATEGORIES)

    trace_events = puller._GetWebPageTrackedEvents({'traceEvents': [
        {'ts':  0, 'args': {},             'cat': 'whatever', 'name': _START},
        {'ts':  1, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADS},
        {'ts':  2, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADE},
        {'ts':  3, 'args': {},             'cat': _BLINK_CAT, 'name': _START},
        {'ts':  4, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADS},
        {'ts':  5, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADE},
        {'ts':  6, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _UNLOAD},
        {'ts':  7, 'args': {},             'cat': _BLINK_CAT, 'name': _START},
        {'ts':  8, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADS},
        {'ts':  9, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADE},
        {'ts': 10, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _UNLOAD},
        {'ts': 11, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _START},
        {'ts': 12, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADS},
        {'ts': 13, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADE},
        {'ts': 14, 'args': {},             'cat': _BLINK_CAT, 'name': _START},
        {'ts': 15, 'args': {},             'cat': _BLINK_CAT, 'name': _START},
        {'ts': 16, 'args': {'frame': '1'}, 'cat': _BLINK_CAT, 'name': _LOADS},
        {'ts': 17, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADS},
        {'ts': 18, 'args': {'frame': '1'}, 'cat': _BLINK_CAT, 'name': _LOADE},
        {'ts': 19, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADE},
        {'ts': 20, 'args': {},             'cat': 'whatever', 'name': _START},
        {'ts': 21, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADS},
        {'ts': 22, 'args': {'frame': '0'}, 'cat': 'whatever', 'name': _LOADE},
        {'ts': 23, 'args': {},             'cat': _BLINK_CAT, 'name': _START},
        {'ts': 24, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADS},
        {'ts': 25, 'args': {'frame': '0'}, 'cat': _BLINK_CAT, 'name': _LOADE}]})

    self.assertEquals(3, len(trace_events))
    self.assertEquals(14, trace_events['requestStart']['ts'])
    self.assertEquals(17, trace_events['loadEventStart']['ts'])
    self.assertEquals(19, trace_events['loadEventEnd']['ts'])

  def testPullMetricsFromTrace(self):
    metrics = puller._PullMetricsFromTrace(_MINIMALIST_TRACE)
    self.assertEquals(4, len(metrics))
    self.assertEquals(20, metrics['total_load'])
    self.assertEquals(5, metrics['onload'])
    self.assertEquals(30971, metrics['browser_malloc_avg'])
    self.assertEquals(55044, metrics['browser_malloc_max'])

  def testCommandLine(self):
    tmp_dir = tempfile.mkdtemp()
    for dirname in ['1', '2', 'whatever']:
      os.mkdir(os.path.join(tmp_dir, dirname))
      with open(os.path.join(tmp_dir, dirname, 'trace.json'), 'w') as out_file:
        json.dump(_MINIMALIST_TRACE, out_file)

    process = subprocess.Popen(['python', puller.__file__, tmp_dir])
    process.wait()
    shutil.rmtree(tmp_dir)

    self.assertEquals(0, process.returncode)


if __name__ == '__main__':
  unittest.main()
