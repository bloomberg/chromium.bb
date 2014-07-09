# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os


LOGGER = logging.getLogger('dmprof')


class Dump(object):
  """Represents a heap profile dump."""
  def __init__(self):
    pass

  @property
  def path(self):
    raise NotImplementedError

  @property
  def count(self):
    raise NotImplementedError

  @property
  def time(self):
    raise NotImplementedError

  @property
  def iter_map(self):
    raise NotImplementedError

  @property
  def iter_stacktrace(self):
    raise NotImplementedError

  def global_stat(self, name):
    raise NotImplementedError

  @property
  def run_id(self):
    raise NotImplementedError

  @property
  def pagesize(self):
    raise NotImplementedError

  @property
  def pageframe_length(self):
    raise NotImplementedError

  @property
  def pageframe_encoding(self):
    raise NotImplementedError

  @property
  def has_pagecount(self):
    raise NotImplementedError

  @staticmethod
  def load(path, log_header='Loading a heap profile dump: '):
    """Loads a heap profile dump.

    Args:
        path: A file path string to load.
        log_header: A preceding string for log messages.

    Returns:
        A loaded Dump object.

    Raises:
        ParsingException for invalid heap profile dumps.
    """
    from lib.deep_dump import DeepDump
    dump = DeepDump(path, os.stat(path).st_mtime)
    with open(path, 'r') as f:
      dump.load_file(f, log_header)
    return dump


class DumpList(object):
  """Represents a sequence of heap profile dumps.

  Individual dumps are loaded into memory lazily as the sequence is accessed,
  either while being iterated through or randomly accessed.  Loaded dumps are
  not cached, meaning a newly loaded Dump object is returned every time an
  element in the list is accessed.
  """

  def __init__(self, dump_path_list):
    self._dump_path_list = dump_path_list

  @staticmethod
  def load(path_list):
    return DumpList(path_list)

  def __len__(self):
    return len(self._dump_path_list)

  def __iter__(self):
    for dump in self._dump_path_list:
      yield Dump.load(dump)

  def __getitem__(self, index):
    return Dump.load(self._dump_path_list[index])
