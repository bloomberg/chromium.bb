#!/usr/bin/env python

import logging
import os

from django.utils import simplejson as json

import gdata.gauth
import gdata.client

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
      'chromoting_token': auth.GetChromotingToken(throws=False),
      'xmpp_token': auth.GetXmppToken(throws=False)
    }
    path = os.path.join(os.path.dirname(__file__), 'hostlist.html')
    self.response.out.write(template.render(path, template_params))


class ChromotingSessionHandler(webapp.RequestHandler):
  """Renders one Chromoting session."""
  @login_required
  def get(self):
    template_params = {
      'hostname': self.request.get('hostname'),
      'username': users.get_current_user().email(),
      'hostjid': self.request.get('hostjid'),
      'xmpp_token': auth.GetXmppToken(),
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
