# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""OAuth2 related utilities and implementation of browser based login flow."""

# pylint: disable=W0613

import BaseHTTPServer
import collections
import datetime
import logging
import optparse
import os
import socket
import sys
import threading
import urlparse
import webbrowser

# oauth2client expects to find itself in sys.path.
# Also ensure we use bundled version instead of system one.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import httplib2
from oauth2client import client
from oauth2client import multistore_file

from third_party import requests
from utils import tools


# Path to a file with cached OAuth2 credentials used by default. Can be
# overridden by command line option or env variable.
DEFAULT_OAUTH_TOKENS_CACHE = os.path.join(
    os.path.expanduser('~'), '.isolated_oauth')


# OAuth authentication method configuration, used by utils/net.py.
# See doc string for 'make_oauth_config' for meaning of fields.
OAuthConfig = collections.namedtuple('OAuthConfig', [
  'disabled',
  'tokens_cache',
  'no_local_webserver',
  'webserver_port',
])


# Access token with its expiration time (UTC datetime, or None if not known).
AccessToken = collections.namedtuple('AccessToken', [
  'token',
  'expires_at',
])


# Configuration fetched from a service, returned by _fetch_service_config.
_ServiceConfig = collections.namedtuple('_ServiceConfig', [
  'client_id',
  'client_secret',
  'primary_url',
])

# Process cache of _fetch_service_config results.
_service_config_cache = {}
_service_config_cache_lock = threading.Lock()


def make_oauth_config(
    disabled=None,
    tokens_cache=None,
    no_local_webserver=None,
    webserver_port=None):
  """Returns new instance of OAuthConfig.

  If some config option is not provided or None, it will be set to a reasonable
  default value. This function also acts as an authoritative place for default
  values of corresponding command line options.

  Args:
    disabled: True to completely turn off OAuth authentication.
    tokens_cache: path to a file with cached OAuth2 credentials.
    no_local_webserver: if True, do not try to run local web server that
        handles redirects. Use copy-pasted verification code instead.
    webserver_port: port to run local webserver on.
  """
  if disabled is None:
    disabled = tools.is_headless()
  if tokens_cache is None:
    tokens_cache = os.environ.get(
        'SWARMING_AUTH_TOKENS_CACHE', DEFAULT_OAUTH_TOKENS_CACHE)
  if no_local_webserver is None:
    no_local_webserver = tools.get_bool_env_var(
        'SWARMING_AUTH_NO_LOCAL_WEBSERVER')
  if webserver_port is None:
    webserver_port = 8090
  return OAuthConfig(disabled, tokens_cache, no_local_webserver, webserver_port)


def add_oauth_options(parser):
  """Appends OAuth related options to OptionParser."""
  default_config = make_oauth_config()
  parser.oauth_group = optparse.OptionGroup(parser, 'OAuth options')
  parser.oauth_group.add_option(
      '--auth-disabled',
      type=int,
      default=int(default_config.disabled),
      help='Set to 1 to disable OAuth and rely only on IP whitelist for '
           'authentication. Currently used from bots. [default: %default]')
  parser.oauth_group.add_option(
      '--auth-tokens-cache',
      default=default_config.tokens_cache,
      help='Path to a file with oauth2client tokens cache. It should be a safe '
          'location accessible only to a current user: knowing content of this '
          'file is roughly equivalent to knowing account password. Can also be '
          'set with SWARMING_AUTH_TOKENS_CACHE environment variable. '
          '[default: %default]')
  parser.oauth_group.add_option(
      '--auth-no-local-webserver',
      action='store_true',
      default=default_config.no_local_webserver,
      help='Do not run a local web server when performing OAuth2 login flow. '
          'Can also be set with SWARMING_AUTH_NO_LOCAL_WEBSERVER=1 '
          'environment variable. [default: %default]')
  parser.oauth_group.add_option(
      '--auth-host-port',
      type=int,
      default=default_config.webserver_port,
      help='Port a local web server should listen on. Used only if '
          '--auth-no-local-webserver is not set. [default: %default]')
  parser.add_option_group(parser.oauth_group)


def extract_oauth_config_from_options(options):
  """Given OptionParser with oauth options, extracts OAuthConfig from it.

  OptionParser should be populated with oauth options by 'add_oauth_options'.
  """
  return make_oauth_config(
      disabled=bool(options.auth_disabled),
      tokens_cache=options.auth_tokens_cache,
      no_local_webserver=options.auth_no_local_webserver,
      webserver_port=options.auth_host_port)


