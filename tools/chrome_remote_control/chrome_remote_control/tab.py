# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import time
import websocket
import socket

import tab_runtime

class Tab(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self.runtime = tab_runtime.TabRuntime(self._inspector_backend)

  def __del__(self):
    self.Close()

  def Close(self):
    if self._inspector_backend:
      self._inspector_backend.Close()
      self._inspector_backend = None

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def BeginToLoadURL(self, url):
    # In order to tell when the document has actually changed,
    # we go to about:blank first and wait. When that has happened, we
    # to go the new URL and detect the document being non-about:blank as
    # indication that the new document is loading.
    self.runtime.Evaluate("document.location = 'about:blank';")
    while self.runtime.Evaluate("document.location.href") != "about:blank":
      time.sleep(0.01)

    self.runtime.Evaluate("document.location = '%s';" % url)
    while self.runtime.Evaluate("document.location.href") == "about:blank":
      time.sleep(0.01)

  def WaitForDocumentReadyStateToBeComplete(self):
    while self.runtime.Evaluate("document.readyState") != 'complete':
      time.sleep(0.01)

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self):
    while True:
      rs = self.runtime.Evaluate("document.readyState")
      if rs == 'complete' or rs == 'interactive':
        break
      time.sleep(0.01)
