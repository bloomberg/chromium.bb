#! /usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Drive TracingConnection"""

import argparse
import json
import logging
import os.path
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import device_setup
import page_track
import tracing


def main():
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--url', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args()
  url = args.url
  if not url.startswith('http'):
    url = 'http://' + url
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  with file(args.output, 'w') as output, \
       file(args.output + '.page', 'w') as page_output, \
       device_setup.DeviceConnection(device) as connection:
    track = tracing.TracingTrack(connection, fetch_stream=False)
    page = page_track.PageTrack(connection)
    connection.SetUpMonitoring()
    connection.SendAndIgnoreResponse('Page.navigate', {'url': url})
    connection.StartMonitoring()
    json.dump(page.GetEvents(), page_output, sort_keys=True, indent=2)
    json.dump(track.GetEvents(), output, sort_keys=True, indent=2)


if __name__ == '__main__':
  main()
