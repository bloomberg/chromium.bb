#!/usr/bin/env python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides authentcation related utilities and endpoint handlers.

All authentication code for the webapp should go through this module. In
general, credentials should be used server-side. The URL endpoints are for
initiating authentication flows, and for managing credential storage per user.
"""

import os
import re
import time
import urllib
from urlparse import urlparse


from google.appengine.ext import db
from google.appengine.api import urlfetch
from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.ext.webapp import util
from google.appengine.ext.webapp.util import login_required

from django.utils import simplejson as json


SCOPES = ['https://www.googleapis.com/auth/chromoting',
          'https://www.googleapis.com/auth/googletalk' ]

# Development OAuth2 ID and keys.
CLIENT_ID = ('440925447803-d9u05st5jjm3gbe865l0jeaujqfrufrn.'
            'apps.googleusercontent.com')
CLIENT_SECRET = 'Nl4vSQEgDpPMP-1rDEsgs3V7'


class NotAuthenticated(Exception):
  """API requiring authentication is called with credentials."""
  pass


class ClientLoginToken(db.Model):
  auth_token = db.StringProperty()


class OAuth2Tokens(db.Model):
  """Stores the Refresh and Access token information for OAuth2."""
  refresh_token = db.StringProperty()
  access_token = db.StringProperty()
  access_token_expiration = db.IntegerProperty()


def HasOAuth2Tokens(throws=True):
  oauth2_tokens = OAuth2Tokens.get_or_insert(GetUserId())
  if oauth2_tokens.refresh_token:
    return True;
  return False;


def GetOAuth2AccessToken(throws=True):
  oauth2_tokens = OAuth2Tokens.get_or_insert(GetUserId())

  if not oauth2_tokens.refresh_token:
    raise NotAuthenticated()

  if time.time() > oauth2_tokens.access_token_expiration:
    form_fields = {
        'client_id' : CLIENT_ID,
        'client_secret' : CLIENT_SECRET,
        'refresh_token' : oauth2_tokens.refresh_token,
        'grant_type' : 'refresh_token'
    }
    form_data = urllib.urlencode(form_fields)
    result = urlfetch.fetch(
        url = 'https://accounts.google.com/o/oauth2/token',
        payload = form_data,
        method = urlfetch.POST,
        headers = {'Content-Type': 'application/x-www-form-urlencoded'})
    if result.status_code != 200:
      raise 'something went wrong %d, %s <br />' % (
          result.status_code, result.content)
    oauth_json = json.loads(result.content)
    oauth2_tokens.access_token = oauth_json['access_token']
    # Give us 30 second buffer to hackily account for RTT on network request.
    oauth2_tokens.access_token_expiration = (
        int(oauth_json['expires_in'] + time.time() - 30))
    oauth2_tokens.put()

  return oauth2_tokens.access_token


def GetClientLoginToken(throws=True):
  """Retrieves the ClientLogin for Chromoting.

  Args:
    throws: bool (optional) Default is True. Throws if no token.

  Returns:
    The auth token for the current user.
  """
  clientlogin_token = ClientLoginToken.get_or_insert(GetUserId())
  if throws and not clientlogin_token.auth_token:
    raise NotAuthenticated()
  return clientlogin_token.auth_token


def ClearClientLoginToken():
  """Clears all Chromoting ClientLogin token state from the datastore."""
  db.delete(db.Key.from_path('ClientLoginToken', GetUserId()))


def ClearOAuth2Token():
  """Clears all Chromoting ClientLogin token state from the datastore."""
  db.delete(db.Key.from_path('OAuth2Tokens', GetUserId()))


def GetUserId():
  """Retrieves the user id for the current user.

  Returns:
    A string with the user id of the logged in user.

  Raises:
    NotAuthenticated if the user is not logged in, or missing an id.
  """
  user = users.get_current_user()
  if not user:
    raise NotAuthenticated()

  if not user.user_id():
    raise NotAuthenticated('no e-mail with google account!')

  return user.user_id()


class ClientLoginAuthHandler(webapp.RequestHandler):
  """Prompts Google Accounts credentials and retrieves a ClientLogin token.

  This class takes the user's plaintext username and password, and then
  posts a request to ClientLogin to get the access token.

  THIS CLASS SHOULD NOT EXIST.

  We should NOT be taking a user's Google Accounts credentials in our webapp.
  However, we need a ClientLogin token for jingle, and this is currently the
  only known workaround.
  """
  @login_required
  def get(self):
    ClearClientLoginToken()
    path = os.path.join(os.path.dirname(__file__), 'client_login.html')
    self.response.out.write(template.render(path, {}))

  def post(self):
    email = self.request.get('username')
    password = self.request.get('password')
    form_fields = {
        'accountType' : 'HOSTED_OR_GOOGLE',
        'Email' : self.request.get('username'),
        'Passwd' : self.request.get('password'),
        'service' : 'chromiumsync',
        'source' : 'chromoplex'
    }
    form_data = urllib.urlencode(form_fields)
    result = urlfetch.fetch(
        url = 'https://www.google.com/accounts/ClientLogin',
        payload = form_data,
        method = urlfetch.POST,
        headers = {'Content-Type': 'application/x-www-form-urlencoded'})
    if result.status_code != 200:
      self.response.out.write(result.content)
      for i in result.headers:
        self.response.headers[i] = result.headers[i]
      self.response.set_status(result.status_code)
      return

    clientlogin_token = ClientLoginToken(key_name = GetUserId())
    clientlogin_token.auth_token = re.search(
        "Auth=(.*)", result.content).group(1)
    clientlogin_token.put()
    self.redirect('/')


class ClearClientLoginTokenHandler(webapp.RequestHandler):
  """Endpoint for dropping the user's ClientLogin token."""
  @login_required
  def get(self):
    ClearClientLoginToken()
    self.redirect('/')


