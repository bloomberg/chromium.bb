#!/usr/bin/env python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API endpoints to get around javascript's single-origin restriction."""

import logging

from django.utils import simplejson as json

import gdata.client

from google.appengine.ext import webapp
from google.appengine.ext.webapp import util
from google.appengine.ext.webapp.util import login_required
import auth


class GetXmppTokenHandler(webapp.RequestHandler):
  """Retrieves the user's XMPP token."""
  @login_required
  def get(self):
    try:
      self.response.headers['Content-Type'] = 'application/json'
      self.response.out.write(
          json.dumps({'xmpp_token': auth.GetXmppToken().token}))
    except auth.NotAuthenticated:
      self.response.out.write('User has not authenticated')
      self.set_status(400)

class GetHostListHandler(webapp.RequestHandler):
  """Proxies the host-list handlers on the Chromoting directory."""
  @login_required
  def get(self):
    try:
      client = gdata.client.GDClient()
      host_list_json = client.Request(
          method='GET',
          uri="https://www.googleapis.com/chromoting/v1/@me/hosts",
          converter=None,
          desired_class=None,
          auth_token=auth.GetChromotingToken())
      self.response.headers['Content-Type'] = 'application/json'
      self.response.out.write(host_list_json.read())
    except auth.NotAuthenticated:
      self.response.out.write('User has not authenticated')
      self.response.set_status(400)
    except (gdata.client.Unauthorized, gdata.client.RequestError), inst:
      self.response.out.write(inst.reason)
      self.response.set_status(inst.status)

def main():
  application = webapp.WSGIApplication(
      [
      ('/api/get_xmpp_token', GetXmppTokenHandler),
      ('/api/get_host_list', GetHostListHandler)
      ],
      debug=True)
  util.run_wsgi_app(application)


if __name__ == '__main__':
  main()
