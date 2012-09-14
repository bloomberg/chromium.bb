# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import socket

from chrome_remote_control import websocket

class InspectorException(Exception):
  pass

class InspectorBackend(object):
  def __init__(self, backend, descriptor):
    self._backend = backend
    self._descriptor = descriptor
    self._socket = websocket.create_connection(
        descriptor['webSocketDebuggerUrl'])
    self._next_request_id = 0
    self._domain_handlers = {}

  def Close(self):
    for _, handlers in self._domain_handlers.items():
      _, will_close_handler = handlers
      will_close_handler()
    self._domain_handlers = {}
    self._socket.close()
    self._socket = None
    self._backend = None

  def DispatchNotifications(self):
    try:
      data = self._socket.recv()
    except socket.error:
      return None

    res = json.loads(data)
    logging.debug('got [%s]', data)
    if 'method' not in res:
      return

    mname = res['method']
    dot_pos = mname.find('.')
    domain_name = mname[:dot_pos]
    if domain_name in self._domain_handlers:
      try:
        self._domain_handlers[domain_name][0](res)
      except:
        import traceback
        traceback.print_exc()

  def SendAndIgnoreResponse(self, req):
    req['id'] = self._next_request_id
    self._next_request_id += 1
    self._socket.send(json.dumps(req))

  def SyncRequest(self, req, timeout=60):
    # TODO(nduca): Listen to the timeout argument
    # pylint: disable=W0613
    # self._socket.settimeout(timeout)
    req['id'] = self._next_request_id
    self._next_request_id += 1
    self._socket.send(json.dumps(req))

    while True:
      data = self._socket.recv()

      res = json.loads(data)
      logging.debug('got [%s]', data)
      if 'method' in res:
        mname = res['method']
        dot_pos = mname.find('.')
        domain_name = mname[:dot_pos]
        if domain_name in self._domain_handlers:
          try:
            self._domain_handlers[domain_name][0](res)
          except:
            import traceback
            traceback.print_exc()
        else:
          logging.debug('Unhandled inspector mesage: %s', data)
        continue

      if res['id'] != req['id']:
        logging.debug('Dropped reply: %s', json.dumps(res))
        continue
      return res

  def RegisterDomain(self,
      domain_name, notification_handler, will_close_handler):
    """Registers a given domain for handling notification methods.

    For example, given inspector_backend:
       def OnConsoleNotification(msg):
          if msg['method'] == 'Console.messageAdded':
             print msg['params']['message']
          return
       def OnConsoleClose(self):
          pass
       inspector_backend.RegisterDomain('Console',
                                        OnConsoleNotification, OnConsoleClose)
       """
    assert domain_name not in self._domain_handlers
    self._domain_handlers[domain_name] = (notification_handler,
                                          will_close_handler)

