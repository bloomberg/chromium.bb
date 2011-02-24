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

import gdata.gauth
import gdata.client

from google.appengine.ext import db
from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.ext.webapp import util
from google.appengine.ext.webapp.util import login_required


SCOPES = ['https://www.googleapis.com/auth/chromoting',
          'https://www.googleapis.com/auth/googletalk' ]


class NotAuthenticated(Exception):
  """API requiring authentication is called with credentials."""
  pass


class OAuthInvalidSetup(Exception):
  """OAuth configuration on app is not complete."""
  pass


class OAuthConfig(db.Model):
  """Stores the configuration data for OAuth.

  Currently used to store the consumer key and secret so that it does not need
  to be checked into the source tree.
  """
  consumer_key = db.StringProperty()
  consumer_secret = db.StringProperty()


def GetChromotingToken(throws=True):
  """Retrieves the Chromoting OAuth token for the user.

  Args:
    throws: bool (optional) Default is True. Throws if no token.

  Returns:
    An gdata.gauth.OAuthHmacToken for the current user.
  """
  user = users.get_current_user()
  access_token = None
  if user:
    access_token = LoadToken('chromoting_token')
  if throws and not access_token:
    raise NotAuthenticated()
  return access_token


def GetXmppToken(throws=True):
  """Retrieves the XMPP for Chromoting.

  Args:
    throws: bool (optional) Default is True. Throws if no token.

  Returns:
    An gdata.gauth.ClientLoginToken for the current user.
  """
  user = users.get_current_user()
  access_token = None
  if user:
    access_token = LoadToken('xmpp_token')
  if throws and not access_token:
    raise NotAuthenticated()
  return access_token


def ClearChromotingToken():
  """Clears all Chromoting OAuth token state from the datastore."""
  DeleteToken('request_token')
  DeleteToken('chromoting_token')


def ClearXmppToken():
  """Clears all Chromoting ClientLogin token state from the datastore."""
  DeleteToken('xmpp_token')


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


def LoadToken(name):
  """Leads a gdata auth token for the current user.

  Tokens are scoped to each user, and retrieved by a name.

  Args:
    name: A string with the name of the token for the current user.

  Returns:
    The token associated with the name for the user.
  """
  user_id = GetUserId();
  return gdata.gauth.AeLoad(user_id + name)


def SaveToken(name, token):
  """Saves a gdata auth token for the current user.

  Tokens are scoped to each user, and stored by a name.

  Args:
    name: A string with the name of the token.
  """
  user_id = GetUserId();
  gdata.gauth.AeSave(token, user_id + name)


def DeleteToken(name):
  """Deletes a stored gdata auth token for the current user.

  Tokens are scoped to each user, and stored by a name.

  Args:
    name: A string with the name of the token.
  """
  user_id = GetUserId();
  gdata.gauth.AeDelete(user_id + name)


def OAuthConfigKey():
  """Generates a standard key path for this app's OAuth configuration."""
  return db.Key.from_path('OAuthConfig', 'oauth_config')


def GetOAuthConfig(throws=True):
  """Retrieves the OAuthConfig for this app.

  Returns:
    The OAuthConfig object for this app.

  Raises:
    OAuthInvalidSetup if no OAuthConfig exists.
  """
  config = db.get(OAuthConfigKey())
  if throws and not config:
    raise OAuthInvalidSetup()
  return config


class ChromotingAuthHandler(webapp.RequestHandler):
  """Initiates getting the OAuth access token for the user.

  This webapp uses 3-legged OAuth. This handlers performs the first step
  of getting the OAuth request token, and then forwarding on to the
  Google Accounts authorization endpoint for the second step.  The final
  step is completed by the ChromotingAuthReturnHandler below.

  FYI, all three steps are collectively known as the "OAuth dance."
  """
  @login_required
  def get(self):
    ClearChromotingToken()
    client = gdata.client.GDClient()

    oauth_callback_url = ('http://%s/auth/chromoting_auth_return' %
        self.request.host)
    request_token = client.GetOAuthToken(
            SCOPES, oauth_callback_url, GetOAuthConfig().consumer_key,
            consumer_secret=GetOAuthConfig().consumer_secret)

    SaveToken('request_token', request_token)
    domain = None  # Not on an Google Apps domain.
    auth_uri = request_token.generate_authorization_url()
    self.redirect(str(auth_uri))


