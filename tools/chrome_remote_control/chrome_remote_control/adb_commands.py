# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

"""This library is a thin wrapper around adb."""

# Implementation note: This is currently a thin wrapper around Android's default
# build scripts, located in chrome/build/android. Because we want
# chrome_remote_control to run without having a full download of chrome, this
# wrapper serves to audit and control our dependency on this external folder, so
# that when we go to make this code standalone, we can quickly do the change.

# Get build/android scripts into our path.
sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     "../../../build/android")))
try:
  from pylib import android_commands as real_android_commands
  from pylib import cmd_helper as real_cmd_helper
except:
  real_android_commands = None

def IsAndroidSupported():
  return real_android_commands

def GetAttachedDevices():
  """Returns a list of attached, online android devices.

  If a preferred device has been set with ANDROID_SERIAL, it will be first in
  the returned list."""
  return real_android_commands.GetAttachedDevices()

class ADBCommands(object):
  """A thin wrapper around ADB"""

  def __init__(self, device):
    self._adb = real_android_commands.AndroidCommands(device)

  def Forward(self, local, remote):
    ret = self._adb._adb.SendCommand('forward %s %s' % (local, remote))
    assert ret == ''

  def RunShellCommand(self, command, timeout_time=20, log_result=False):
    """Send a command to the adb shell and return the result.

    Args:
      command: String containing the shell command to send. Must not include
               the single quotes as we use them to escape the whole command.
      timeout_time: Number of seconds to wait for command to respond before
        retrying, used by AdbInterface.SendShellCommand.
      log_result: Boolean to indicate whether we should log the result of the
                  shell command.

    Returns:
      list containing the lines of output received from running the command
    """
    return self._adb.RunShellCommand(command, timeout_time, log_result)

  def KillAll(self, process):
    """Android version of killall, connected via adb.

    Args:
      process: name of the process to kill off

    Returns:
      the number of processess killed
    """
    return self._adb.KillAll(process)

  def ExtractPid(self, process_name):
    """Extracts Process Ids for a given process name from Android Shell.

    Args:
      process_name: name of the process on the device.

    Returns:
      List of all the process ids (as strings) that match the given name.
      If the name of a process exactly matches the given name, the pid of
      that process will be inserted to the front of the pid list.
    """
    return self._adb.ExtractPid(process_name)

  def StartActivity(self, package, activity, wait_for_completion=False,
                    action='android.intent.action.VIEW',
                    category=None, data=None,
                    extras=None, trace_file_name=None):
    """Starts |package|'s activity on the device.

    Args:
      package: Name of package to start (e.g. 'com.google.android.apps.chrome').
      activity: Name of activity (e.g. '.Main' or
        'com.google.android.apps.chrome.Main').
      wait_for_completion: wait for the activity to finish launching (-W flag).
      action: string (e.g. "android.intent.action.MAIN"). Default is VIEW.
      category: string (e.g. "android.intent.category.HOME")
      data: Data string to pass to activity (e.g. 'http://www.example.com/').
      extras: Dict of extras to pass to activity. Values are significant.
      trace_file_name: If used, turns on and saves the trace to this file name.
    """
    return self._adb.StartActivity(package, activity, wait_for_completion,
                    action,
                    category, data,
                    extras, trace_file_name)

  def Push(self, local, remote):
    return self._adb.Adb().Push(local, remote)
