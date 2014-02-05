# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""OAuth2 related utilities and implementation of browser based login flow."""

# pylint: disable=W0613

import BaseHTTPServer
import datetime
import logging
import optparse
import os
import socket
import sys
import urlparse
import webbrowser

# oauth2client expects to find itself in sys.path.
# Also ensure we use bundled version instead of system one.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import httplib2
from oauth2client import client
from oauth2client import multistore_file

# keyring_storage depends on 'keyring' module that may not be available.
try:
  from oauth2client import keyring_storage
except ImportError:
  keyring_storage = None

from third_party import requests
from utils import tools


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
      '--auth-no-local-webserver', action='store_true',
      default=tools.get_bool_env_var('SWARMING_AUTH_NO_LOCAL_WEBSERVER'),
      help=('Do not run a local web server when performing OAuth2 login flow. '
          'Can also be set with SWARMING_AUTH_NO_LOCAL_WEBSERVER=1 '
          'environment variable.'))
  parser.oauth_group.add_option(
      '--auth-host-port', action='append', type=int,
      default=[8080, 8090],
      help=('Port a local web server should listen on. Used only if '
          '--auth-no-local-webserver is not set.'))
  parser.oauth_group.add_option(
      '--auth-no-keyring', action='store_true',
      default=tools.get_bool_env_var('SWARMING_AUTH_NO_KEYRING'),
      help=('Do not use system keyring to store OAuth2 tokens, store them in '
          'file %s instead. Can also be set with SWARMING_AUTH_NO_KEYRING=1 '
          'environment variable.' % OAUTH_STORAGE_FILE))
  parser.add_option_group(parser.oauth_group)


def load_access_token(urlhost, options):
  """Returns cached access token if it is not expired yet."""
  storage = _get_storage(urlhost, options)
  credentials = storage.get()
  # Missing?
  if not credentials or credentials.invalid:
    return None
  # Expired?
  if not credentials.access_token or credentials.access_token_expired:
    return None
  return credentials.access_token


def create_access_token(urlhost, options, allow_user_interaction):
  """Mints and caches new access_token, launching OAuth2 dance if necessary.

  Args:
    urlhost: base URL of a host to make OAuth2 token for.
    options: OptionParser instance with options added by 'add_oauth_options'.
    allow_user_interaction: if False, do not use interactive browser based
        flow (return None instead if it is required).

  Returns:
    access_token on success.
    None on error or if OAuth2 flow was interrupted.
  """
  storage = _get_storage(urlhost, options)
  credentials = storage.get()

  # refresh_token is missing, need to go through full flow.
  if credentials is None or credentials.invalid:
    if allow_user_interaction:
      return _run_oauth_dance(urlhost, storage, options)
    return None

  # refresh_token is ok, use it.
  try:
    credentials.refresh(httplib2.Http())
  except client.Error as err:
    logging.error('OAuth error: %s', err)
    if allow_user_interaction:
      return _run_oauth_dance(urlhost, storage, options)
    return None

  # Success.
  logging.info('OAuth access_token refreshed. Expires in %s.',
      credentials.token_expiry - datetime.datetime.utcnow())
  storage.put(credentials)
  return credentials.access_token


def purge_access_token(urlhost, options):
  """Deletes OAuth tokens that can be used to access |urlhost|."""
  _get_storage(urlhost, options).delete()


def _get_storage(urlhost, options):
  """Returns oauth2client.Storage with tokens to access |urlhost|."""
  # Normalize urlhost.
  urlhost = urlhost.rstrip('/')
  # Use keyring storage if 'keyring' module is present and enabled.
  if keyring_storage and not options.auth_no_keyring:
    return keyring_storage.Storage(urlhost, 'oauth2_tokens')
  # Revert to less secure plain text storage otherwise.
  return multistore_file.get_credential_storage_custom_string_key(
      OAUTH_STORAGE_FILE, urlhost)


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


# The chunk of code below is based on oauth2client.tools module. Unfortunately
# 'tools' module itself depends on 'argparse' module unavailable on Python 2.6
# so it can't be imported directly.


def _run_oauth_dance(urlhost, storage, options):
  """Perform full OAuth2 dance with the browser."""
  # Fetch client_id and client_secret from service itself.
  client_id, client_not_so_secret = _fetch_oauth_client_id(urlhost)
  if not client_id or not client_not_so_secret:
    print 'Couldn\'t fetch OAuth client credentials'
    return None

  # Appengine expects a token scoped to 'userinfo.email'.
  flow = client.OAuth2WebServerFlow(
      client_id,
      client_not_so_secret,
      'https://www.googleapis.com/auth/userinfo.email',
      approval_prompt='force')

  use_local_webserver = not options.auth_no_local_webserver
  ports = options.auth_host_port
  if use_local_webserver:
    success = False
    port_number = 0
    for port in ports:
      port_number = port
      try:
        httpd = ClientRedirectServer(('localhost', port), ClientRedirectHandler)
      except socket.error:
        pass
      else:
        success = True
        break
    use_local_webserver = success
    if not success:
      print 'Failed to start a local webserver listening on ports %r.' % ports
      print 'Please check your firewall settings and locally'
      print 'running programs that may be blocking or using those ports.'
      print
      print 'Falling back to --auth-no-local-webserver and continuing with',
      print 'authorization.'
      print

  if use_local_webserver:
    oauth_callback = 'http://localhost:%s/' % port_number
  else:
    oauth_callback = client.OOB_CALLBACK_URN
  flow.redirect_uri = oauth_callback
  authorize_url = flow.step1_get_authorize_url()

  if use_local_webserver:
    webbrowser.open(authorize_url, new=1, autoraise=True)
    print 'Your browser has been opened to visit:'
    print
    print '    ' + authorize_url
    print
    print 'If your browser is on a different machine then exit and re-run this'
    print 'application with the command-line parameter '
    print
    print '  --auth-no-local-webserver'
    print
  else:
    print 'Go to the following link in your browser:'
    print
    print '    ' + authorize_url
    print

  code = None
  if use_local_webserver:
    httpd.handle_request()
    if 'error' in httpd.query_params:
      print 'Authentication request was rejected.'
      return None
    if 'code' not in httpd.query_params:
      print 'Failed to find "code" in the query parameters of the redirect.'
      print 'Try running with --auth-no-local-webserver.'
      return None
    code = httpd.query_params['code']
  else:
    code = raw_input('Enter verification code: ').strip()

  try:
    credential = flow.step2_exchange(code)
  except client.FlowExchangeError as e:
    print 'Authentication has failed: %s' % e
    return None

  print 'Authentication successful.'
  storage.put(credential)
  credential.set_store(storage)
  return credential.access_token


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