class ChromotingAuthReturnHandler(webapp.RequestHandler):
  """Finishes the authorization started in ChromotingAuthHandler.i

  After the user authorizes the OAuth request token at the OAuth request
  URL they were redirected to in ChromotingAuthHandler, OAuth will send
  them back here with an auth token in the URL.

  This handler retrievies the access token, and stores it completing the
  OAuth dance.
  """
  @login_required
  def get(self):
    saved_request_token = LoadToken('request_token')
    DeleteToken('request_token')
    request_token = gdata.gauth.AuthorizeRequestToken(
      saved_request_token, self.request.uri)

    # Upgrade the token and save in the user's datastore
    client = gdata.client.GDClient()
    access_token = client.GetAccessToken(request_token)
    SaveToken('chromoting_token', access_token)
    self.redirect("/")


class XmppAuthHandler(webapp.RequestHandler):
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
    ClearXmppToken()
    path = os.path.join(os.path.dirname(__file__), 'client_login.html')
    self.response.out.write(template.render(path, {}))

  def post(self):
    client = gdata.client.GDClient()
    email = self.request.get('username')
    password = self.request.get('password')
    try:
      client.ClientLogin(
          email, password, 'chromoclient', 'chromiumsync')
      SaveToken('xmpp_token', client.auth_token)
    except gdata.client.CaptchaChallenge:
      self.response.out.write('You need to solve a Captcha. '
          'Unforutnately, we still have to implement that.')
    self.redirect('/')


class ClearChromotingTokenHandler(webapp.RequestHandler):
  """Endpoint for dropping the user's Chromoting token."""
  @login_required
  def get(self):
    ClearChromotingToken()
    self.redirect('/')


class ClearXmppTokenHandler(webapp.RequestHandler):
  """Endpoint for dropping the user's Xmpp token."""
  @login_required
  def get(self):
    ClearXmppToken()
    self.redirect('/')


class SetupOAuthHandler(webapp.RequestHandler):
  """Administrative page for specifying the OAuth consumer key/secret."""
  @login_required
  def get(self):
    path = os.path.join(os.path.dirname(__file__),
                        'chromoting_oauth_setup.html')
    self.response.out.write(template.render(path, {}))

  def post(self):
    old_consumer_secret = self.request.get('old_consumer_secret')

    query = OAuthConfig.all()

    # If there is an existing key, only allow updating if you know the old
    # key. This is a simple safeguard against random users hitting this page.
    config = GetOAuthConfig(throws=False)
    if config:
      if config.consumer_secret != old_consumer_secret:
        self.response.out.set_status(400)
        self.response.out.write('Incorrect old consumer secret')
        return
    else:
      config = OAuthConfig(key_name = OAuthConfigKey().id_or_name())

    config.consumer_key = self.request.get('consumer_key')
    config.consumer_secret = self.request.get('new_consumer_secret')
    config.put()
    self.redirect('/')


def main():
  application = webapp.WSGIApplication(
      [
      ('/auth/chromoting_auth', ChromotingAuthHandler),
      ('/auth/xmpp_auth', XmppAuthHandler),
      ('/auth/chromoting_auth_return', ChromotingAuthReturnHandler),
      ('/auth/clear_xmpp_token', ClearXmppTokenHandler),
      ('/auth/clear_chromoting_token', ClearChromotingTokenHandler),
      ('/auth/setup_oauth', SetupOAuthHandler)
      ],
      debug=True)
  util.run_wsgi_app(application)


if __name__ == '__main__':
  main()
