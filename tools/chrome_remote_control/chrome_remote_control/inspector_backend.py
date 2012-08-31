# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import time
import websocket
import socket

class InspectorException(Exception):
  pass

class InspectorBackend(object):
  def __init__(self, backend, descriptor):
    self._backend = backend
    self._descriptor = descriptor
    self._socket = websocket.create_connection(
        descriptor["webSocketDebuggerUrl"]);
    self._socket.settimeout(0.5)
    self._next_request_id = 0
    self._domain_handlers = {}

  def Close(self):
    for domain, handlers in self._domain_handlers.items():
      notification_handler, will_close_handler = handlers
      will_close_handler()
    self._domain_handlers = {}
    self._socket.close()
    self._socket = None
    self._backend = None

  def SyncRequest(self, req):
    req["id"] = self._next_request_id
    self._next_request_id += 1
    self._socket.send(json.dumps(req))
    while True:
      try:
        data = self._socket.recv()
      except socket.error:
        req["id"] = self._next_request_id
        self._next_request_id += 1
        self._socket.send(json.dumps(req))
        continue
      res = json.loads(data)
      if "method" in res:
        mname = res["method"]
        dot_pos = mname.find(".")
        domain_name = mname[:dot_pos]
        if domain_name in self._domain_handlers:
          try:
            self._domain_handlers[domain_name][0](res)
          except:
            import traceback
            traceback.print_exc()
        continue

      if res["id"] != req["id"]:
        continue
      return res

  def RegisterDomain(self,
      domain_name, notification_handler, will_close_handler):
    """Registers a given domain for handling notification methods.

    For example, given inspector_backend:
       def OnConsoleNotification(msg):
          if msg["method"] == "Console.messageAdded":
             print msg["params"]["message"]
          return
       def OnConsoleClose(self):
          pass
       inspector_backend.RegisterDomain("Console",
                                        OnConsoleNotification, OnConsoleClose)
       """
    assert domain_name not in self._domain_handlers
    self._domain_handlers[domain_name] = (notification_handler,
                                          will_close_handler)

