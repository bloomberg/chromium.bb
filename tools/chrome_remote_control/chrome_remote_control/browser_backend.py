# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import urllib2
import httplib
import json

from chrome_remote_control import inspector_backend
from chrome_remote_control import tab
from chrome_remote_control import util

class BrowserGoneException(Exception):
  pass

class BrowserBackend(object):
  """A base class for broser backends. Provides basic functionality
  once a remote-debugger port has been established."""
  def __init__(self, is_content_shell):
    self.is_content_shell = is_content_shell
    self._port = None

  def _WaitForBrowserToComeUp(self):
    def IsBrowserUp():
      try:
        self._ListTabs()
      except httplib.BadStatusLine:
        if not self.IsBrowserRunning():
          raise BrowserGoneException()
        return False
      except urllib2.URLError:
        if not self.IsBrowserRunning():
          raise BrowserGoneException()
        return False
      else:
        return True
    try:
      util.WaitFor(IsBrowserUp, timeout=30)
    except util.TimeoutException:
      raise BrowserGoneException()

  def _ListTabs(self, timeout=None):
    if timeout:
      req = urllib2.urlopen('http://localhost:%i/json' % self._port,
                            timeout=timeout)
    else:
      req = urllib2.urlopen('http://localhost:%i/json' % self._port)
    data = req.read()
    return json.loads(data)

  @property
  def num_tabs(self):
    return len(self._ListTabs())

  def GetNthTabUrl(self, index):
    return self._ListTabs()[index]['url']

  def ConnectToNthTab(self, index):
    ib = inspector_backend.InspectorBackend(self, self._ListTabs()[index])
    return tab.Tab(self, ib)

  def CreateForwarder(self, host_port):
    raise NotImplementedError()

  def IsBrowserRunning(self):
    raise NotImplementedError()
