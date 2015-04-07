# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""OAuth2 related utilities and implementation for git cl commands."""

import copy
import logging
import optparse
import os

from third_party.oauth2client import tools
from third_party.oauth2client.file import Storage
import third_party.oauth2client.client as oa2client


REDIRECT_URI = 'urn:ietf:wg:oauth:2.0:oob'
CLIENT_ID = ('174799409470-8k3b89iov4racu9jrf7if3k4591voig3'
             '.apps.googleusercontent.com')
CLIENT_SECRET = 'DddcCK1d6_ADwxqGDEGlsisy'
SCOPE = 'email'


def _fetch_storage(code_review_server):
  storage_dir = os.path.expanduser(os.path.join('~', '.git_cl_credentials'))
  if not os.path.isdir(storage_dir):
    os.makedirs(storage_dir)
  storage_path = os.path.join(storage_dir, code_review_server)
  storage = Storage(storage_path)
  return storage


def _fetch_creds_from_storage(storage):
  logging.debug('Fetching OAuth2 credentials from local storage ...')
  credentials = storage.get()
  if not credentials or credentials.invalid:
    return None
  if not credentials.access_token or credentials.access_token_expired:
    return None
  return credentials


def add_oauth2_options(parser):
  """Add OAuth2-related options."""
  group = optparse.OptionGroup(parser, "OAuth2 options")
  group.add_option(
      '--auth-host-name',
      default='localhost',
      help='Host name to use when running a local web server '
           'to handle redirects during OAuth authorization.'
           'Default: localhost.'
  )
  group.add_option(
      '--auth-host-port',
      type=int,
      action='append',
      default=[8080, 8090],
      help='Port to use when running a local web server to handle '
           'redirects during OAuth authorization. '
           'Repeat this option to specify a list of values.'
           'Default: [8080, 8090].'
  )
  group.add_option(
      '--noauth-local-webserver',
      action='store_true',
      default=False,
      help='Run a local web server to handle redirects '
           'during OAuth authorization.'
           'Default: False.'
  )
  group.add_option(
      '--no-cache',
      action='store_true',
      default=False,
      help='Get fresh credentials from web server instead of using '
           'the crendentials stored on a local storage file.'
           'Default: False.'
  )
  parser.add_option_group(group)


def get_oauth2_creds(options, code_review_server):
  """Get OAuth2 credentials.

  Args:
    options: Command line options.
    code_review_server: Code review server name, e.g., codereview.chromium.org.
  """
  storage = _fetch_storage(code_review_server)
  creds = None
  if not options.no_cache:
    creds = _fetch_creds_from_storage(storage)
  if creds is None:
    logging.debug('Fetching OAuth2 credentials from web server...')
    flow = oa2client.OAuth2WebServerFlow(
        client_id=CLIENT_ID,
        client_secret=CLIENT_SECRET,
        scope=SCOPE,
        redirect_uri=REDIRECT_URI)
    flags = copy.deepcopy(options)
    flags.logging_level = 'WARNING'
    creds = tools.run_flow(flow, storage, flags)
  return creds
