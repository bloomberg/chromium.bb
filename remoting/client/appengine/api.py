#!/usr/bin/env python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API endpoints to get around javascript's single-origin restriction."""

import logging

from django.utils import simplejson as json

from google.appengine.api import urlfetch
from google.appengine.ext import webapp
from google.appengine.ext.webapp import util
from google.appengine.ext.webapp.util import login_required
import auth


class GetHostListHandler(webapp.RequestHandler):
  """Proxies the host-list handlers on the Chromoting directory."""
  @login_required
  def get(self):
    if not auth.HasOAuth2Tokens():
      self.response.set_status(403)
      self.response.headers['Content-Type'] = 'application/json'
      self.response.out.write(
          '{"error": { "code": -1, "message": "No OAuth2 token" }}')
      return
    result = urlfetch.fetch(
        url = 'https://www.googleapis.com/chromoting/v1/@me/hosts',
        method = urlfetch.GET,
        headers = {'Authorization': 'OAuth ' + auth.GetOAuth2AccessToken()})
    self.response.set_status(result.status_code)
    for i in result.headers:
      self.response.headers[i] = result.headers[i]
    self.response.out.write(result.content)


def main():
  application = webapp.WSGIApplication(
      [
      ('/api/get_host_list', GetHostListHandler)
      ],
      debug=True)
  util.run_wsgi_app(application)


if __name__ == '__main__':
  main()