def load_access_token(urlhost, config):
  """Returns cached AccessToken if it is not expired yet."""
  assert isinstance(config, OAuthConfig)
  if config.disabled:
    return None
  auth_service_url = _fetch_auth_service_url(urlhost)
  if not auth_service_url:
    return None
  storage = _get_storage(auth_service_url, config)
  credentials = storage.get()
  # Missing?
  if not credentials or credentials.invalid:
    return None
  # Expired?
  if not credentials.access_token or credentials.access_token_expired:
    return None
  return AccessToken(credentials.access_token, credentials.token_expiry)


def create_access_token(urlhost, config, allow_user_interaction):
  """Mints and caches new access_token, launching OAuth2 dance if necessary.

  Args:
    urlhost: base URL of a host to make OAuth2 token for.
    config: OAuthConfig instance.
    allow_user_interaction: if False, do not use interactive browser based
        flow (return None instead if it is required).

  Returns:
    AccessToken on success.
    None on error or if OAuth2 flow was interrupted.
  """
  assert isinstance(config, OAuthConfig)
  if config.disabled:
    return None
  auth_service_url = _fetch_auth_service_url(urlhost)
  if not auth_service_url:
    return None
  storage = _get_storage(auth_service_url, config)
  credentials = storage.get()

  # refresh_token is missing, need to go through full flow.
  if credentials is None or credentials.invalid:
    if allow_user_interaction:
      return _run_oauth_dance(auth_service_url, storage, config)
    return None

  # refresh_token is ok, use it.
  try:
    credentials.refresh(httplib2.Http(ca_certs=tools.get_cacerts_bundle()))
  except client.Error as err:
    logging.error('OAuth error: %s', err)
    if allow_user_interaction:
      return _run_oauth_dance(auth_service_url, storage, config)
    return None

  # Success.
  logging.info('OAuth access_token refreshed. Expires in %s.',
      credentials.token_expiry - datetime.datetime.utcnow())
  storage.put(credentials)
  return AccessToken(credentials.access_token, credentials.token_expiry)


def purge_access_token(urlhost, config):
  """Deletes OAuth tokens that can be used to access |urlhost|."""
  assert isinstance(config, OAuthConfig)
  auth_service_url = _fetch_auth_service_url(urlhost)
  if auth_service_url:
    _get_storage(auth_service_url, config).delete()


def _get_storage(urlhost, config):
  """Returns oauth2client.Storage with tokens to access |urlhost|."""
  return multistore_file.get_credential_storage_custom_string_key(
      config.tokens_cache, urlhost.rstrip('/'))


def _fetch_auth_service_url(urlhost):
  """Fetches URL of a main authentication service used by |urlhost|.

  Returns:
    * If |urlhost| is using a authentication service, returns its URL.
    * If |urlhost| is not using authentication servier, returns |urlhost|.
    * If there was a error communicating with |urlhost|, returns None.
  """
  service_config = _fetch_service_config(urlhost)
  if not service_config:
    return None
  url = (service_config.primary_url or urlhost).rstrip('/')
  assert url.startswith(('https://', 'http://localhost:')), url
  return url


def _fetch_service_config(urlhost):
  """Fetches OAuth related configuration from a service.

  The configuration includes OAuth client_id and client_secret, as well as
  URL of a primary authentication service (or None if not used).

  Returns:
    Instance of _ServiceConfig on success, None on failure.
  """
  def do_fetch():
    # client_secret is not really a secret in that case. So an attacker can
    # impersonate service's identity in OAuth2 flow. But that's generally
    # fine as long as a list of allowed redirect_uri's associated with client_id
    # is limited to 'localhost' or 'urn:ietf:wg:oauth:2.0:oob'. In that case
    # attacker needs some process running on user's machine to successfully
    # complete the flow and grab access_token. When you have malicious code
    # running on your machine you're screwed anyway.
    response = requests.get(
        '%s/auth/api/v1/server/oauth_config' % urlhost.rstrip('/'),
        verify=tools.get_cacerts_bundle())
    if response.status_code == 200:
      try:
        config = response.json()
        if not isinstance(config, dict):
          raise ValueError()
        return _ServiceConfig(
            config['client_id'],
            config['client_not_so_secret'],
            config.get('primary_url'))
      except (KeyError, ValueError) as err:
        logging.error('Invalid response from the service: %s', err)
    else:
      logging.warning(
          'Error when fetching oauth_config, HTTP status code %d',
          response.status_code)
    return None

  # Use local cache to avoid unnecessary network calls.
  with _service_config_cache_lock:
    if urlhost not in _service_config_cache:
      config = do_fetch()
      if config:
        _service_config_cache[urlhost] = config
    return _service_config_cache.get(urlhost)


