#! /usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Loads a URL on an Android device, logging all the requests made to do it
to a JSON file using DevTools.
"""

import contextlib
import httplib
import json
import logging
import optparse
import os
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import devil_chromium

sys.path.append(os.path.join(_SRC_DIR, 'tools', 'perf'))
from chrome_telemetry_build import chromium_config
sys.path.append(chromium_config.GetTelemetryDir())
from telemetry.internal.backends.chrome_inspector import inspector_websocket
from telemetry.internal.backends.chrome_inspector import websocket

sys.path.append(os.path.join(_SRC_DIR, 'tools', 'chrome_proxy'))
from common import inspector_network

import device_setup


class AndroidRequestsLogger(object):
  """Logs all the requests made to load a page on a device."""

  def __init__(self, device):
    """If device is None, we connect to a local chrome session."""
    self.device = device
    self._please_stop = False
    self._main_frame_id = None
    self._tracing_data = []

  def _PageDataReceived(self, msg):
    """Called when a Page event is received.

    Records the main frame, and stops the recording once it has finished
    loading.

    Args:
      msg: (dict) Message sent by DevTools.
    """
    if 'params' not in msg:
      return
    params = msg['params']
    method = msg.get('method', None)
    if method == 'Page.frameStartedLoading' and self._main_frame_id is None:
      self._main_frame_id = params['frameId']
    elif (method == 'Page.frameStoppedLoading'
          and params['frameId'] == self._main_frame_id):
      self._please_stop = True

  def _TracingDataReceived(self, msg):
    self._tracing_data.append(msg)

  def _LogPageLoadInternal(self, url, clear_cache):
    """Returns the collection of requests made to load a given URL.

    Assumes that DevTools is available on http://localhost:DEVTOOLS_PORT.

    Args:
      url: URL to load.
      clear_cache: Whether to clear the HTTP cache.

    Returns:
      [inspector_network.InspectorNetworkResponseData, ...]
    """
    self._main_frame_id = None
    self._please_stop = False
    r = httplib.HTTPConnection(
        device_setup.DEVTOOLS_HOSTNAME, device_setup.DEVTOOLS_PORT)
    r.request('GET', '/json')
    response = r.getresponse()
    if response.status != 200:
      logging.error('Cannot connect to the remote target.')
      return None
    json_response = json.loads(response.read())
    r.close()
    websocket_url = json_response[0]['webSocketDebuggerUrl']
    ws = inspector_websocket.InspectorWebsocket()
    ws.Connect(websocket_url)
    inspector = inspector_network.InspectorNetwork(ws)
    if clear_cache:
      inspector.ClearCache()
    ws.SyncRequest({'method': 'Page.enable'})
    ws.RegisterDomain('Page', self._PageDataReceived)
    inspector.StartMonitoringNetwork()
    ws.SendAndIgnoreResponse({'method': 'Page.navigate',
                              'params': {'url': url}})
    while not self._please_stop:
      try:
        ws.DispatchNotifications()
      except websocket.WebSocketTimeoutException as e:
        logging.warning('Exception: ' + str(e))
        break
    if not self._please_stop:
      logging.warning('Finished with timeout instead of page load')
    inspector.StopMonitoringNetwork()
    return inspector.GetResponseData()

  def _LogTracingInternal(self, url):
    self._main_frame_id = None
    self._please_stop = False
    r = httplib.HTTPConnection('localhost', device_setup.DEVTOOLS_PORT)
    r.request('GET', '/json')
    response = r.getresponse()
    if response.status != 200:
      logging.error('Cannot connect to the remote target.')
      return None
    json_response = json.loads(response.read())
    r.close()
    websocket_url = json_response[0]['webSocketDebuggerUrl']
    ws = inspector_websocket.InspectorWebsocket()
    ws.Connect(websocket_url)
    ws.RegisterDomain('Tracing', self._TracingDataReceived)
    logging.warning('Tracing.start: ' +
                    str(ws.SyncRequest({'method': 'Tracing.start',
                                        'options': 'zork'})))
    ws.SendAndIgnoreResponse({'method': 'Page.navigate',
                              'params': {'url': url}})
    while not self._please_stop:
      try:
        ws.DispatchNotifications()
      except websocket.WebSocketTimeoutException:
        break
    if not self._please_stop:
      logging.warning('Finished with timeout instead of page load')
    return {'events': self._tracing_data,
            'end': ws.SyncRequest({'method': 'Tracing.end'})}


  def LogPageLoad(self, url, clear_cache, package):
    """Returns the collection of requests made to load a given URL on a device.

    Args:
      url: (str) URL to load on the device.
      clear_cache: (bool) Whether to clear the HTTP cache.

    Returns:
      See _LogPageLoadInternal().
    """
    return device_setup.SetUpAndExecute(
        self.device, package,
        lambda: self._LogPageLoadInternal(url, clear_cache))

  def LogTracing(self, url):
    """Log tracing events from a load of the given URL.

    TODO(mattcary): This doesn't work. It would be best to log tracing
    simultaneously with network requests, but as that wasn't working the tracing
    logging was broken out separately. It still doesn't work...
    """
    return device_setup.SetUpAndExecute(
        self.device, 'chrome', lambda: self._LogTracingInternal(url))


def _ResponseDataToJson(data):
  """Converts a list of inspector_network.InspectorNetworkResponseData to JSON.

  Args:
    data: as returned by _LogPageLoad()

  Returns:
    A JSON file with the following format:
    [request1, request2, ...], and a request is:
    {'status': str, 'headers': dict, 'request_headers': dict,
     'timestamp': double, 'timing': dict, 'url': str,
      'served_from_cache': bool, 'initiator': str})
  """
  result = []
  for r in data:
    result.append({'status': r.status,
                   'headers': r.headers,
                   'request_headers': r.request_headers,
                   'timestamp': r.timestamp,
                   'timing': r.timing,
                   'url': r.url,
                   'served_from_cache': r.served_from_cache,
                   'initiator': r.initiator})
  return json.dumps(result)


def _CreateOptionParser():
  """Returns the option parser for this tool."""
  parser = optparse.OptionParser(description='Starts a browser on an Android '
                                 'device, gathers the requests made to load a '
                                 'page and dumps it to a JSON file.')
  parser.add_option('--url', help='URL to load.',
                    default='https://www.google.com', metavar='URL')
  parser.add_option('--output', help='Output file.', default='result.json')
  parser.add_option('--no-clear-cache', help=('Do not clear the HTTP cache '
                                              'before loading the URL.'),
                    default=True, action='store_false', dest='clear_cache')
  parser.add_option('--package', help='Package info for chrome build. '
                                      'See build/android/pylib/constants.',
                    default='chrome')
  parser.add_option('--local', action='store_true', default=False,
                    help='Connect to local chrome session rather than android.')
  return parser


def main():
  logging.basicConfig(level=logging.WARNING)
  parser = _CreateOptionParser()
  options, _ = parser.parse_args()

  devil_chromium.Initialize()

  if options.local:
    device = None
  else:
    devices = device_utils.DeviceUtils.HealthyDevices()
    device = devices[0]

  request_logger = AndroidRequestsLogger(device)
  response_data = request_logger.LogPageLoad(
      options.url, options.clear_cache, options.package)
  json_data = _ResponseDataToJson(response_data)
  with open(options.output, 'w') as f:
    f.write(json_data)


if __name__ == '__main__':
  main()
