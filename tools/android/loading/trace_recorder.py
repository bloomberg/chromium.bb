#! /usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Loading trace recorder."""

import os
import sys

file_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(file_dir, '..', '..', '..', 'build', 'android'))

from pylib.device import device_utils

import device_setup
import devtools_monitor


class PageTrack(devtools_monitor.Track):
  """Records the events from the page track."""
  def __init__(self, connection):
    self._connection = connection
    self._events = []
    self._main_frame_id = None
    self._connection.RegisterListener('Page.frameStartedLoading', self)
    self._connection.RegisterListener('Page.frameStoppedLoading', self)

  def Handle(self, method, msg):
    params = msg['params']
    frame_id = params['frameId']
    should_stop = False
    if method == 'Page.frameStartedLoading' and self._main_frame_id is None:
      self._main_frame_id = params['frameId']
    elif (method == 'Page.frameStoppedLoading'
          and params['frameId'] == self._main_frame_id):
      should_stop = True
    self._events.append((method, frame_id))
    if should_stop:
      self._connection.StopMonitoring()

  def GetEvents(self):
    return self._events


class AndroidTraceRecorder(object):
  """Records a loading trace."""
  def __init__(self, url):
    self.url = url

  def Go(self):
    self.devtools_connection = devtools_monitor.DevToolsConnection(
        device_setup.DEVTOOLS_HOSTNAME, device_setup.DEVTOOLS_PORT)
    self.page_track = PageTrack(self.devtools_connection)

    self.devtools_connection.SetUpMonitoring()
    self.devtools_connection.SendAndIgnoreResponse(
        'Page.navigate', {'url': self.url})
    self.devtools_connection.StartMonitoring()
    print self.page_track.GetEvents()


def DoIt(url):
  devices = device_utils.DeviceUtils.HealthyDevices()
  device = devices[0]
  trace_recorder = AndroidTraceRecorder(url)
  device_setup.SetUpAndExecute(device, 'chrome', trace_recorder.Go)


if __name__ == '__main__':
  DoIt(sys.argv[1])
