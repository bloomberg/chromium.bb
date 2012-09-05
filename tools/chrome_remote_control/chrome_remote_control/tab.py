# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import websocket
import socket
import time

import tab_page
import tab_runtime
import util

DEFAULT_TAB_TIMEOUT=60

class Tab(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self.page = tab_page.TabPage(self._inspector_backend)
    self.runtime = tab_runtime.TabRuntime(self._inspector_backend)


  def __del__(self):
    self.Close()

  def Close(self):
    self.page = None
    self.runtime = None
    if self._inspector_backend:
      self._inspector_backend.Close()
      self._inspector_backend = None

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def WaitForDocumentReadyStateToBeComplete(self, timeout=60):
    util.WaitFor(
        lambda: self.runtime.Evaluate('document.readyState') == 'complete',
        timeout)

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self, timeout=60):
    def IsReadyStateInteractiveOrBetter():
      rs = self.runtime.Evaluate('document.readyState')
      return rs == 'complete' or rs == 'interactive'
    util.WaitFor(IsReadyStateInteractiveOrBetter, timeout)
