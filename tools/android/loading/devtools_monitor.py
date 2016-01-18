# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library handling DevTools websocket interaction.
"""

import httplib
import json
import logging
import os
import sys

file_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(file_dir, '..', '..', 'telemetry'))

from telemetry.internal.backends.chrome_inspector import inspector_websocket
from telemetry.internal.backends.chrome_inspector import websocket


class DevToolsConnectionException(Exception):
  def __init__(self, message):
    super(DevToolsConnectionException, self).__init__(message)
    logging.warning("DevToolsConnectionException: " + message)


class DevToolsConnection(object):
  """Handles the communication with a DevTools server.
  """
  def __init__(self, hostname, port):
    """Initializes the connection with a DevTools server.

    Args:
      hostname: server hostname.
      port: port number.
    """
    self._ws = self._Connect(hostname, port)
    self._listeners = {}
    self._domains_to_enable = set()
    self._please_stop = False

  def RegisterListener(self, name, listener):
    """Registers a listener for an event.

    Also takes care of enabling the relevant domain before starting monitoring.

    Args:
      name: (str) Event the listener wants to listen to, e.g.
            Network.requestWillBeSent.
      listener: (Listener) listener instance.
    """
    domain = name[:name.index('.')]
    self._listeners[name] = listener
    self._domains_to_enable.add(domain)

  def UnregisterListener(self, listener):
    """Unregisters a listener.

    Args:
      listener: (Listener) listener to unregister.
    """
    keys = [k for (k, v) in self._listeners if v is listener]
    assert keys, "Removing non-existent listener"
    for key in keys:
      del(self._listeners[key])

  def SyncRequest(self, method, params=None):
    """Issues a synchronous request to the DevTools server.

    Args:
      method: (str) Method.
      params: (dict) Optional parameters to the request.

    Returns:
      The answer.
    """
    request = {'method': method}
    if params:
      request['params'] = params
    return self._ws.SyncRequest(request)

  def SendAndIgnoreResponse(self, method, params=None):
    """Issues a request to the DevTools server, do not wait for the response.

    Args:
      method: (str) Method.
      params: (dict) Optional parameters to the request.
    """
    request = {'method': method}
    if params:
      request['params'] = params
    self._ws.SendAndIgnoreResponse(request)

  def SetUpMonitoring(self):
    for domain in self._domains_to_enable:
      self._ws.RegisterDomain(domain, self._OnDataReceived)
      self.SyncRequest('%s.enable' % domain)

  def StartMonitoring(self):
    """Starts monitoring.

    DevToolsConnection.SetUpMonitoring() has to be called first.
    """
    while not self._please_stop:
      try:
        self._ws.DispatchNotifications()
      except websocket.WebSocketTimeoutException:
        break
    if not self._please_stop:
      logging.warning('Monitoring stopped on a timeout.')
    self._TearDownMonitoring()

  def StopMonitoring(self):
    """Stops the monitoring."""
    self._please_stop = True

  def _TearDownMonitoring(self):
    for domain in self._domains_to_enable:
      self.SyncRequest('%s.disable' % domain)
      self._ws.UnregisterDomain(domain)
    self._domains_to_enable.clear()
    self._listeners.clear()

  def _OnDataReceived(self, msg):
    method = msg.get('method', None)
    if method not in self._listeners:
      return
    self._listeners[method].Handle(method, msg)

  @classmethod
  def _GetWebSocketUrl(cls, hostname, port):
    r = httplib.HTTPConnection(hostname, port)
    r.request('GET', '/json')
    response = r.getresponse()
    if response.status != 200:
      raise DevToolsConnectionException(
          'Cannot connect to DevTools, reponse code %d' % response.status)
    json_response = json.loads(response.read())
    r.close()
    websocket_url = json_response[0]['webSocketDebuggerUrl']
    return websocket_url

  @classmethod
  def _Connect(cls, hostname, port):
    websocket_url = cls._GetWebSocketUrl(hostname, port)
    ws = inspector_websocket.InspectorWebsocket()
    ws.Connect(websocket_url)
    return ws


class Listener(object):
  """Listens to events forwarded by a DevToolsConnection instance."""
  def __init__(self, connection):
    """Initializes a Listener instance.

    Args:
      connection: (DevToolsConnection).
    """
    pass

  def Handle(self, method, msg):
    """Handles an event this instance listens for.

    Args:
      event_name: (str) Event name, as registered.
      event: (dict) complete event.
    """
    pass


class Track(Listener):
  """Collects data from a DevTools server."""
  def GetEvents(self):
    """Returns a list of collected events, finalizing the state if necessary."""
    pass

  def ToJsonDict(self):
    """Serializes to a dictionary, to be dumped as JSON.

    Returns:
      A dict that can be dumped by the json module, and loaded by
      FromJsonDict().
    """
    pass

  @classmethod
  def FromJsonDict(cls, json_dict):
    """Returns a Track instance constructed from data dumped by
       Track.ToJsonDict().

    Args:
      json_data: (dict) Parsed from a JSON file using the json module.

    Returns:
      a Track instance.
    """
    pass
