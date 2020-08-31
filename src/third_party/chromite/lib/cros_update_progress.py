# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A progress class for tracking CrOS auto-update process.

This class is mainly designed for:
  1. Set the pattern for generating the filenames of
     track_status_file/execute_log_file.
     track_status_file: Used for record the current step of CrOS auto-update
                        process. Only has one line.
     execute_log_file: Used for record the whole logging info of the CrOS
                       auto-update process, including any debug information.
  2. Write current auto-update process into the track_status_file.
  3. Read current auto-update process from the track_status_file.

This file also offers external functions that are related to add/check/delete
the progress of the CrOS auto-update process.
"""

from __future__ import print_function

import datetime
import glob
import os
import re

from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib.xbuddy import cherrypy_log_util


# Module-local log function.
def _Log(message, *args):
  return cherrypy_log_util.LogWithTag('CROS_UPDATE_PROGRESS', message, *args)

# Path for status tracking log.
_TRACK_LOG_FILE_PATH = '/tmp/auto-update/tracking_log/%s_%s.log'

# Pattern for status tracking log filename.
_TRACK_LOG_FILE_NAME_PATTERN = r'([^_]+)_([^_]+).log'

# The gap hour used in checking AU processes' count.
AU_PROCESS_HOUR_GAP = 3

# Path for executing log.
_EXECUTE_LOG_FILE_PATH = '/tmp/auto-update/executing_log/%s_%s.log'

# Path and files for temporarily saving devserver codes, devserver and
# update engine log.
_CROS_UPDATE_TEMP_PATH = '/tmp/cros-update_%s_%s'

_CROS_HOSTLOG_PATTERN = 'devserver_hostlog*'

# The string for update process finished.
FINISHED = 'Completed'
ERROR_TAG = 'Error'


def IsProcessAlive(pid):
  """Detect whether a process is alive.

  Args:
    pid: The process id.

  Returns:
    True if the process is still alive. False otherwise.
  """
  path = '/proc/%s/stat' % pid
  if not os.path.exists(path):
    return False

  with open(path, 'r') as fp:
    stat = fp.readline().rstrip('\n')

  return stat.split()[2] != 'Z'


def GetExecuteLogFile(host_name, pid):
  """Return the whole path of execute log file."""
  osutils.SafeMakedirs(os.path.dirname(_EXECUTE_LOG_FILE_PATH))

  return _EXECUTE_LOG_FILE_PATH % (host_name, pid)


def GetTrackStatusFile(host_name, pid):
  """Return the whole path of track status file."""
  osutils.SafeMakedirs(os.path.dirname(_TRACK_LOG_FILE_PATH))

  return _TRACK_LOG_FILE_PATH % (host_name, pid)


def GetAllTrackStatusFileByTime():
  """Return all track status files existing in TRACK_LOG_FILE_PATH.

  Returns:
    A track status file list ordered by created time reversely.
  """
  return sorted(glob.glob(_TRACK_LOG_FILE_PATH % ('*', '*')),
                key=os.path.getctime, reverse=True)


def ParsePidFromTrackLogFileName(track_log_filename):
  """Parse pid from a given track log file's name.

  The track log file's name for auto-update is fixed:
      hostname_pid.log

  This func is used to parse pid from a given track log file.

  Args:
    track_log_filename: the filename of the track log to be parsed.

  Returns:
    the parsed pid (int).
  """
  match = re.match(_TRACK_LOG_FILE_NAME_PATTERN, track_log_filename)
  try:
    return int(match.groups()[1])
  except (AttributeError, IndexError, ValueError) as e:
    _Log('Cannot parse pid from track log file %s: %s', track_log_filename, e)
    return None


def GetAllTrackStatusFileByHostName(host_name):
  """Return a list of existing track status files generated for a host."""
  return glob.glob(_TRACK_LOG_FILE_PATH % (host_name, '*'))


def GetAllRunningAUProcess():
  """Get all the ongoing AU processes' pids from tracking logs.

  This func only checks the tracking logs generated in latest several hours,
  which is for avoiding the case that 'there's a running process whose id is
  as the same as a previous AU process'.

  Returns:
    A list of background AU processes' pids.
  """
  pids = []
  now = datetime.datetime.now()
  track_log_list = GetAllTrackStatusFileByTime()
  # Only check log file created in 3 hours.
  for track_log in track_log_list:
    try:
      created_time = datetime.datetime.fromtimestamp(
          os.path.getctime(track_log))
      if now - created_time >= datetime.timedelta(hours=AU_PROCESS_HOUR_GAP):
        break

      pid = ParsePidFromTrackLogFileName(os.path.basename(track_log))
      if pid and IsProcessAlive(pid):
        pids.append(pid)
    except (ValueError, OSError) as e:
      _Log('Error happened in getting pid from %s: %s', track_log, e)

  return pids


def GetAUTempDirectory(host_name, pid):
  """Return the temp dir for storing codes and logs during auto-update."""
  au_tempdir = _CROS_UPDATE_TEMP_PATH % (host_name, pid)
  osutils.SafeMakedirs(au_tempdir)

  return au_tempdir


def ReadExecuteLogFile(host_name, pid):
  """Return the content of execute log file."""
  return osutils.ReadFile(GetExecuteLogFile(host_name, pid))


def ReadAUHostLogFiles(host_name, pid):
  """Returns a dictionary containing the devserver host log files."""
  au_dir = GetAUTempDirectory(host_name, pid)
  hostlog_filenames = glob.glob(os.path.join(au_dir, _CROS_HOSTLOG_PATTERN))
  hostlog_files = {}
  for f in hostlog_filenames:
    hostlog_files[os.path.basename(f)] = osutils.ReadFile(f)
  return hostlog_files


def DelTrackStatusFile(host_name, pid):
  """Delete the track status log."""
  osutils.SafeUnlink(GetTrackStatusFile(host_name, pid))


def DelExecuteLogFile(host_name, pid):
  """Delete the track status log."""
  osutils.SafeUnlink(GetExecuteLogFile(host_name, pid))


def DelAUTempDirectory(host_name, pid):
  """Delete the directory including auto-update-related logs."""
  osutils.RmDir(GetAUTempDirectory(host_name, pid))


class AUProgress(object):
  """Used for tracking the CrOS auto-update progress."""

  def __init__(self, host_name, pid):
    """Initialize a CrOS update progress instance.

    Args:
      host_name: The name of host, should be in the file_name of the status
        tracking file of auto-update process.
      pid: The process id, should be in the file_name too.
    """
    self.host_name = host_name
    self.pid = pid

  @property
  def track_status_file(self):
    """The track status file to record the CrOS auto-update progress."""
    return GetTrackStatusFile(self.host_name, self.pid)

  def WriteStatus(self, content):
    """Write auto-update progress into status tracking file.

    Args:
      content: The content to be recorded.
    """
    if not self.track_status_file:
      return

    try:
      osutils.WriteFile(self.track_status_file, content)
    except IOError as e:
      logging.error('Cannot write au status: %r', e)

  def ReadStatus(self):
    """Read auto-update progress from status tracking file."""
    return osutils.ReadFile(self.track_status_file).rstrip('\n')
