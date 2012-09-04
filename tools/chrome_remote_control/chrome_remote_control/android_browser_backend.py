# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os as real_os
import sys as real_sys
import shutil
import tempfile
import urllib2
import json

import browser_backend
import browser_finder
import inspector_backend
import tab

class AndroidBrowserBackend(browser_backend.BrowserBackend):
  """The backend for controlling a browser instance running on Android.
  """
  def __init__(self, type, options, adb,
               package, is_content_shell, cmdline_file, activity,
               devtools_remote_port):
    super(AndroidBrowserBackend, self).__init__(is_content_shell)
    # Initialize fields so that an explosion during init doesn't break in Close.
    self._options = options
    self._adb = adb
    self._package = package
    self._cmdline_file = cmdline_file
    self._activity = activity
    self._port = 9222
    self._devtools_remote_port = devtools_remote_port

    # Beginnings of a basic command line.
    if is_content_shell:
      pseudo_exec_name = "content_shell"
    else:
      pseudo_exec_name = "chrome"
    args = [pseudo_exec_name,
            "--disable-fre", "--no-first-run"]

    # Kill old broser.
    self._adb.KillAll(self._package)
    self._adb.KillAll('forawrder')
    self._adb.Forward('tcp:9222', self._devtools_remote_port)

    # Set up temporary dir if needed. As far as we can tell, content_shell
    # doesn't have persisted data, so --user-data-dir isn't needed.
    if not is_content_shell and not options.dont_override_profile:
      self._tmpdir = "/sdcard/chrome_remote_control_data"
      self._adb.RunShellCommand('rm -r %s' %  self._tmpdir)
      args.append("--user-data-dir=%s" % self._tmpdir)

    # Set up the command line.
    args.extend(options.extra_browser_args)
    with tempfile.NamedTemporaryFile() as f:
      f.write(" ".join(args))
      f.flush()
      self._adb.Push(f.name, cmdline_file)

    # Start it up!
    self._adb.StartActivity(self._package,
                            self._activity,
                            True,
                            None,
                            "chrome://newtab/")
    try:
      self._WaitForBrowserToComeUp()
    except:
      import traceback
      traceback.print_exc()
      self.Close()
      raise

  def __del__(self):
    self.Close()

  def Close(self):
    self._adb.KillAll(self._package)

  def IsBrowserRunning(self):
    pids = self._adb.ExtractPid(self._package)
    return len(pids) != 0
