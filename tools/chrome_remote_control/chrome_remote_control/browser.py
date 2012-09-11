# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess
import urllib2
import json
import tab

import browser_finder
import temporary_http_server

class Browser(object):
  """A running browser instance that can be controled in a limited way.

  To create a browser instance, use browser_finder.FindBrowser.

  Be sure to clean up after yourself by calling Close() when you are done with
  the browser. Or better yet:
    browser_to_create = FindBrowser(options)
    with browser_to_create.Create() as browser:
      ... do all your operations on browser here
  """
  def __init__(self, backend):
    self._backend = backend

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  @property
  def is_content_shell(self):
    """Returns whether this browser is a content shell, only."""
    return self._backend.is_content_shell

  @property
  def num_tabs(self):
    return self._backend.num_tabs

  def GetNthTabUrl(self, index):
    return self._backend.GetNthTabUrl(index)

  def ConnectToNthTab(self, index):
    return self._backend.ConnectToNthTab(index)

  def Close(self):
    self._backend.Close()

  def CreateTemporaryHTTPServer(self, path):
    return temporary_http_server.TemporaryHTTPServer(self._backend, path)
