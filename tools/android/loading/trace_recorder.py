#! /usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Loading trace recorder. DEPRECATED!"""

import argparse
import datetime
import json
import logging
import os
import sys
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import devil_chromium

import device_setup
import devtools_monitor
import loading_trace
import page_track
import request_track
import tracing


def MonitorUrl(connection, url, clear_cache=False,
               categories=tracing.DEFAULT_CATEGORIES,
               timeout=devtools_monitor.DEFAULT_TIMEOUT_SECONDS):
  """Monitor a URL via a trace recorder.

  DEPRECATED! Use LoadingTrace.FromUrlAndController instead.

  Args:
    connection: A devtools_monitor.DevToolsConnection instance.
    url: url to navigate to as string.
    clear_cache: boolean indicating if cache should be cleared before loading.
    categories: List of tracing event categories to record.
    timeout: Websocket timeout.

  Returns:
    loading_trace.LoadingTrace.
  """
  page = page_track.PageTrack(connection)
  request = request_track.RequestTrack(connection)
  trace = tracing.TracingTrack(connection, categories=categories)
  connection.SetUpMonitoring()
  if clear_cache:
    connection.ClearCache()
  connection.SendAndIgnoreResponse('Page.navigate', {'url': url})
  connection.StartMonitoring(timeout=timeout)
  metadata = {'date': datetime.datetime.utcnow().isoformat(),
              'seconds_since_epoch': time.time()}
  return loading_trace.LoadingTrace(url, metadata, page, request, trace)

def RecordAndDumpTrace(device, url, output_filename):
  with file(output_filename, 'w') as output,\
        device_setup.DeviceConnection(device) as connection:
    trace = MonitorUrl(connection, url)
    json.dump(trace.ToJsonDict(), output)


def main():
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  parser = argparse.ArgumentParser()
  parser.add_argument('--url', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args()
  url = args.url
  if not url.startswith('http'):
    url = 'http://' + url
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  RecordAndDumpTrace(device, url, args.output)


if __name__ == '__main__':
  main()
