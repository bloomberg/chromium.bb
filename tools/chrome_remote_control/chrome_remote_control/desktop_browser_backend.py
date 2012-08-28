# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess
import shutil
import tempfile
import urllib2
import json

import browser_finder
import inspector_backend
import tab

DEFAULT_PORT = 9273

class DesktopBrowserBackend(object):
  """The backend for controlling a locally-executed browser instance, on Linux,
  Mac or Windows.
  """
  def __init__(self, executable, options):
    # Initialize fields so that an exposion during init doesn't break in Close.
    self._proc = None
    self._devnull = None
    self._tmpdir = None

    self._executable = executable
    if not self._executable:
      raise Exception("Cannot create browser, no executable found!")

    # TODO(nduca): Delete the user data dir or create one that is unique.
    self._tmpdir = tempfile.mkdtemp()
    self._port = DEFAULT_PORT
    args = [self._executable,
            "--no-first-run",
            "--remote-debugging-port=%i" % self._port]
    if not options.dont_override_profile:
      args.append("--user-data-dir=%s" % self._tmpdir)
    args.extend(options.extra_browser_args)
    if options.hide_stdout:
      self._devnull = open(real_os.devnull, 'w')
      self._proc = real_subprocess.Popen(
        args,stdout=self._devnull, stderr=self._devnull)
    else:
      self._devnull = None
      self._proc = real_subprocess.Popen(args)

    try:
      self._WaitForBrowserToComeUp()
    except:
      self.Close()
      raise

  def __del__(self):
    self.Close()

  def _WaitForBrowserToComeUp(self):
    while True:
      try:
        self._ListTabs()
        break
      except urllib2.URLError, ex:
        pass

  def _ListTabs(self):
    req = urllib2.urlopen("http://localhost:%i/json" % self._port)
    data = req.read()
    return json.loads(data)

  @property
  def num_tabs(self):
    return len(self._ListTabs())

  def GetNthTabURL(self, index):
    return self._ListTabs()[index]["url"]

  def ConnectToNthTab(self, index):
    ib = inspector_backend.InspectorBackend(self, self._ListTabs()[index])
    return tab.Tab(ib)

  def Close(self):
    if self._proc:
      self._proc.terminate()
      self._proc.wait()
      self._proc = None

    if real_os.path.exists(self._tmpdir):
      shutil.rmtree(self._tmpdir, ignore_errors=True)

    if self._devnull:
      self._devnull.close()
