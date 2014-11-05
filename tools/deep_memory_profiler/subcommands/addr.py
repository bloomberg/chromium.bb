# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys
import re

from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class AddrCommand(SubCommand):
  def __init__(self):
    super(AddrCommand, self).__init__('Usage: %prog addr <dump-file>')

  def do(self, sys_argv, out=sys.stdout):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    (bucket_set, dump) = SubCommand.load_basic_files(dump_path, False)
    AddrCommand._output(dump, bucket_set, out)
    return 0

  @staticmethod
  def _output(dump, bucket_set, out):
    """Prints memory usage by function addresses with resolving symbols.

    Args:
        bucket_set: A BucketSet object.
        out: An IO object to output.
    """
    sizes = {}
    library = {}

    _ADDR_PATTERN = re.compile(r'^0x[a-f0-9]+$', re.IGNORECASE)
    _TCMALLOC_PATTERN = re.compile(r'.*(ProfilerMalloc|MemoryRegionMap::'
                 '|TypeProfilerMalloc|DoAllocWithArena|SbrkSysAllocator::Alloc'
                 '|MmapSysAllocator::Alloc|LowLevelAlloc::Alloc'
                 '|LowLevelAlloc::AllocWithArena).*', re.IGNORECASE)

    for bucket_id, virtual, committed, _, _ in dump.iter_stacktrace:
      bucket = bucket_set.get(bucket_id)
      if not bucket:
        AddrCommand._add_size(bucket_id, "bucket", virtual, committed, sizes)
        continue

      if _TCMALLOC_PATTERN.match(bucket.symbolized_joined_stackfunction):
        AddrCommand._add_size("TCMALLOC", "TCMALLOC",
                              virtual, committed, sizes)
        continue

      for index, addr in enumerate(bucket.stacktrace):
        if _ADDR_PATTERN.match(bucket.symbolized_stackfunction[index]):
          AddrCommand._find_library(addr, dump, library)
          AddrCommand._add_size(hex(addr), library[addr],
                                virtual, committed, sizes)
        else:
          AddrCommand._add_size(hex(addr),
                    bucket.symbolized_stackfunction[index]
                    + "@" + bucket.symbolized_stacksourcefile[index],
                    virtual, committed, sizes)

    for key in sorted(sizes):
      out.write('%s;%s;%s;%s\n' % (sizes[key]["vss"], sizes[key]["rss"],
                    key, sizes[key]["desc"]))


  @staticmethod
  def _add_size(key, desc, vss, rss, sizes):
    if not key in sizes:
      sizes[key] = {"desc":desc, "vss":0, "rss":0, "alloc":0, "free": 0}
    if sizes[key]["desc"] == desc:
      sizes[key]["vss"] += vss
      sizes[key]["rss"] += rss
    else:
      sys.stderr.write('%s:(%s) or (%s)?\n' % (key, sizes[key]["desc"], desc))
      sys.exit(1)

  @staticmethod
  def _find_library(func, dump, library):
    if not func in library:
      library[func] = "Unknown"
      for addr, region in dump.iter_map:
        if addr[0] >= func and func < addr[1]:
          library[func] = region[1]['vma']['name']
          break