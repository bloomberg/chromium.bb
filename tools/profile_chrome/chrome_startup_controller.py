# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import time

from pylib import flag_changer
from pylib.device import intent
from pylib.perf import cache_control

from profile_chrome import controllers

class ChromeStartupTracingController(controllers.BaseController):
  def __init__(self, device, package_info, cold, url):
    self._device = device
    self._package_info = package_info
    self._cold = cold
    self._logcat_monitor = self._device.GetLogcatMonitor()
    self._url = url
    self._trace_file = None
    self._trace_finish_re = re.compile(r' Completed startup tracing to (.*)')

  def __repr__(self):
    return 'Browser Startup Trace'

  def _SetupTracing(self):
    # TODO(lizeb): Figure out how to clean up the command-line file when
    # _TearDownTracing() is not executed in StopTracing().
    changer = flag_changer.FlagChanger(
        self._device, self._package_info.cmdline_file)
    changer.AddFlags(['--trace-startup'])
    self._device.ForceStop(self._package_info.package)
    if self._cold:
      self._device.EnableRoot()
      cache_control.CacheControl(self._device).DropRamCaches()
    self._device.StartActivity(
        intent.Intent(
            package=self._package_info.package,
            activity=self._package_info.activity,
            data=self._url,
            extras={'create_new_tab' : True}), blocking=True)

  def _TearDownTracing(self):
    changer = flag_changer.FlagChanger(
        self._device, self._package_info.cmdline_file)
    changer.RemoveFlags(['--trace-startup'])

  def StartTracing(self, interval):
    self._SetupTracing()
    self._logcat_monitor.Start()

  def StopTracing(self):
    try:
      self._trace_file = self._logcat_monitor.WaitFor(
          self._trace_finish_re).group(1)
    finally:
      self._TearDownTracing()

  def PullTrace(self):
    # Wait a bit for the browser to finish writing the trace file.
    time.sleep(3)
    trace_file = self._trace_file.replace('/storage/emulated/0/', '/sdcard/')
    host_file = os.path.join(os.path.curdir, os.path.basename(trace_file))
    self._device.PullFile(trace_file, host_file)
    return host_file
