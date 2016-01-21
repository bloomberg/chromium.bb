# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A server to allow events to be passed from one task to another.

This server supports getting, setting, and deleting of arbitrary event names.
This is a "pull" style of server, so its on the producer and consumer to
proactively get, set, and delete the events.


Operation | Url            | Action
-------------------------------------------------------------------------------
GET       | /events/<NAME> | Returns 200 for an event that is set.
          |                | Returns 404 for an event that is not set
-------------------------------------------------------------------------------
PUT       | /events/<NAME> | Sets the event and returns 200.
-------------------------------------------------------------------------------
DELETE    | /events/<NAME> | Deletes the event and returns 200.
-------------------------------------------------------------------------------

Errors
Error | Action | Reason
-------------------------------------------------------------------------------
403   | GET    | Category is not specified
-------------------------------------------------------------------------------
404   | GET    | Event doesn't exist or isn't set
-------------------------------------------------------------------------------
405   | All    | Incorrect URL format (must be events/<NAME>)
-------------------------------------------------------------------------------
"""

import BaseHTTPServer
import httplib
import re
import SimpleHTTPServer
import SocketServer
import threading

from legion.lib import common_lib


class ThreadedServer(SocketServer.ThreadingMixIn,
                     BaseHTTPServer.HTTPServer):
  """An extension of the HTTPServer class which handles requests in threads."""

  def __init__(self, address=''):
    self._port = common_lib.GetUnusedPort()
    self._address = address
    BaseHTTPServer.HTTPServer.__init__(self,
                                       (self._address, self._port),
                                       HttpRequestHandler)

  @property
  def port(self):
    return self._port

  @property
  def address(self):
    return self._address

  def start(self):
    """Starts the server in another thread.

    The thread will stay active until shutdown() is called. There is no reason
    to hold a reference to the thread object.
    """
    threading.Thread(target=self.serve_forever).start()


class EventHandler(object):
  """Handles event set/get/delete operations."""

  _REGEX = re.compile('^/events/(?P<name>[a-zA-Z0-9]*)')
  _events = {}
  _event_lock = threading.Lock()

  def _GetName(self, request):
    """Gets the event name from the URL."""
    match = self._REGEX.match(request.path)
    if not match:
      return None
    return match.group('name')


  def do_GET(self, request):
    """Handles GET requests."""
    name = self._GetName(request)
    if not name:
      return request.send_error(405, 'Event name required')
    with self._event_lock:
      if name not in self._events:
        return request.send_error(404, 'Event: %s not found' % name)
      else:
        return request.send_response(200)

  def do_PUT(self, request):
    """Handles PUT requests."""
    name = self._GetName(request)
    if not name:
      return request.send_error(405, 'Event name required')
    with self._event_lock:
      self._events[name] = 1
      return request.send_response(200)

  def do_DELETE(self, request):
    """Handles DELETE requests."""
    name = self._GetName(request)
    if not name:
      return request.send_error(405, 'Event name required')
    with self._event_lock:
      if name in self._events:
        del self._events[name]
        return request.send_response(200)
      else:
        return request.send_error(404, 'Event not found')


class HttpRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Request handler which dispatches requests to the correct subhandler.

  To extend this functionality implement a class that handles do_GET, do_PUT,
  and do_DELETE methods, then add the name/class to the _HANDLERS dict.
  """

  _REGEX = re.compile('/(?P<category>[a-zA-Z0-9]*)')

  _HANDLERS = {
      'events': EventHandler,
      }

  def log_message(self, *args, **kwargs):
    """Silence those pesky server-side print statements."""
    pass

  def _GetCategoryName(self):
    """Extracts and returns the category name."""
    match = self._REGEX.match(self.path)
    if not match:
      return
    return match.group('category')

  def _GetHandler(self):
    """Returns the category handler object if it exists."""
    category = self._GetCategoryName()
    if not category:
      return self.send_error(403, 'Category must be supplied')
    handler = self._HANDLERS.get(category)
    if not handler:
      return self.send_error(405, '/%s is not supported' % category)
    return handler()

  def do_GET(self):
    """Handles GET requests."""
    handler = self._GetHandler()
    if handler:
      handler.do_GET(self)

  def do_PUT(self):
    """Handles PUT requests."""
    handler = self._GetHandler()
    if handler:
      handler.do_PUT(self)

  def do_DELETE(self):
    """Handles DELETE requests."""
    handler = self._GetHandler()
    if handler:
      handler.do_DELETE(self)
