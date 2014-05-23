#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Describe the size difference of two binaries.

Generates a description of the size difference of two binaries based
on the difference of the size of various symbols.

This tool needs "nm" dumps of each binary with full symbol
information. You can obtain the necessary dumps by running the
run_binary_size_analysis.py script upon each binary, with the
"--nm-out" parameter set to the location in which you want to save the
dumps. Example:

  # obtain symbol data from first binary in /tmp/nm1.dump
  cd $CHECKOUT1_SRC
  ninja -C out/Release binary_size_tool
  tools/binary_size/run_binary_size_analysis \
      --library <path_to_library>
      --destdir /tmp/throwaway
      --nm-out /tmp/nm1.dump

  # obtain symbol data from second binary in /tmp/nm2.dump
  cd $CHECKOUT2_SRC
  ninja -C out/Release binary_size_tool
  tools/binary_size/run_binary_size_analysis \
      --library <path_to_library>
      --destdir /tmp/throwaway
      --nm-out /tmp/nm2.dump

  # cleanup useless files
  rm -r /tmp/throwaway

  # run this tool
  explain_binary_size_delta.py --nm1 /tmp/nm1.dump --nm2 /tmp/nm2.dump
"""

import optparse
import os
import sys

import binary_size_utils


def Compare(symbols1, symbols2):
  """Executes a comparison of the symbols in symbols1 and symbols2.

  Returns:
      tuple of lists: (added_symbols, removed_symbols, changed_symbols, others)
  """
  added = [] # tuples
  removed = [] # tuples
  changed = [] # tuples
  unchanged = [] # tuples

  cache1 = {}
  cache2 = {}
  # Make a map of (file, symbol_type) : (symbol_name, symbol_size)
  for cache, symbols in ((cache1, symbols1), (cache2, symbols2)):
    for symbol_name, symbol_type, symbol_size, file_path in symbols:
      if 'vtable for ' in symbol_name:
        symbol_type = '@' # hack to categorize these separately
      if file_path:
        file_path = os.path.normpath(file_path)
      else:
        file_path = '(No Path)'
      key = (file_path, symbol_type)
      bucket = cache.setdefault(key, {})
      bucket[symbol_name] = symbol_size

  # Now diff them. We iterate over the elements in cache1. For each symbol
  # that we find in cache2, we record whether it was deleted, changed, or
  # unchanged. We then remove it from cache2; all the symbols that remain
  # in cache2 at the end of the iteration over cache1 are the 'new' symbols.
  for key, bucket1 in cache1.items():
    bucket2 = cache2.get(key)
    if not bucket2:
      # A file was removed. Everything in bucket1 is dead.
      for symbol_name, symbol_size in bucket1.items():
        removed.append((key[0], key[1], symbol_name, symbol_size, None))
    else:
      # File still exists, look for changes within.
      for symbol_name, symbol_size in bucket1.items():
        size2 = bucket2.get(symbol_name)
        if size2 is None:
          # Symbol no longer exists in bucket2.
          removed.append((key[0], key[1], symbol_name, symbol_size, None))
        else:
          del bucket2[symbol_name] # Symbol is not new, delete from cache2.
          if len(bucket2) == 0:
            del cache1[key] # Entire bucket is empty, delete from cache2
          if symbol_size != size2:
            # Symbol has change size in bucket.
            changed.append((key[0], key[1], symbol_name, symbol_size, size2))
          else:
            # Symbol is unchanged.
            unchanged.append((key[0], key[1], symbol_name, symbol_size, size2))

  # We have now analyzed all symbols that are in cache1 and removed all of
  # the encountered symbols from cache2. What's left in cache2 is the new
  # symbols.
  for key, bucket2 in cache2.iteritems():
    for symbol_name, symbol_size in bucket2.items():
      added.append((key[0], key[1], symbol_name, None, symbol_size))
  return (added, removed, changed, unchanged)


def CrunchStats(added, removed, changed, unchanged, showsources, showsymbols):
  """Outputs to stdout a summary of changes based on the symbol lists."""
  print 'Symbol statistics:'
  sources_with_new_symbols = set()
  new_symbols_size = 0
  new_symbols_by_path = {}
  for file_path, symbol_type, symbol_name, size1, size2 in added:
    sources_with_new_symbols.add(file_path)
    new_symbols_size += size2
    bucket = new_symbols_by_path.setdefault(file_path, [])
    bucket.append((symbol_name, symbol_type, None, size2))
  print('  %d added, totalling %d bytes across %d sources' %
        (len(added), new_symbols_size, len(sources_with_new_symbols)))

  sources_with_removed_symbols = set()
  removed_symbols_size = 0
  removed_symbols_by_path = {}
  for file_path, symbol_type, symbol_name, size1, size2 in removed:
    sources_with_removed_symbols.add(file_path)
    removed_symbols_size += size1
    bucket = removed_symbols_by_path.setdefault(file_path, [])
    bucket.append((symbol_name, symbol_type, size1, None))
  print('  %d removed, totalling %d bytes removed across %d sources' %
        (len(removed), removed_symbols_size, len(sources_with_removed_symbols)))

  sources_with_changed_symbols = set()
  before_size = 0
  after_size = 0
  changed_symbols_by_path = {}
  for file_path, symbol_type, symbol_name, size1, size2 in changed:
    sources_with_changed_symbols.add(file_path)
    before_size += size1
    after_size += size2
    bucket = changed_symbols_by_path.setdefault(file_path, [])
    bucket.append((symbol_name, symbol_type, size1, size2))
  print('  %d changed, resulting in a net change of %d bytes '
        '(%d bytes before, %d bytes after) across %d sources' %
        (len(changed), (after_size - before_size), before_size, after_size,
         len(sources_with_changed_symbols)))

  maybe_unchanged_sources = set()
  unchanged_symbols_size = 0
  for file_path, symbol_type, symbol_name, size1, size2 in unchanged:
    maybe_unchanged_sources.add(file_path)
    unchanged_symbols_size += size1 # == size2
  print('  %d unchanged, totalling %d bytes' %
        (len(unchanged), unchanged_symbols_size))

  # High level analysis, always output.
  unchanged_sources = (maybe_unchanged_sources -
    sources_with_changed_symbols -
    sources_with_removed_symbols -
    sources_with_new_symbols)
  new_sources = (sources_with_new_symbols -
    maybe_unchanged_sources -
    sources_with_removed_symbols)
  removed_sources = (sources_with_removed_symbols -
    maybe_unchanged_sources -
    sources_with_new_symbols)
  partially_changed_sources = (sources_with_changed_symbols |
    sources_with_new_symbols |
    sources_with_removed_symbols) - removed_sources - new_sources
  allFiles = (sources_with_new_symbols |
    sources_with_removed_symbols |
    sources_with_changed_symbols |
    maybe_unchanged_sources)
  print 'Source stats:'
  print('  %d sources encountered.' % len(allFiles))
  print('  %d completely new.' % len(new_sources))
  print('  %d removed completely.' % len(removed_sources))
  print('  %d partially changed.' % len(partially_changed_sources))
  print('  %d completely unchanged.' % len(unchanged_sources))
  remainder = (allFiles - new_sources - removed_sources -
    partially_changed_sources - unchanged_sources)
  assert len(remainder) == 0

  if not showsources:
    return  # Per-source analysis, only if requested
  print 'Per-source Analysis:'
  delta_by_path = {}
  for path in new_symbols_by_path:
    entry = delta_by_path.get(path)
    if not entry:
      entry = {'plus': 0, 'minus': 0}
      delta_by_path[path] = entry
    for symbol_name, symbol_type, size1, size2 in new_symbols_by_path[path]:
      entry['plus'] += size2
  for path in removed_symbols_by_path:
    entry = delta_by_path.get(path)
    if not entry:
      entry = {'plus': 0, 'minus': 0}
      delta_by_path[path] = entry
    for symbol_name, symbol_type, size1, size2 in removed_symbols_by_path[path]:
      entry['minus'] += size1
  for path in changed_symbols_by_path:
    entry = delta_by_path.get(path)
    if not entry:
      entry = {'plus': 0, 'minus': 0}
      delta_by_path[path] = entry
    for symbol_name, symbol_type, size1, size2 in changed_symbols_by_path[path]:
      delta = size2 - size1
      if delta > 0:
        entry['plus'] += delta
      else:
        entry['minus'] += (-1 * delta)

  for path in sorted(delta_by_path):
    print '  Source: ' + path
    size_data = delta_by_path[path]
    gain = size_data['plus']
    loss = size_data['minus']
    delta = size_data['plus'] - size_data['minus']
    print ('    Change: %d bytes (gained %d, lost %d)' % (delta, gain, loss))
    if showsymbols:
      if path in new_symbols_by_path:
        print '    New symbols:'
        for symbol_name, symbol_type, size1, size2 in \
            new_symbols_by_path[path]:
          print ('      %s type=%s, size=%d bytes' %
                 (symbol_name, symbol_type, size2))
      if path in removed_symbols_by_path:
        print '    Removed symbols:'
        for symbol_name, symbol_type, size1, size2 in \
            removed_symbols_by_path[path]:
          print ('      %s type=%s, size=%d bytes' %
                 (symbol_name, symbol_type, size1))
      if path in changed_symbols_by_path:
        print '    Changed symbols:'
        def sortkey(item):
          symbol_name, _symbol_type, size1, size2 = item
          return (size1 - size2, symbol_name)
        for symbol_name, symbol_type, size1, size2 in \
            sorted(changed_symbols_by_path[path], key=sortkey):
          print ('      %s type=%s, delta=%d bytes (was %d bytes, now %d bytes)'
                 % (symbol_name, symbol_type, (size2 - size1), size1, size2))


def main():
  usage = """%prog [options]

  Analyzes the symbolic differences between two binary files
  (typically, not necessarily, two different builds of the same
  library) and produces a detailed description of symbols that have
  been added, removed, or whose size has changed.

  Example:
       explain_binary_size_delta.py --nm1 /tmp/nm1.dump --nm2 /tmp/nm2.dump

  Options are available via '--help'.
  """
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--nm1', metavar='PATH',
                    help='the nm dump of the first library')
  parser.add_option('--nm2', metavar='PATH',
                    help='the nm dump of the second library')
  parser.add_option('--showsources', action='store_true', default=False,
                    help='show per-source statistics')
  parser.add_option('--showsymbols', action='store_true', default=False,
                    help='show all symbol information; implies --showfiles')
  parser.add_option('--verbose', action='store_true', default=False,
                    help='output internal debugging stuff')
  opts, _args = parser.parse_args()

  if not opts.nm1:
    parser.error('--nm1 is required')
  if not opts.nm2:
    parser.error('--nm2 is required')
  symbols = []
  for path in [opts.nm1, opts.nm2]:
    with file(path, 'r') as nm_input:
      if opts.verbose:
        print 'parsing ' + path + '...'
      symbols.append(list(binary_size_utils.ParseNm(nm_input)))
  (added, removed, changed, unchanged) = Compare(symbols[0], symbols[1])
  CrunchStats(added, removed, changed, unchanged,
    opts.showsources | opts.showsymbols, opts.showsymbols)

if __name__ == '__main__':
  sys.exit(main())