# The chunk of code below is based on oauth2client.tools module, but adapted for
# usage of _fetch_service_config, our command line arguments, and so on.


def _run_oauth_dance(urlhost, storage, config):
  """Perform full OAuth2 dance with the browser."""
  def out(s):
    print s
  def err(s):
    print >> sys.stderr, s

  # Fetch client_id and client_secret from the service itself.
  service_config = _fetch_service_config(urlhost)
  if not service_config:
    err('Couldn\'t fetch OAuth configuration')
    return None
  if not service_config.client_id or not service_config.client_secret:
    err('OAuth is not configured on the service')
    return None

  # Appengine expects a token scoped to 'userinfo.email'.
  flow = client.OAuth2WebServerFlow(
      service_config.client_id,
      service_config.client_secret,
      'https://www.googleapis.com/auth/userinfo.email',
      approval_prompt='force')

  use_local_webserver = not config.no_local_webserver
  port = config.webserver_port
  if use_local_webserver:
    success = False
    try:
      httpd = ClientRedirectServer(('localhost', port), ClientRedirectHandler)
    except socket.error:
      pass
    else:
      success = True
    use_local_webserver = success
    if not success:
      out(
        'Failed to start a local webserver listening on port %d.\n'
        'Please check your firewall settings and locally running programs that '
        'may be blocking or using those ports.\n\n'
        'Falling back to --auth-no-local-webserver and continuing with '
        'authentication.\n' % port)

  if use_local_webserver:
    oauth_callback = 'http://localhost:%s/' % port
  else:
    oauth_callback = client.OOB_CALLBACK_URN
  flow.redirect_uri = oauth_callback
  authorize_url = flow.step1_get_authorize_url()

  if use_local_webserver:
    webbrowser.open(authorize_url, new=1, autoraise=True)
    out(
      'Your browser has been opened to visit:\n\n'
      '    %s\n\n'
      'If your browser is on a different machine then exit and re-run this '
      'application with the command-line parameter\n\n'
      '  --auth-no-local-webserver\n' % authorize_url)
  else:
    out(
      'Go to the following link in your browser:\n\n'
      '    %s\n' % authorize_url)

  try:
    code = None
    if use_local_webserver:
      httpd.handle_request()
      if 'error' in httpd.query_params:
        err('Authentication request was rejected.')
        return None
      if 'code' not in httpd.query_params:
        err(
          'Failed to find "code" in the query parameters of the redirect.\n'
          'Try running with --auth-no-local-webserver.')
        return None
      code = httpd.query_params['code']
    else:
      code = raw_input('Enter verification code: ').strip()
  except KeyboardInterrupt:
    err('Canceled.')
    return None

  try:
    credential = flow.step2_exchange(code)
  except client.FlowExchangeError as e:
    err('Authentication has failed: %s' % e)
    return None

  out('Authentication successful.')
  storage.put(credential)
  credential.set_store(storage)
  return AccessToken(credential.access_token, credential.token_expiry)


class ClientRedirectServer(BaseHTTPServer.HTTPServer):
  """A server to handle OAuth 2.0 redirects back to localhost.

  Waits for a single request and parses the query parameters
  into query_params and then stops serving.
  """
  query_params = {}


class ClientRedirectHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """A handler for OAuth 2.0 redirects back to localhost.

  Waits for a single request and parses the query parameters
  into the servers query_params and then stops serving.
  """

  def do_GET(self):
    """Handle a GET request.

    Parses the query parameters and prints a message
    if the flow has completed. Note that we can't detect
    if an error occurred.
    """
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    query = self.path.split('?', 1)[-1]
    query = dict(urlparse.parse_qsl(query))
    self.server.query_params = query
    self.wfile.write('<html><head><title>Authentication Status</title></head>')
    self.wfile.write('<body><p>The authentication flow has completed.</p>')
    self.wfile.write('</body></html>')

  def log_message(self, _format, *args):
    """Do not log messages to stdout while running as command line program."""
