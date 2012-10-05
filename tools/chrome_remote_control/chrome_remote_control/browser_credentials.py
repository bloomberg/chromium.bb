# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import json
import os

from chrome_remote_control import google_credentials_backend

class BrowserCredentials(object):
  def __init__(self, backends = None):
    self._credentials = {}
    self._credentials_path = None

    if backends is None:
      backends = [
        google_credentials_backend.GoogleCredentialsBackend()]

    self._backends = {}
    for backend in backends:
      self._backends[backend.credentials_type] = backend

  def AddBackend(self, backend):
    assert backend.credentials_type not in self._backends
    self._backends[backend.credentials_type] = backend

  def CanLogin(self, credentials_type):
    if credentials_type not in self._backends:
      raise Exception('Unrecognized credentials type: %s', credentials_type)
    return credentials_type in self._credentials

  def LoginNeeded(self, tab, credentials_type):
    if credentials_type not in self._backends:
      raise Exception('Unrecognized credentials type: %s', credentials_type)
    if credentials_type not in self._credentials:
      return False
    return self._backends[credentials_type].LoginNeeded(
      tab, self._credentials[credentials_type])

  def LoginNoLongerNeeded(self, tab, credentials_type):
    assert credentials_type in self._backends
    self._backends[credentials_type].LoginNoLongerNeeded(tab)

  @property
  def credentials_path(self):
    return self._credentials_path

  @credentials_path.setter
  def credentials_path(self, credentials_path):
    self._credentials_path = credentials_path
    self._ReadCredentialsFile()

  def _ReadCredentialsFile(self):
    credentials = {}
    if self._credentials_path == None:
      pass
    elif os.path.exists(self._credentials_path):
      with open(self._credentials_path, 'r') as f:
        credentials = json.loads(f.read())
    else:
      logging.warning(
        "%s was not found. Sites that require login may not run." %
        self._credentials_path)

    # TODO(nduca): use system keychain, if possible.
    homedir_credentials_path = os.path.expanduser('~/.crc-credentials')
    homedir_credentials = {}

    # Workaround pylint issues by importing browser_options with voodo.
    import sys
    if 'chrome_remote_control.browser_options' in sys.modules:
      browser_options = sys.modules['chrome_remote_control.browser_options']
    else:
      browser_options = None

    if (browser_options and
        browser_options.options_for_unittests and
        os.path.exists(homedir_credentials_path)):
      logging.info("Found ~/.crc-credentials. Its contents will be used when "
                   "no other credentials can be found.")
      with open(homedir_credentials_path, 'r') as f:
        homedir_credentials = json.loads(f.read())

    self._credentials = {}
    all_keys = set(credentials.keys()).union(homedir_credentials.keys())
    for k in all_keys:
      if k in credentials:
        self._credentials[k] = credentials[k]
      if k in homedir_credentials:
        logging.info("Will use ~/.crc-credentials for %s logins." % k)
