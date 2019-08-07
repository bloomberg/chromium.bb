# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import os
import platform
import subprocess
import sys
import time

from telemetry.core import os_version as os_version_module
from telemetry import decorators
from telemetry.internal.platform import posix_platform_backend
from telemetry.util import process_statistic_timeline_data


class MacPlatformBackend(posix_platform_backend.PosixPlatformBackend):
  def __init__(self):
    super(MacPlatformBackend, self).__init__()
    self.libproc = None

  def GetSystemLog(self):
    # Since the log file can be very large, only show the last 200 lines.
    return subprocess.check_output(
        ['tail', '-n', '200', '/var/log/system.log'])

  @classmethod
  def IsPlatformBackendForHost(cls):
    return sys.platform == 'darwin'

  def IsThermallyThrottled(self):
    raise NotImplementedError()

  def HasBeenThermallyThrottled(self):
    raise NotImplementedError()

  def _GetIdleWakeupCount(self, pid):
    top_output = self._GetTopOutput(pid, ['idlew'])

    # Sometimes top won't return anything here, just ignore such cases -
    # crbug.com/354812 .
    if top_output[-2] != 'IDLEW':
      return process_statistic_timeline_data.IdleWakeupTimelineData(pid, 0)
    # Numbers reported by top may have a '+' appended.
    wakeup_count = int(top_output[-1].strip('+ '))
    return process_statistic_timeline_data.IdleWakeupTimelineData(
        pid, wakeup_count)

  def GetCpuStats(self, pid):
    """Returns a dict of cpu statistics for the process represented by |pid|."""
    class ProcTaskInfo(ctypes.Structure):
      """Struct for proc_pidinfo() call."""
      _fields_ = [("pti_virtual_size", ctypes.c_uint64),
                  ("pti_resident_size", ctypes.c_uint64),
                  ("pti_total_user", ctypes.c_uint64),
                  ("pti_total_system", ctypes.c_uint64),
                  ("pti_threads_user", ctypes.c_uint64),
                  ("pti_threads_system", ctypes.c_uint64),
                  ("pti_policy", ctypes.c_int32),
                  ("pti_faults", ctypes.c_int32),
                  ("pti_pageins", ctypes.c_int32),
                  ("pti_cow_faults", ctypes.c_int32),
                  ("pti_messages_sent", ctypes.c_int32),
                  ("pti_messages_received", ctypes.c_int32),
                  ("pti_syscalls_mach", ctypes.c_int32),
                  ("pti_syscalls_unix", ctypes.c_int32),
                  ("pti_csw", ctypes.c_int32),
                  ("pti_threadnum", ctypes.c_int32),
                  ("pti_numrunning", ctypes.c_int32),
                  ("pti_priority", ctypes.c_int32)]
      PROC_PIDTASKINFO = 4
      def __init__(self):
        self.size = ctypes.sizeof(self)
        super(ProcTaskInfo, self).__init__()  # pylint: disable=bad-super-call

    proc_info = ProcTaskInfo()
    if not self.libproc:
      self.libproc = ctypes.CDLL(ctypes.util.find_library('libproc'))
    self.libproc.proc_pidinfo(pid, proc_info.PROC_PIDTASKINFO, 0,
                              ctypes.byref(proc_info), proc_info.size)

    # Convert nanoseconds to seconds.
    cpu_time = (proc_info.pti_total_user / 1000000000.0 +
                proc_info.pti_total_system / 1000000000.0)
    results = {'CpuProcessTime': cpu_time,
               'ContextSwitches': proc_info.pti_csw}

    # top only reports idle wakeup count starting from OS X 10.9.
    if self.GetOSVersionName() >= os_version_module.MAVERICKS:
      results.update({'IdleWakeupCount': self._GetIdleWakeupCount(pid)})
    return results

  def GetCpuTimestamp(self):
    """Return current timestamp in seconds."""
    return {'TotalTime': time.time()}

  @decorators.Cache
  def GetSystemTotalPhysicalMemory(self):
    return int(self.RunCommand(['sysctl', '-n', 'hw.memsize']))

  @decorators.Cache
  def GetArchName(self):
    return platform.machine()

  def GetOSName(self):
    return 'mac'

  @decorators.Cache
  def GetOSVersionName(self):
    os_version = os.uname()[2]

    if os_version.startswith('9.'):
      return os_version_module.LEOPARD
    if os_version.startswith('10.'):
      return os_version_module.SNOWLEOPARD
    if os_version.startswith('11.'):
      return os_version_module.LION
    if os_version.startswith('12.'):
      return os_version_module.MOUNTAINLION
    if os_version.startswith('13.'):
      return os_version_module.MAVERICKS
    if os_version.startswith('14.'):
      return os_version_module.YOSEMITE
    if os_version.startswith('15.'):
      return os_version_module.ELCAPITAN
    if os_version.startswith('16.'):
      return os_version_module.SIERRA
    if os_version.startswith('17.'):
      return os_version_module.HIGHSIERRA
    if os_version.startswith('18.'):
      return os_version_module.MOJAVE

    raise NotImplementedError('Unknown mac version %s.' % os_version)

  @decorators.Cache
  def GetOSVersionDetailString(self):
    product = subprocess.check_output(['sw_vers', '-productVersion']).strip()
    build = subprocess.check_output(['sw_vers', '-buildVersion']).strip()
    return product + ' ' + build

  def CanTakeScreenshot(self):
    return True

  def TakeScreenshot(self, file_path):
    return subprocess.call(['screencapture', file_path])

  def CanFlushIndividualFilesFromSystemCache(self):
    return False

  def SupportFlushEntireSystemCache(self):
    return self.HasRootAccess()

  def FlushEntireSystemCache(self):
    mavericks_or_later = self.GetOSVersionName() >= os_version_module.MAVERICKS
    p = self.LaunchApplication('purge', elevate_privilege=mavericks_or_later)
    p.communicate()
    assert p.returncode == 0, 'Failed to flush system cache'

  def CanMonitorPower(self):
    return False

  def CanMeasurePerApplicationPower(self):
    return False
