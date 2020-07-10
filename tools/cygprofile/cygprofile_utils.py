# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilites used by cygprofile scripts.
"""

import logging
import os
import re


START_OF_TEXT_SYMBOL = 'linker_script_start_of_text'


class WarningCollector(object):
  """Collects warnings, but limits the number printed to a set value."""
  def __init__(self, max_warnings, level=logging.WARNING):
    self._warnings = 0
    self._max_warnings = max_warnings
    self._level = level

  def Write(self, message):
    """Prints a warning if fewer than max_warnings have already been printed."""
    if self._warnings < self._max_warnings:
      logging.log(self._level, message)
    self._warnings += 1

  def WriteEnd(self, message):
    """Once all warnings have been printed, use this to print the number of
    elided warnings."""
    if self._warnings > self._max_warnings:
      logging.log(self._level, '%d more warnings for: %s' % (
          self._warnings - self._max_warnings, message))


def InvertMapping(x_to_ys):
  """Given a map x -> [y1, y2...] returns inverse mapping y->[x1, x2...]."""
  y_to_xs = {}
  for x, ys in x_to_ys.items():
    for y in ys:
      y_to_xs.setdefault(y, []).append(x)
  return y_to_xs


def GetObjDir(libchrome):
  """Get the path to the obj directory corresponding to the given libchrome.

  Assumes libchrome is in for example .../Release/lib/libchrome.so and object
  files are in .../Release/obj.
  """
  # TODO(lizeb,pasko): Pass obj path in explicitly where needed rather than
  # relying on the above assumption.
  return os.path.abspath(os.path.join(
      os.path.dirname(libchrome), '../obj'))
