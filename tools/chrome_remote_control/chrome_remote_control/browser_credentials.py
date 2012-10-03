# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from chrome_remote_control import google_credentials_backend
import logging
import json

class BrowserCredentials(object):
  def __init__(self):
    self._config = None
    self._config_file = None

    self._credentials_backends = \
        [google_credentials_backend.GoogleCredentialsBackend(self)]

  def _GetCredentialsBackend(self, credentials_type):
    for backend in self._credentials_backends:
      if credentials_type == backend.label:
        return backend

  def LoginNeeded(self, credentials_type, tab):
    backend = self._GetCredentialsBackend(credentials_type)
    if backend:
      backend.LoginNeeded(tab)
    else:
      logging.warning('Credentials not implemented: "%s"', credentials_type)

  def LoginNoLongerNeeded(self, credentials_type, tab):
    backend = self._GetCredentialsBackend(credentials_type)
    if backend:
      backend.LoginNoLongerNeeded(tab)
    else:
      logging.warning('Credentials not implemented: "%s"', credentials_type)

  def _ReadConfigFile(self):
    try:
      with open(self._config_file, 'r') as f:
        contents = f.read()
        return json.loads(contents)
    except SyntaxError, e:
      logging.info('Could not read %s: %s', self._config_file,
                   str(e))

  def GetConfig(self, config):
    if self._config is None:
      self._config = self._ReadConfigFile()

    for key in self._config:
      if self._config.get(key) is not None and key in config:
        config[key] = self._config.get(key)

    return config

  def GetCredentialsConfigFile(self):
    return self._config_file

  def SetCredentialsConfigFile(self, credentials_path):
    self._config_file = credentials_path
