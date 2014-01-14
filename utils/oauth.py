# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""OAuth2 related utilities, mainly wrapper around oauth2client."""

# pylint: disable=W0613

import datetime
import logging
import optparse
import os
import sys

# oauth2client expects to find itself in sys.path.
# Also ensure we use bundled version instead of system one.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import httplib2
from oauth2client import client
from oauth2client import tools
from oauth2client import multistore_file

# keyring_storage depends on 'keyring' module that may not be available.
try:
  from oauth2client import keyring_storage
except ImportError:
  keyring_storage = None

from third_party import requests


# Path to a file with cached OAuth2 credentials, used only if keyring is not
# available.
OAUTH_STORAGE_FILE = os.path.join(os.path.expanduser('~'), '.isolated_oauth')


def add_oauth_options(parser):
  """Appends OAuth related options to OptionParser."""
  # Same options as in tools.argparser, excluding auth_host_name and
  # logging_level. We should never ever use OAuth client_id with
  # open-ended request_uri. It should always be 'localhost'.
  parser.oauth_group = optparse.OptionGroup(parser, 'OAuth options')
  parser.oauth_group.add_option(
      '--noauth-local-webserver', action='store_true',
      default=bool(int(os.environ.get('DISABLE_LOCAL_SERVER', '0'))),
      help='Do not run a local web server.')
  parser.oauth_group.add_option(
      '--auth-host-port', action='append', type=int,
      default=[8080, 8090], help='Port web server should listen on.')
  parser.add_option_group(parser.oauth_group)


def load_access_token(urlhost, options):
  """Returns cached access token if it is not expired yet."""
  storage = _get_storage(urlhost)
  credentials = storage.get()
  # Missing?
  if not credentials or credentials.invalid:
    return None
  # Expired?
  if not credentials.access_token or credentials.access_token_expired:
    return None
  return credentials.access_token


def create_access_token(urlhost, options):
  """Mints and caches new access_token, launching OAuth2 dance if necessary.

  Returns:
    access_token on success.
    None on error or if OAuth2 flow was interrupted.
  """
  storage = _get_storage(urlhost)
  credentials = storage.get()

  # refresh_token is missing, need to go through full flow.
  if credentials is None or credentials.invalid:
    return _run_oauth_dance(urlhost, storage, options)

  # refresh_token is ok, use it.
  try:
    credentials.refresh(httplib2.Http())
  except client.Error as err:
    logging.error('OAuth error: %s', err)
    return _run_oauth_dance(urlhost, storage, options)

  # Success.
  logging.info('OAuth access_token refreshed. Expires in %s.',
      credentials.token_expiry - datetime.datetime.utcnow())
  storage.put(credentials)
  return credentials.access_token


def purge_access_token(urlhost):
  """Deletes OAuth tokens that can be used to access |urlhost|."""
  _get_storage(urlhost).delete()


def _get_storage(urlhost):
  """Returns oauth2client.Storage with tokens to access |urlhost|."""
  # Normalize urlhost.
  urlhost = urlhost.rstrip('/')
  # Use keyring storage if 'keyring' module is present (it is on Linux).
  if keyring_storage:
    return keyring_storage.Storage(urlhost, 'oauth2_tokens')
  # Revert to less secure plain text storage otherwise (Win, Mac).
  return multistore_file.get_credential_storage_custom_string_key(
      OAUTH_STORAGE_FILE, urlhost)


def _run_oauth_dance(urlhost, storage, options):
  """Perform full OAuth2 dance with the browser."""
  # Fetch client_id and client_secret from service itself.
  client_id, client_not_so_secret = _fetch_oauth_client_id(urlhost)
  if not client_id or not client_not_so_secret:
    logging.error('Couldn\'t fetch OAuth client credentials')
    return None

  # Appengine expects a token scoped to 'userinfo.email'.
  flow = client.OAuth2WebServerFlow(
      client_id,
      client_not_so_secret,
      'https://www.googleapis.com/auth/userinfo.email',
      approval_prompt='force')

  # There are some issues with run_flow.
  # 1) It messes up global logging level.
  # 2) It expects to find 'logging_level' in options.
  # 3) It mutates options inside.
  # 4) It uses sys.exit() to indicate a failure.
  try:
    tools.logging = _EmptyMock()
    credentials = tools.run_flow(flow, storage, _extract_flags(options))
  except SystemExit:
    return None

  # run_flow stores a token into |storage| itself.
  return credentials.access_token


def _fetch_oauth_client_id(urlhost):
  """Ask service to for client_id and client_secret to use."""
  # client_secret is not really a secret in that case. So an attacker can
  # impersonate service's identity in OAuth2 flow. But that's generally
  # fine as long as a list of allowed redirect_uri's associated with client_id
  # is limited to 'localhost' or 'urn:ietf:wg:oauth:2.0:oob'. In that case
  # attacker needs some process running on user's machine to successfully
  # complete the flow and grab access_token. When you have malicious code
  # running on your machine your screwed anyway.
  response = requests.get(
      '%s/auth/api/v1/server/oauth_config' % urlhost.rstrip('/'))
  if response.status_code == 200:
    try:
      config = response.json()
      if not isinstance(config, dict):
        raise ValueError()
      return config['client_id'], config['client_not_so_secret']
    except (KeyError, ValueError) as err:
      logging.error('Invalid response from the service: %s', err)
  else:
    logging.error(
        'Error when fetching oauth_config, HTTP status code %d',
        response.status_code)
  return None, None


class _EmptyMock(object):
  """Black hole that absorbs all calls and attribute accesses."""
  def __getattr__(self, name):
    return self
  def __call__(self, *args, **kwargs):
    return self


def _extract_flags(options):
  """Options parser -> mutable object with a structure expected by run_flow."""
  class Flags(object):
    auth_host_name = 'localhost'
    auth_host_port = options.auth_host_port
    logging_level = 'ignored'
    noauth_local_webserver = options.noauth_local_webserver
  return Flags()
