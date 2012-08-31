# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import websocket
import socket
import time

import tab_runtime
import util

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

  def BeginToLoadUrl(self, url):
    # In order to tell when the document has actually changed,
    # we go to about:blank first and wait. When that has happened, we
    # to go the new URL and detect the document being non-about:blank as
    # indication that the new document is loading.
    self.runtime.Evaluate('document.location = "about:blank";')
    util.WaitFor(lambda:
        self.runtime.Evaluate('document.location.href') == 'about:blank')

    self.runtime.Evaluate('document.location = "%s";' % url)
    util.WaitFor(lambda:
        self.runtime.Evaluate('document.location.href') != 'about:blank')

  def LoadUrl(self, url):
    self.BeginToLoadUrl(url)
    # TODO(dtu): Detect HTTP redirects.
    time.sleep(2)  # Wait for unpredictable redirects.
    self.WaitForDocumentReadyStateToBeInteractiveOrBetter()

  def WaitForDocumentReadyStateToBeComplete(self):
    util.WaitFor(
        lambda: self.runtime.Evaluate('document.readyState') == 'complete')

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self):
    def IsReadyStateInteractiveOrBetter():
      rs = self.runtime.Evaluate('document.readyState')
      return rs == 'complete' or rs == 'interactive'
    util.WaitFor(IsReadyStateInteractiveOrBetter)
