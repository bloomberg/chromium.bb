# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WSGI application to manage a USB gadget.
"""

import re
import sys

from tornado import httpserver
from tornado import web

import default_gadget

VERSION_PATTERN = re.compile(r'.*usb_gadget-([a-z0-9]{32})\.zip')

address = None
chip = None
claimed_by = None
default = default_gadget.DefaultGadget()
gadget = None


def SwitchGadget(new_gadget):
  if chip.IsConfigured():
    chip.Destroy()

  global gadget
  gadget = new_gadget
  gadget.AddStringDescriptor(3, address)
  chip.Create(gadget)


class VersionHandler(web.RequestHandler):

  def get(self):
    version = 'unpackaged'
    for path in sys.path:
      match = VERSION_PATTERN.match(path)
      if match:
        version = match.group(1)
        break

    self.write(version)


class ClaimHandler(web.RequestHandler):

  def post(self):
    global claimed_by

    if claimed_by is None:
      claimed_by = self.get_argument('session_id')
    else:
      self.write('Device is already claimed by "{}".'.format(claimed_by))
      self.set_status(403)


class UnclaimHandler(web.RequestHandler):

  def post(self):
    global claimed_by
    claimed_by = None
    if gadget != default:
      SwitchGadget(default)


class UnconfigureHandler(web.RequestHandler):

  def post(self):
    SwitchGadget(default)


class DisconnectHandler(web.RequestHandler):

  def post(self):
    if chip.IsConfigured():
      chip.Destroy()


class ReconnectHandler(web.RequestHandler):

  def post(self):
    if not chip.IsConfigured():
      chip.Create(gadget)


app = web.Application([
    (r'/version', VersionHandler),
    (r'/claim', ClaimHandler),
    (r'/unclaim', UnclaimHandler),
    (r'/unconfigure', UnconfigureHandler),
    (r'/disconnect', DisconnectHandler),
    (r'/reconnect', ReconnectHandler),
])

http_server = httpserver.HTTPServer(app)
