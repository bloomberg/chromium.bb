#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

from sets import Set


_ENTRIES = [
    ('Total', '.* r... .*'),
    ('Read-only', '.* r--. .*'),
    ('Read-write', '.* rw-. .*'),
    ('Executable', '.* ..x. .*'),
    ('File read-write', '.* rw.. .* /.*'),
    ('Anonymous read-write', '.* rw.. .* .*other=[0-9]+ ($|.*chromium:.*)'),
    ('File executable', '.* ..x. .* /.*'),
    ('Anonymous executable (JIT\'ed code)', '.* ..x. .* shared_other=[0-9]+ $'),
    ('chromium mmap', '.* r... .*chromium:.*'),
    ('chromium TransferBuffer', '.* r... .*chromium:.*CreateTransferBuffer.*'),
    ('Galaxy Nexus GL driver', '.* r... .*pvrsrvkm.*'),
    ('Dalvik', '.* rw.. .* /.*dalvik.*'),
    ('Dalvik heap', '.* rw.. .* /.*dalvik-heap.*'),
    ('Native heap (jemalloc)', '.* r... .* /.*jemalloc.*'),
    ('System heap', '.* r... .* \\[heap\\]'),
    ('Ashmem', '.* rw.. .* /dev/ashmem .*'),
    ('libchromeview.so total', '.* r... .* /.*libchromeview.so'),
    ('libchromeview.so read-only', '.* r--. .* /.*libchromeview.so'),
    ('libchromeview.so read-write', '.* rw-. .* /.*libchromeview.so'),
    ('libchromeview.so executable', '.* r.x. .* /.*libchromeview.so'),
]


def _CollectMemoryStats(memdump, region_filters):
  processes = []
  mem_usage_for_regions = None
  regexps = {}
  for region_filter in region_filters:
    regexps[region_filter] = re.compile(region_filter)
  for line in memdump:
    if 'PID=' in line:
      mem_usage_for_regions = {}
      processes.append(mem_usage_for_regions)
      continue
    matched_regions = Set([])
    for region_filter in region_filters:
      if regexps[region_filter].match(line.rstrip('\r\n')):
        matched_regions.add(region_filter)
        if not region_filter in mem_usage_for_regions:
          mem_usage_for_regions[region_filter] = {
              'private': 0,
              'shared_app': 0,
              'shared_other': 0,
          }
    for matched_region in matched_regions:
      mem_usage = mem_usage_for_regions[matched_region]
      for key in mem_usage:
        for token in line.split(' '):
          if key in token:
            mem_usage[key] += int(token.split('=')[1])
            break
  return processes


def _ConvertMemoryField(field):
  return str(field / (1024.0 * 1024))


def _DumpCSV(processes_stats):
  total_map = {}
  i = 0
  for process in processes_stats:
    i += 1
    print ',Process ' + str(i) + ',private,shared_app,shared_other,'
    for (k, v) in _ENTRIES:
      if not v in process:
        print ',' + k + ',0,0,0,'
        continue
      if not v in total_map:
        total_map[v] = 0
      total_map[v] += process[v]['private'] + (
          process[v]['shared_app'] / len(processes_stats))
      print ',' + k + ',' + _ConvertMemoryField(process[v]['private']) + ',' + (
          _ConvertMemoryField(process[v]['shared_app']) + ',' + (
          _ConvertMemoryField(process[v]['shared_other'])) + ',')
    print ''

  if len(processes_stats) > 2:
    print >>sys.stderr, (
        'Total stats are not supported yet with more than two processes.')
    return
  for (k, v) in _ENTRIES:
    if not v in total_map:
      print ',' + k + ',0,0'
      continue
    print ',' + k + ',' + _ConvertMemoryField(total_map[v]) + ','
  print ''


def main(argv):
  _DumpCSV(_CollectMemoryStats(sys.stdin, [value for (key, value) in _ENTRIES]))


if __name__ == '__main__':
  main(sys.argv)
