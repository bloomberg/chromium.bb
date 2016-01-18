#! /usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Loading trace recorder."""

import os
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import devil_chromium

import device_setup
import devtools_monitor
import page_track

class AndroidTraceRecorder(object):
  """Records a loading trace."""
  def __init__(self, url):
    self.url = url
    self.devtools_connection = None
    self.page_track = None

  def Go(self, connection):
    self.devtools_connection = connection
    self.page_track = page_track.PageTrack(self.devtools_connection)
    self.devtools_connection.SetUpMonitoring()
    self.devtools_connection.SendAndIgnoreResponse(
        'Page.navigate', {'url': self.url})
    self.devtools_connection.StartMonitoring()
    print self.page_track.GetEvents()


def DoIt(url):
  devil_chromium.Initialize()
  devices = device_utils.DeviceUtils.HealthyDevices()
  device = devices[0]
  trace_recorder = AndroidTraceRecorder(url)
  device_setup.SetUpAndExecute(device, 'chrome', trace_recorder.Go)


if __name__ == '__main__':
  DoIt(sys.argv[1])