class ClearOAuth2TokenHandler(webapp.RequestHandler):
  """Endpoint for dropping the user's OAuth2 token."""
  @login_required
  def get(self):
    ClearOAuth2Token()
    self.redirect('/')


class OAuth2ReturnHandler(webapp.RequestHandler):
  """Handles the redirect in the OAuth dance."""
  @login_required
  def get(self):
    code = self.request.get('code')
    state = self.request.get('state')
    parsed_url = urlparse(self.request.url)
    server = parsed_url.scheme + '://' + parsed_url.netloc
    form_fields = {
        'client_id' : CLIENT_ID,
        'client_secret' : CLIENT_SECRET,
        'redirect_uri' : server + '/auth/oauth2_return',
        'code' : code,
        'grant_type' : 'authorization_code'
    }
    form_data = urllib.urlencode(form_fields)
    result = urlfetch.fetch(
        url = 'https://accounts.google.com/o/oauth2/token',
        payload = form_data,
        method = urlfetch.POST,
        headers = {'Content-Type': 'application/x-www-form-urlencoded'})

    if result.status_code != 200:
      self.response.out.write('something went wrong %d, %s <br />' %
                              (result.status_code, result.content))
      self.response.out.write(
          'We tried posting %s code(%s) [%s]' % (form_data, code, form_fields))
      self.response.set_status(400)
      return

    oauth_json = json.loads(result.content)
    oauth2_tokens = OAuth2Tokens(key_name = GetUserId())
    oauth2_tokens.refresh_token = oauth_json['refresh_token']
    oauth2_tokens.access_token = oauth_json['access_token']
    # Give us 30 second buffer to hackily account for RTT on network request.
    oauth2_tokens.access_token_expiration = (
        int(oauth_json['expires_in'] + time.time() - 30))
    oauth2_tokens.put()

    if state:
      self.redirect(state)
    else:
      self.redirect('/')


def main():
  application = webapp.WSGIApplication(
      [
      ('/auth/clientlogin_auth', ClientLoginAuthHandler),
      ('/auth/clear_clientlogin_token', ClearClientLoginTokenHandler),
      ('/auth/clear_oauth2_token', ClearOAuth2TokenHandler),
      ('/auth/oauth2_return', OAuth2ReturnHandler)
      ],
      debug=True)
  util.run_wsgi_app(application)


if __name__ == '__main__':
  main()
