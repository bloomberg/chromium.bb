# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import getpass
import os
import urllib

DEFAULT_GAIA_URL = "https://www.google.com:443/accounts/ClientLogin"

class GaiaAuthenticator:
  def __init__(self, service, url = DEFAULT_GAIA_URL):
    self._service = service
    self._url = url

  ## Logins to gaia and returns auth token.
  def authenticate(self, email, passwd):
    params = urllib.urlencode({'Email': email, 'Passwd': passwd,
                               'source': 'chromoting',
                               'service': self._service,
                               'PersistentCookie': 'true',
                               'accountType': 'GOOGLE'})
    f = urllib.urlopen(self._url, params);
    result = f.read()
    for line in result.splitlines():
      if line.startswith('Auth='):
        auth_string = line[5:]
        return auth_string
    raise Exception("Gaia didn't return auth token: " + result)
