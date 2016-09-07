# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

"""Authenticators used for network communication over HTTP."""

import datetime
import threading

from utils import oauth


class Authenticator(object):
  """Base class for objects that know how to authenticate into http services."""

  def authorize(self, request):
    """Add authentication information to the request."""

  def login(self, allow_user_interaction):
    """Run interactive authentication flow refreshing the token."""
    raise NotImplementedError()

  def logout(self):
    """Purges access credentials from local cache."""


class OAuthAuthenticator(Authenticator):
  """Uses OAuth Authorization header to authenticate requests."""

  def __init__(self, urlhost, config):
    super(OAuthAuthenticator, self).__init__()
    assert isinstance(config, oauth.OAuthConfig)
    self.urlhost = urlhost
    self.config = config
    self._lock = threading.Lock()
    self._access_token = None

  def authorize(self, request):
    with self._lock:
      # Load from cache on a first access.
      if not self._access_token:
        self._access_token = oauth.load_access_token(self.urlhost, self.config)
      # Refresh if expired.
      need_refresh = True
      if self._access_token:
        if self._access_token.expires_at is not None:
          # Allow 5 min of clock skew.
          now = datetime.datetime.utcnow() + datetime.timedelta(seconds=300)
          need_refresh = now >= self._access_token.expires_at
        else:
          # Token without expiration time never expired.
          need_refresh = False
      if need_refresh:
        self._access_token = oauth.create_access_token(
            self.urlhost, self.config, False)
      if self._access_token:
        request.headers['Authorization'] = (
            'Bearer %s' % self._access_token.token)

  def login(self, allow_user_interaction):
    with self._lock:
      # Forcefully refresh the token.
      self._access_token = oauth.create_access_token(
          self.urlhost, self.config, allow_user_interaction)
      return self._access_token is not None

  def logout(self):
    with self._lock:
      self._access_token = None
      oauth.purge_access_token(self.urlhost, self.config)
