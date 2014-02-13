# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import tempfile


class MockAdb(object):
  """Simple and effective mock for Android adb.

  This module is meant to be used in integration tests in conjunction with the
  other script named 'adb' living in the same directory.
  The purpose of this class is preparing a dictionary of mocked responses to adb
  commands. The dictionary is stored into a json file and read by the mock 'adb'
  script. The path of the json dict is held in the MOCK_ADB_CFG env var.
  """

  def __init__(self):
    self._responses = {}  # Maps expected_cmd (str) -> prepared_response (str).
    self._cfg_file = None

  def Start(self):
    (fd_num, self._cfg_file) = tempfile.mkstemp()
    with os.fdopen(fd_num, 'w') as f:
      json.dump(self._responses, f)
      f.close()
    os.environ['MOCK_ADB_CFG'] = self._cfg_file

  def Stop(self):
    assert(self._cfg_file), 'Stop called before Start.'
    os.unlink(self._cfg_file)

  def PrepareResponse(self, cmd, response):
    self._responses[cmd] = response
