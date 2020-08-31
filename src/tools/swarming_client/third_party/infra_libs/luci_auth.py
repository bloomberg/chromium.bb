# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements oauth2client's OAuth2Credentials subclass that uses LUCI local
auth (usually on Swarming) for getting access tokens.
"""

import collections
import datetime
import json
import logging

import httplib2
import oauth2client.client

from infra_libs import luci_ctx


DEFAULT_SCOPES = ('https://www.googleapis.com/auth/userinfo.email',)


class LUCIAuthError(Exception):
  """Raised if there's unexpected error when getting access token from LUCI."""


def available(environ=None):
  """Returns True if the environment has ambient LUCI auth setup."""
  params = _local_auth_params(environ)
  return params and params.default_account_id


def get_access_token(scopes=DEFAULT_SCOPES, environ=None, http=None):
  """Grabs an access token from LUCI local server.

  Does RPC each time it is called. For efficiency, either cache the token until
  it expires, or use LUCICredentials class that does it itself already.

  Args:
    scopes: list of OAuth scopes the constructed token should have.
    environ: an environ dict to use instead of os.environ, for tests.
    http: Http instance to use for request, default is new httplib2.Http.

  Returns:
    Tuple (access token str, datatime.datetime when it expires or None if not).

  Raises:
    LUCIAuthError on unexpected errors.
  """
  _check_scopes(scopes)
  params = _local_auth_params(environ)
  if not params or not params.default_account_id:
    raise LUCIAuthError('LUCI auth is not available')

  logging.debug(
      'luci_auth: requesting an access token for account "%s"',
      params.default_account_id)

  http = http or httplib2.Http()
  host = '127.0.0.1:%d' % params.rpc_port
  resp, content = http.request(
      uri='http://%s/rpc/LuciLocalAuthService.GetOAuthToken' % host,
      method='POST',
      body=json.dumps({
        'account_id': params.default_account_id,
        'scopes': scopes,
        'secret': params.secret,
      }),
      headers={'Content-Type': 'application/json'})
  if resp.status != 200:
    raise LUCIAuthError(
        'Failed to grab access token from local auth server %s, status %d: %r' %
        (host, resp.status, content))

  try:
    token = json.loads(content)
  except ValueError as e:
    raise LUCIAuthError('Cannot parse the local auth server reply: %s' % e)

  error_code = token.get('error_code')
  error_message = token.get('error_message')
  access_token = token.get('access_token')
  expiry = token.get('expiry')

  if error_code:
    raise LUCIAuthError(
        'Error #%d when retrieving access token: %s' %
        (error_code, error_message))

  if expiry is None:
    logging.debug(
        'luci_auth: got an access token for account "%s" that does not expire',
        params.default_account_id)
    return access_token, None

  exp = datetime.datetime.utcfromtimestamp(expiry)
  logging.debug(
      'luci_auth: got an access token for account "%s" that expires in %d sec',
      params.default_account_id,
      (exp - datetime.datetime.utcnow()).total_seconds())
  return str(access_token), exp


class LUCICredentials(oauth2client.client.OAuth2Credentials):
  """Credentials that use LUCI local auth to get access tokens."""

  def __init__(self, scopes=DEFAULT_SCOPES, environ=None, http=None):
    """Create a LUCICredentials that request tokens with given scopes.

    Args:
      scopes: list of OAuth scopes the constructed token should have.
      environ: an environ dict to use instead of os.environ, for tests.
      http: Http instance to use for request, default is new httplib2.Http.
    """
    if not available(environ=environ):
      raise LUCIAuthError('LUCI auth is not available')
    super(LUCICredentials, self).__init__(
        None, None, None, None, None, None, None)
    _check_scopes(scopes)
    self._scopes = tuple(scopes)
    self._environ = environ
    self._http = http

  def _refresh(self, _http_request):
    # pylint: disable=attribute-defined-outside-init
    # All fields here are 'inherited' from OAuth2Credentials.
    self.access_token, self.token_expiry = get_access_token(
        self._scopes,
        environ=self._environ,
        http=self._http)
    self.invalid = False


###


# Represents LUCI_CONTEXT['local_auth'] dict.
_LocalAuthParams = collections.namedtuple(
  '_LocalAuthParams', [
    'default_account_id',
    'secret',
    'rpc_port',
])


def _local_auth_params(environ):
  """Reads _LocalAuthParams from LUCI_CONTEXT, returns None if not there."""
  p = luci_ctx.read('local_auth', environ=environ)
  if not p:
    return None
  try:
    return _LocalAuthParams(
        default_account_id=p.get('default_account_id'),
        secret=p.get('secret'),
        rpc_port=int(p.get('rpc_port')))
  except ValueError as e:
    raise LUCIAuthError('local_auth section is malformed: %s' % e)


def _check_scopes(scopes):
  """Raises TypeError is list of scopes is malformed."""
  if not isinstance(scopes, (list, tuple)):
    raise TypeError('Scopes must be a list or a tuple')
  if not all(isinstance(s, basestring) for s in scopes):
    raise TypeError('Each scope must be a string')
