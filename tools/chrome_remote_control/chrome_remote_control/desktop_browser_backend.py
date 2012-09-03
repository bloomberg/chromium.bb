# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as os
import sys as sys
import subprocess as subprocess
import shutil
import tempfile

import browser_backend
import browser_finder
import inspector_backend
import tab

DEFAULT_PORT = 9273

class DesktopBrowserBackend(browser_backend.BrowserBackend):
  """The backend for controlling a locally-executed browser instance, on Linux,
  Mac or Windows.
  """
  def __init__(self, options, executable):
    super(DesktopBrowserBackend, self).__init__()

    # Initialize fields so that an explosion during init doesn't break in Close.
    self._proc = None
    self._devnull = None
    self._tmpdir = None

    self._executable = executable
    if not self._executable:
      raise Exception("Cannot create browser, no executable found!")

    self._tmpdir = tempfile.mkdtemp()
    self._port = DEFAULT_PORT
    args = [self._executable,
            "--no-first-run",
            "--remote-debugging-port=%i" % self._port]
    if not options.dont_override_profile:
      args.append("--user-data-dir=%s" % self._tmpdir)
    args.extend(options.extra_browser_args)
    if options.hide_stdout:
      self._devnull = open(os.devnull, 'w')
      self._proc = subprocess.Popen(
        args,stdout=self._devnull, stderr=self._devnull)
    else:
      self._devnull = None
      self._proc = subprocess.Popen(args)

    try:
      self._WaitForBrowserToComeUp()
    except:
      self.Close()
      raise

  def IsBrowserRunning(self):
    return not self._proc.poll()

  def __del__(self):
    self.Close()

  def Close(self):
    if self._proc:
      self._proc.terminate()
      self._proc.wait()
      self._proc = None

    if os.path.exists(self._tmpdir):
      shutil.rmtree(self._tmpdir, ignore_errors=True)

    if self._devnull:
      self._devnull.close()
