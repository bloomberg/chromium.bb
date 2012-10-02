# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

# Get chrome/test/functional scripts into our path.
# TODO(tonyg): Move webpagereplay.py to a common location.
sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     '../../../chrome/test/functional')))
import webpagereplay

CHROME_FLAGS = webpagereplay.CHROME_FLAGS

class ReplayServer(object):
  def __init__(self, browser_backend, path, is_record_mode):
    self._browser_backend = browser_backend
    self._http_forwarder = None
    self._https_forwarder = None
    self._web_page_replay = None
    self._is_record_mode = is_record_mode

    self._http_forwarder = browser_backend.CreateForwarder(
        webpagereplay.HTTP_PORT)
    self._https_forwarder = browser_backend.CreateForwarder(
        webpagereplay.HTTPS_PORT)

    options = []
    if self._is_record_mode:
      options.append('--record')
    self._web_page_replay = webpagereplay.ReplayServer(path, options)
    self._web_page_replay.StartServer()

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def Close(self):
    if self._browser_backend:
      self._browser_backend.Close()
    if self._http_forwarder:
      self._http_forwarder.Close()
      self._http_forwarder = None
    if self._https_forwarder:
      self._https_forwarder.Close()
      self._https_forwarder = None
    if self._web_page_replay:
      self._web_page_replay.StopServer()
      self._web_page_replay = None

class DoNothingReplayServer(object):
  def __init__(self, browser_backend, path, is_record_mode):
    pass

  def __enter__(self):
    return self

  def __exit__(self, *args):
    pass
