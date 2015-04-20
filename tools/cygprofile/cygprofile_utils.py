#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilites used by cygprofile scripts.
"""

import logging
import os
import re

class WarningCollector(object):
  """Collects warnings, but limits the number printed to a set value."""
  def __init__(self, max_warnings):
    self._warnings = 0
    self._max_warnings = max_warnings

  def Write(self, message):
    """Print a warning if fewer than max_warnings have already been printed."""
    if self._warnings < self._max_warnings:
      logging.warning(message)
    self._warnings += 1

  def WriteEnd(self, message):
    """Once all warnings have been printed, use this to print the number of
    elided warnings."""
    if self._warnings > self._max_warnings:
      logging.warning('%d more warnings for: %s' % (
          self._warnings - self._max_warnings, message))


def DetectArchitecture(default='arm'):
  """Detects the architecture by looking for target_arch in GYP_DEFINES.
  If not not found, returns default.
  """
  gyp_defines = os.environ.get('GYP_DEFINES', '')
  match = re.match('target_arch=(\S+)', gyp_defines)
  if match and len(match.groups()) == 1:
    return match.group(1)
  else:
    return default
