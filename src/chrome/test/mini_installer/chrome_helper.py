# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common helper module for working with Chrome's processes and windows."""

import logging
import os
import psutil
import re
import win32gui
import win32process


def GetProcessIDAndPathPairs():
  """Returns a list of 2-tuples of (process id, process path).
  """
  process_id_and_path_pairs = []
  for process in psutil.process_iter():
    try:
      process_id_and_path_pairs.append((process.pid, process.exe))
    except psutil.Error:
      # It's normal that some processes are not accessible.
      pass
  return process_id_and_path_pairs


def GetProcessIDs(process_path):
  """Returns a list of IDs of processes whose path is |process_path|.

  Args:
    process_path: The path to the process.

  Returns:
    A list of process IDs.
  """
  return [pid for (pid, path) in GetProcessIDAndPathPairs() if
          path == process_path]


def WaitForChromeExit(chrome_path):
  """Waits for all |chrome_path| processes to exit.

  Args:
    chrome_path: The path to the chrome.exe on which to wait.
  """
  def GetChromeProcesses(chrome_path):
    """Returns a dict of all |chrome_path| processes indexed by pid."""
    chrome_processes = dict()
    for process in psutil.process_iter():
      try:
        if process.exe == chrome_path:
          chrome_processes[process.pid] = process
          logging.info('Found chrome process %s' % process.exe)
        elif process.name == os.path.basename(chrome_path):
          raise Exception('Found other chrome process %s' % process.exe)
      except psutil.Error:
        pass
    return chrome_processes

  def GetBrowserProcess(chrome_processes):
    """Returns a psutil.Process for the browser process in |chrome_processes|.
    """
    # Find the one whose parent isn't a chrome.exe process.
    for process in chrome_processes.itervalues():
      try:
        if process.ppid not in chrome_processes:
          return process
      except psutil.Error:
        pass
    return None

  chrome_processes = GetChromeProcesses(chrome_path)
  while chrome_processes:
    # Prefer waiting on the browser process.
    process = GetBrowserProcess(chrome_processes)
    if not process:
      # Pick any process to wait on if no top-level parent was found.
      process = next(chrome_processes.itervalues())
    if process.is_running():
      logging.info(
        'Waiting on PID %s for %s %s processes to exit' %
        (process.pid, len(chrome_processes), process.exe))
      process.wait()
    # Check for stragglers and keep waiting until all are gone.
    chrome_processes = GetChromeProcesses(chrome_path)


def GetWindowHandles(process_ids):
  """Returns a list of handles of windows owned by processes in |process_ids|.

  Args:
    process_ids: A list of process IDs.

  Returns:
    A list of handles of windows owned by processes in |process_ids|.
  """
  hwnds = []
  def EnumerateWindowCallback(hwnd, _):
    _, found_process_id = win32process.GetWindowThreadProcessId(hwnd)
    if found_process_id in process_ids and win32gui.IsWindowVisible(hwnd):
      hwnds.append(hwnd)
  # Enumerate all the top-level windows and call the callback with the hwnd as
  # the first parameter.
  win32gui.EnumWindows(EnumerateWindowCallback, None)
  return hwnds


def WindowExists(process_ids, class_pattern):
  """Returns whether there exists a window with the specified criteria.

  This method returns whether there exists a window that is owned by a process
  in |process_ids| and has a class name that matches |class_pattern|.

  Args:
    process_ids: A list of process IDs.
    class_pattern: The regular expression pattern of the window class name.

  Returns:
    A boolean indicating whether such window exists.
  """
  for hwnd in GetWindowHandles(process_ids):
    if re.match(class_pattern, win32gui.GetClassName(hwnd)):
      return True
  return False
