# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This parser turns the am dumpheap -n output into a |NativeHeap| object."""

import logging
import re

from memory_inspector.core import native_heap
from memory_inspector.core import memory_map
from memory_inspector.core import stacktrace


def Parse(lines):
  """Parses the output of Android's am dumpheap -n.

  am dumpheap dumps the oustanding malloc information (when the system property
  libc.debug.malloc == 1).

  The expected dumpheap output looks like this:
  ------------------------------------------------------------------------------
  ... Some irrelevant banner lines ...
  z 0  sz    1000   num    3  bt 1234 5678 9abc ...
  ...
  MAPS
  9dcd0000-9dcd6000 r-xp 00000000 103:00 815       /system/lib/libnbaio.so
  ...
  ------------------------------------------------------------------------------
  The lines before MAPS list the allocations grouped by {size, backtrace}. In
  the example above, "1000" is the size of each alloc, "3" is their cardinality
  and "1234 5678 9abc" are the first N stack frames (absolute addresses in the
  process virtual address space).  The lines after MAPS provide essentially the
  same information of /proc/PID/smaps.
  See tests/android_backend_test.py for a more complete example.

  Args:
      lines: array of strings containing the am dumpheap -n output.

  Returns:
      An instance of |native_heap.NativeHeap|.
  """
  (STATE_PARSING_BACKTRACES, STATE_PARSING_MAPS, STATE_ENDED) = range(3)
  BT_RE = re.compile(
      r'^\w+\s+\d+\s+sz\s+(\d+)\s+num\s+(\d+)\s+bt\s+((?:[0-9a-f]+\s?)+)$')
  MAP_RE = re.compile(
      r'^([0-9a-f]+)-([0-9a-f]+)\s+....\s*([0-9a-f]+)\s+\w+:\w+\s+\d+\s*(.*)$')

  state = STATE_PARSING_BACKTRACES
  skip_first_n_lines = 5
  mmap = memory_map.Map()
  nativeheap = native_heap.NativeHeap()

  for line in lines:
    line = line.rstrip('\r\n')
    if skip_first_n_lines > 0:
      skip_first_n_lines -= 1
      continue

    if state == STATE_PARSING_BACKTRACES:
      if line == 'MAPS':
        state = STATE_PARSING_MAPS
        continue
      m = BT_RE.match(line)
      if not m:
        logging.warning('Skipping unrecognized dumpheap alloc: "%s"' % line)
        continue
      alloc_size = int(m.group(1))
      alloc_count = int(m.group(2))
      alloc_bt_str = m.group(3)
      strace = stacktrace.Stacktrace()
      # Keep only one |stacktrace.Frame| per distinct |absolute_addr|, in order
      # to ease the complexity of the final de-offset pass.
      for absolute_addr in alloc_bt_str.split():
        absolute_addr = int(absolute_addr, 16)
        stack_frame = nativeheap.GetStackFrame(absolute_addr)
        strace.Add(stack_frame)
      nativeheap.Add(native_heap.Allocation(alloc_size, alloc_count, strace))

    # The am dumpheap output contains also a list of mmaps. This information is
    # used in this module for the only purpose of normalizing addresses (i.e.
    # translating an absolute addr into its offset inside the mmap-ed library).
    # The mmap information is not further retained. A more complete mmap dump is
    # performed (and retained) using the memdump tool (see memdump_parser.py).
    elif state == STATE_PARSING_MAPS:
      if line == 'END':
        state = STATE_ENDED
        continue
      m = MAP_RE.match(line)
      if not m:
        logging.warning('Skipping unrecognized dumpheap mmap: "%s"' % line)
        continue
      mmap.Add(memory_map.MapEntry(
          start=int(m.group(1), 16),
          end=int(m.group(2), 16),
          prot_flags='----', # Not really needed for lookup
          mapped_file=m.group(4),
          mapped_offset=int(m.group(3), 16)))

    elif state == STATE_ENDED:
      pass

    else:
      assert(False)

  # Final pass: translate all the stack frames' absolute addresses into
  # relative offsets (exec_file + offset) using the memory maps just processed.
  for abs_addr, stack_frame in nativeheap.stack_frames.iteritems():
    assert(abs_addr == stack_frame.address)
    map_entry = mmap.Lookup(abs_addr)
    if not map_entry:
      continue
    stack_frame.SetExecFileInfo(map_entry.mapped_file,
                                map_entry.GetRelativeOffset(abs_addr))

  return nativeheap
