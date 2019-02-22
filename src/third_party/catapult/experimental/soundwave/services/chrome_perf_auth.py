# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import oauth2client.client
import oauth2client.file
import oauth2client.tools


OAUTH_CLIENT_ID = (
    '62121018386-h08uiaftreu4dr3c4alh3l7mogskvb7i.apps.googleusercontent.com')
OAUTH_CLIENT_SECRET = 'vc1fZfV1cZC6mgDSHV-KSPOz'
SCOPES = ['https://www.googleapis.com/auth/userinfo.email']
USER_CREDENTIALS_PATH = os.path.normpath(os.path.join(
    os.path.dirname(__file__), '..', 'user-credentials.json'))


def GetUserCredentials(flags):
  """Get user account credentials for the performance dashboard.

  Args:
    flags: An argparse.Namespace as returned by oauth2client.tools.argparser
      (or an argparser which includes that as a parent).
  """
  store = oauth2client.file.Storage(USER_CREDENTIALS_PATH)
  if os.path.exists(USER_CREDENTIALS_PATH):
    credentials = store.locked_get()
    if not credentials.invalid:
      return credentials
  flow = oauth2client.client.OAuth2WebServerFlow(
      OAUTH_CLIENT_ID, OAUTH_CLIENT_SECRET, SCOPES,
      access_type='offline', include_granted_scopes='true',
      prompt='consent')
  return oauth2client.tools.run_flow(flow, store, flags)
