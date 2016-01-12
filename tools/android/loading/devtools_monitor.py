# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library handling DevTools websocket interaction.
"""

class DevToolsConnection(object):
  """Handles the communication with a DevTools server.
  """
  def __init__(self, hostname, port):
    """Initializes the connection with a DevTools server.

    Args:
      hostname: server hostname.
      port: port number.
    """
    pass

  def RegisterListener(self, name, listener):
    """Registers a listener for an event.

    Also takes care of enabling the relevant domain before starting monitoring.

    Args:
      name: (str) Event the listener wants to listen to, e.g.
            Network.requestWillBeSent.
      listener: (Listener) listener instance.
    """
    pass

  def UnregisterListener(self, listener):
    """Unregisters a listener.

    Args:
      listener: (Listener) listener to unregister.
    """
    pass

  def SyncRequest(self, method, params=None):
    """Issues a synchronous request to the DevTools server.

    Args:
      method: (str) Method.
      params: (dict) Optional parameters to the request.

    Returns:
      The answer.
    """
    pass

  def SendAndIgnoreResponse(self, method, params=None):
    """Issues a request to the DevTools server, do not wait for the response.

    Args:
      method: (str) Method.
      params: (dict) Optional parameters to the request.
    """
    pass

  def StartMonitoring(self):
    """Starts monitoring, enabling the relevant domains first."""
    pass

  def Stop(self):
    """Stops the monitoring."""
    pass


class Listener(object):
  """Listens to events forwarded by a DevToolsConnection instance."""
  def __init__(self, connection):
    """Initializes a Listener instance.

    Args:
      connection: (DevToolsConnection).
    """
    pass

  def Handle(self, event_name, event):
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
