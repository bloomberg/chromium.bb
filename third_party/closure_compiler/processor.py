# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import re
import os


class LineNumber(object):
  def __init__(self, file, line_number):
    self.file = file
    self.line_number = int(line_number)


class FileCache(object):
  _cache = defaultdict(str)

  def _read(self, file):
    file = os.path.abspath(file)
    self._cache[file] = self._cache[file] or open(file, "r").read()
    return self._cache[file]

  @staticmethod
  def read(file):
    return FileCache()._read(file)


class Processor(object):
  _IF_TAGS_REG = "</?if[^>]*?>"
  _INCLUDE_REG = "<include[^>]+src=['\"]([^>]*)['\"]>"

  def __init__(self, file):
    self._included_files = set()
    self._index = 0
    self._lines = self._get_file(file)

    while self._index < len(self._lines):
      current_line = self._lines[self._index]
      match = re.search(self._INCLUDE_REG, current_line[2])
      if match:
        file_dir = os.path.dirname(current_line[0])
        self._include_file(os.path.join(file_dir, match.group(1)))
      else:
        self._index += 1

    for i, line in enumerate(self._lines):
      self._lines[i] = line[:2] + (re.sub(self._IF_TAGS_REG, "", line[2]),)

    self.contents = "\n".join(l[2] for l in self._lines)

  # Returns a list of tuples in the format: (file, line number, line contents).
  def _get_file(self, file):
    lines = FileCache.read(file).splitlines()
    return [(file, lnum + 1, line) for lnum, line in enumerate(lines)]

  def _include_file(self, file):
    self._included_files.add(file)
    f = self._get_file(file)
    self._lines = self._lines[:self._index] + f + self._lines[self._index + 1:]

  def get_file_from_line(self, line_number):
    line_number = int(line_number) - 1
    return LineNumber(self._lines[line_number][0], self._lines[line_number][1])

  def included_files(self):
    return self._included_files
