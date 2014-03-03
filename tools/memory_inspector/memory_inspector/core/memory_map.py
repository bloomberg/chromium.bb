# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bisect


class Map(object):
  """Models the memory map of a given |backends.Process|.

  This is typically obtained by calling backends.Process.DumpMemoryMaps()."""

  def __init__(self):
    self.entries = []

  def Add(self, entry):
    assert(isinstance(entry, MapEntry))
    bisect.insort_right(self.entries, entry)

  def Lookup(self, addr):
    """Returns the MapEntry containing the given address, if any."""
    idx = bisect.bisect_right(self.entries, addr) - 1
    if idx < 0:
      return None
    entry = self.entries[idx]
    assert(addr >= entry.start)
    # bisect_right returns the latest element <= addr, but addr might fall after
    # its end (in which case we want to return None here).
    if addr > entry.end:
      return None
    return entry

  def __getitem__(self, index):
    return self.entries[index]

  def __len__(self):
    return len(self.entries)


class MapEntry(object):
  """An entry (address range + stats) in a memory |Map|."""
  PAGE_SIZE = 4096

  def __init__(self, start, end, prot_flags, mapped_file, mapped_offset,
      priv_dirty_bytes=0, priv_clean_bytes=0, shared_dirty_bytes=0,
      shared_clean_bytes=0, resident_pages=None):
    assert(end > start)
    assert(start >= 0)
    self.start = start
    self.end = end
    self.prot_flags = prot_flags
    self.mapped_file = mapped_file
    self.mapped_offset = mapped_offset
    self.priv_dirty_bytes = priv_dirty_bytes
    self.priv_clean_bytes = priv_clean_bytes
    self.shared_dirty_bytes = shared_dirty_bytes
    self.shared_clean_bytes = shared_clean_bytes
    # resident_pages is a bitmap (array of bytes) in which each bit represents
    # the presence of its corresponding page.
    self.resident_pages = resident_pages or []

  def GetRelativeOffset(self, abs_addr):
    """Converts abs_addr to the corresponding offset in the mapped file."""
    assert(abs_addr >= self.start and abs_addr <= self.end)
    return abs_addr - self.start + self.mapped_offset

  def IsPageResident(self, relative_page_index):
    """Checks whether a given memory page is resident in memory."""
    assert(relative_page_index >= 0 and
           relative_page_index < self.len / MapEntry.PAGE_SIZE)
    assert(len(self.resident_pages) * MapEntry.PAGE_SIZE * 8 >= self.len)
    arr_idx = relative_page_index / 8
    arr_bit = relative_page_index % 8
    return (self.resident_pages[arr_idx] & (1 << arr_bit)) != 0

  def __cmp__(self, other):
    """Comparison operator required for bisect."""
    if isinstance(other, MapEntry):
      return self.start - other.start
    elif isinstance(other, int):
      return self.start - other
    else:
      raise Exception('Cannot compare with %s' % other.__class__)

  @property
  def len(self):
    return self.end - self.start + 1
