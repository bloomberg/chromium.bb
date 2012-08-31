# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import inspector_backend

class EvaluateException(Exception):
  pass

class TabRuntime(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self._inspector_backend.RegisterDomain(
        'Runtime',
        self._OnNotification,
        self._OnClose)

  def _OnNotification(self, msg):
    pass

  def _OnClose(self):
    pass

  def Evaluate(self, expr):
    request = {
      "method": "Runtime.evaluate",
      "params": {
        "expression": expr,
        "returnByValue": True
        }
      }
    res = self._inspector_backend.SyncRequest(request)
    if "error" in res:
      raise inspector_backend.InspectorException(res["error"]["message"])

    if res["result"]["wasThrown"]:
      # TODO(nduca): propagate stacks from javascript up to the python
      # exception.
      raise EvaluateException(res["result"]["result"]["description"])
    if res["result"]["result"]["type"] == 'undefined':
      return None
    return res["result"]["result"]["value"]

