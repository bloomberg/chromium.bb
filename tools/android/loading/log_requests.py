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
import time

file_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(file_dir, '..', '..', '..', 'build', 'android'))
sys.path.append(os.path.join(file_dir, '..', '..', 'telemetry'))
sys.path.append(os.path.join(file_dir, '..', '..', 'chrome_proxy'))

from pylib import constants
from pylib import flag_changer
from pylib.device import device_utils
from pylib.device import intent
from common import inspector_network
from telemetry.internal.backends.chrome_inspector import inspector_websocket
from telemetry.internal.backends.chrome_inspector import websocket


_PORT = 9222 # DevTools port number.


@contextlib.contextmanager
def FlagChanger(device, command_line_path, new_flags):
  """Changes the flags in a context, restores them afterwards.

  Args:
    device: Device to target, from DeviceUtils.
    command_line_path: Full path to the command-line file.
    new_flags: Flags to add.
  """
  changer = flag_changer.FlagChanger(device, command_line_path)
  changer.AddFlags(new_flags)
  try:
    yield
  finally:
    changer.Restore()


@contextlib.contextmanager
def ForwardPort(device, local, remote):
  """Forwards a local port to a remote one on a device in a context."""
  device.adb.Forward(local, remote)
  try:
    yield
  finally:
    device.adb.ForwardRemove(local)


def _SetUpDevice(device, package_info):
  """Enables root and closes Chrome on a device."""
  device.EnableRoot()
  device.KillAll(package_info.package, quiet=True)


class AndroidRequestsLogger(object):
  """Logs all the requests made to load a page on a device."""

  def __init__(self, device):
    self.device = device
    self._please_stop = False
    self._main_frame_id = None

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

  def _LogPageLoadInternal(self, url, clear_cache):
    """Returns the collection of requests made to load a given URL.

    Assumes that DevTools is available on http://localhost:_PORT.

    Args:
      url: URL to load.
      clear_cache: Whether to clear the HTTP cache.

    Returns:
      [inspector_network.InspectorNetworkResponseData, ...]
    """
    self._main_frame_id = None
    self._please_stop = False
    r = httplib.HTTPConnection('localhost', _PORT)
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
    inspector.StopMonitoringNetwork()
    return inspector.GetResponseData()

  def LogPageLoad(self, url, clear_cache):
    """Returns the collection of requests made to load a given URL on a device.

    Args:
      url: (str) URL to load on the device.
      clear_cache: (bool) Whether to clear the HTTP cache.

    Returns:
      See _LogPageLoadInternal().
    """
    package_info = constants.PACKAGE_INFO['chrome']
    command_line_path = '/data/local/chrome-command-line'
    new_flags = ['--enable-test-events', '--remote-debugging-port=%d' % _PORT]
    _SetUpDevice(self.device, package_info)
    with FlagChanger(self.device, command_line_path, new_flags):
      start_intent = intent.Intent(
          package=package_info.package, activity=package_info.activity,
          data='about:blank')
      self.device.StartActivity(start_intent, blocking=True)
      time.sleep(2)
      with ForwardPort(self.device, 'tcp:%d' % _PORT,
                       'localabstract:chrome_devtools_remote'):
        return self._LogPageLoadInternal(url, clear_cache)


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
  return parser


def main():
  parser = _CreateOptionParser()
  options, _ = parser.parse_args()
  devices = device_utils.DeviceUtils.HealthyDevices()
  device = devices[0]
  request_logger = AndroidRequestsLogger(device)
  response_data = request_logger.LogPageLoad(options.url, options.clear_cache)
  json_data = _ResponseDataToJson(response_data)
  with open(options.output, 'w') as f:
    f.write(json_data)


if __name__ == '__main__':
  main()
