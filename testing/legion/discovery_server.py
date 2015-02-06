# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The discovery server used to register clients.

The discovery server is started by the host controller and allows the clients
to register themselves when they start. Authentication of the client controllers
is based on an OTP passed to the client controller binary on startup.
"""

import logging
import threading
import xmlrpclib
import SimpleXMLRPCServer

#pylint: disable=relative-import
import common_lib


class DiscoveryServer(object):
  """Discovery server run on the host."""

  def __init__(self):
    self._expected_clients = {}
    self._rpc_server = None
    self._thread = None

  def _RegisterClientRPC(self, otp, ip):
    """The RPC used by a client to register with the discovery server."""
    assert otp in self._expected_clients
    cb = self._expected_clients.pop(otp)
    cb(ip)

  def RegisterClientCallback(self, otp, callback):
    """Registers a callback associated with an OTP."""
    assert callable(callback)
    self._expected_clients[otp] = callback

  def Start(self):
    """Starts the discovery server."""
    logging.debug('Starting discovery server')
    self._rpc_server = SimpleXMLRPCServer.SimpleXMLRPCServer(
        (common_lib.SERVER_ADDRESS, common_lib.SERVER_PORT),
        allow_none=True, logRequests=False)
    self._rpc_server.register_function(
        self._RegisterClientRPC, 'RegisterClient')
    self._thread = threading.Thread(target=self._rpc_server.serve_forever)
    self._thread.start()

  def Shutdown(self):
    """Shuts the discovery server down."""
    if self._thread and self._thread.is_alive():
      logging.debug('Shutting down discovery server')
      self._rpc_server.shutdown()
