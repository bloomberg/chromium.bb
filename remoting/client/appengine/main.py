#!/usr/bin/env python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from django.utils import simplejson as json

from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.ext.webapp import util
from google.appengine.ext.webapp.util import login_required

import auth

class HostListHandler(webapp.RequestHandler):
  """Renders the main hostlist page."""
  @login_required
  def get(self):
    template_params = {
      'has_oauth2_tokens': auth.HasOAuth2Tokens(),
      'clientlogin_token': auth.GetClientLoginToken(throws=False)
    }
    path = os.path.join(os.path.dirname(__file__), 'hostlist.html')
    self.response.out.write(template.render(path, template_params))


class ChromotingSessionHandler(webapp.RequestHandler):
  """Renders one Chromoting session."""
  @login_required
  def get(self):
    token_type = self.request.get('token_type')
    if token_type == 'clientlogin':
      talk_token = auth.GetClientLoginToken()
    else:
      talk_token = auth.GetOAuth2AccessToken()

    template_params = {
      'hostname': self.request.get('hostname'),
      'username': users.get_current_user().email(),
      'hostjid': self.request.get('hostjid'),
      'connect_method': self.request.get('connect_method'),
      'insecure': self.request.get('insecure'),
      'talk_token': talk_token,
      'http_xmpp_proxy': self.request.get('http_xmpp_proxy')
    }
    path = os.path.join(os.path.dirname(__file__), 'chromoting_session.html')
    self.response.out.write(template.render(path, template_params))


def main():
  application = webapp.WSGIApplication(
      [
      ('/', HostListHandler),
      ('/session', ChromotingSessionHandler),
      ],
      debug=True)
  util.run_wsgi_app(application)


if __name__ == '__main__':
  main()
