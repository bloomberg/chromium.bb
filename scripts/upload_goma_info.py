# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Uploads goma log info to storage.

Note that build/ has similar concept scripts.
cf)
https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/goma_utils.py
https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/upload_goma_logs.py

These and this scripts should be maintained in almost sync.
"""

from __future__ import print_function

import collections
import datetime
import glob
import json
import os
import re

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs


# The Google Cloud Storage bucket to store logs related to goma.
GOMA_LOG_GS_BUCKET = 'chrome-goma-log'

# Timestamp format in a log file.
_TIMESTAMP_PATTERN = re.compile(r'(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2})')
_TIMESTAMP_FORMAT = '%Y/%m/%d %H:%M:%S'


def _GetGomaLogDirectory():
  """Returns goma's log directory.

  Returns:
    a string of a directory name where goma's log may exist.
  """
  candidates = ['GLOG_log_dir', 'GOOGLE_LOG_DIR', 'TEST_TMPDIR',
                'TMPDIR', 'TMP']
  for candidate in candidates:
    value = os.environ.get(candidate)
    if value and os.path.isdir(value):
      return value

  # Fallback to /tmp.
  return '/tmp'


def _GetLogFileTimestamp(path):
  """Returns timestamp when the given glog log was created.

  Args:
    path: a path to a google-glog log file.

  Returns:
    datetime.datetime instance when the log file was created.
    Or, None, if it is not a glog file.
  """
  with open(path) as f:
    # Test the first line only.
    matched = _TIMESTAMP_PATTERN.search(f.readline())
    if matched:
      return datetime.datetime.strptime(matched.group(1), _TIMESTAMP_FORMAT)
  return None


def _GetBuilderInfo():
  """Returns builder info containing bot info."""
  # Use OrderedDict for json output stabilization.
  return collections.OrderedDict([
      ('builder', os.environ.get('BUILDBOT_BUILDERNAME', '')),
      ('master', os.environ.get('BUILDBOT_MASTERNAME', '')),
      ('slave', os.environ.get('BUILDBOT_SLAVENAME', '')),
      ('clobber', bool(os.environ.get('BUILDBOT_CLOBBER'))),
      ('os', 'chromeos'),
  ])


class GomaLogUploader(object):
  """Manages uploading goma log files to Google Cloud Storage."""

  # Special object to find latest log file. Please see _GetGlogInfoFileList()
  # for details.
  _LATEST = object()

  def __init__(self, goma_log_dir=None, dry_run=False):
    self._gs_context = gs.GSContext(dry_run=dry_run)
    self._goma_log_dir = goma_log_dir or _GetGomaLogDirectory()
    logging.info('Goma log directory is: %s', self._goma_log_dir)

  def _GetGlogInfoFileList(self, pattern, start_timestamp):
    """Returns filepaths of the google glog INFO file.

    Args:
      pattern: a string of INFO file pattern.
      start_timestamp: datetime.datetime instance or special object _LATEST.
        if datetime.datetime instance is passed, returns files after the
        timestamp. If timestamp is _LATEST, then returns the latest one only
        (i.e., the returned list contains only one filepath).

    Returns:
      a list of found glog INFO files in fullpath.
      Or, an empty list if not found.
    """
    info_pattern = os.path.join(self._goma_log_dir, '%s.*.INFO.*' % pattern)
    candidates = glob.glob(info_pattern)
    if not candidates:
      return []

    if start_timestamp is GomaLogUploader._LATEST:
      return [max(candidates)]

    # Sort in ascending order for stabilization.
    return sorted(path for path in candidates
                  if _GetLogFileTimestamp(path) > start_timestamp)

  def _GetCompilerProxyStartTime(self):
    """Returns timestamp when the latest compiler_proxy started."""
    # Returns the latest compiler_proxy's log file created timing.
    compiler_proxy_info_list = self._GetGlogInfoFileList(
        'compiler_proxy', GomaLogUploader._LATEST)
    if not compiler_proxy_info_list:
      return None
    return _GetLogFileTimestamp(compiler_proxy_info_list[0])

  def _UploadToGomaLogGS(self, path, remote_dir, headers):
    """Uploads the file to GS with gzip'ing.

    Args:
      path: local file path to be uploaded.
      remote_dir: path to the gs remote directory.
      headers: builder info to be annotated.
    """
    logging.info('Uploading %s', path)
    self._gs_context.CopyInto(
        path, remote_dir, filename=os.path.basename(path) + '.gz',
        auto_compress=True, headers=headers)

  def _UploadInfoFiles(self, remote_dir, headers, pattern, start_timestamp):
    """Uploads INFO files matched.

    Finds files whose path matches with pattern, and created after
    start_timestamp, then uploads it with gzip'ing.

    Args:
      remote_dir: path to the gs remote directory.
      headers: builder info to be annotated.
      pattern: matching path pattern.
      start_timestamp: files created after this timestamp will be uploaded.
        Can be _LATEST if latest one should be uploaded.
    """
    paths = self._GetGlogInfoFileList(pattern, start_timestamp)
    if not paths:
      logging.warning('No glog files matched with: %s', pattern)
      return

    for path in paths:
      self._UploadToGomaLogGS(path, remote_dir, headers)

  def Upload(self, today=None):
    """Uploads goma related INFO files to Google Cloud Storage.

    Args:
      today: datetime.date instance representing today. This is introduced for
        testing purpose, because datetime.date is unpatchable.
        In real use case, this must be None.
    """
    if today is None:
      today = datetime.date.today()
    remote_dir = 'gs://%s/%s/%s' % (
        GOMA_LOG_GS_BUCKET, today.strftime('%Y/%m/%d'),
        cros_build_lib.GetHostName())

    builder_info = json.dumps(_GetBuilderInfo())
    logging.info('BuilderInfo: %s', builder_info)
    headers = ['x-goog-meta-builderinfo:' + builder_info]

    self._UploadInfoFiles(
        remote_dir, headers, 'compiler_proxy-subproc', GomaLogUploader._LATEST)
    self._UploadInfoFiles(
        remote_dir, headers, 'compiler_proxy', GomaLogUploader._LATEST)
    compiler_proxy_start_time = self._GetCompilerProxyStartTime()
    if not compiler_proxy_start_time:
      logging.error('Compiler proxy start time is not found. '
                    'So, gomacc INFO files will not be uploaded.')
      return

    # TODO(crbug.com/719843): Enable uploading gomacc logs after
    # crbug.com/719843 is resolved.


def _GetParser():
  """Returns parser for upload_goma_info.

  Returns:
    commandline.ArgumentParser object to parse the commandline args.
  """
  parser = commandline.ArgumentParser()
  parser.add_argument('--dry-run', action='store_true',
                      help='If set, do not run actual gsutil commands.')
  return parser


def main(argv):
  parser = _GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  GomaLogUploader(dry_run=options.dry_run).Upload()
