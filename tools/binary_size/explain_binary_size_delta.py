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

import collections
import operator
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
      size_list = bucket.setdefault(symbol_name, [])
      size_list.append(symbol_size)

  # Now diff them. We iterate over the elements in cache1. For each symbol
  # that we find in cache2, we record whether it was deleted, changed, or
  # unchanged. We then remove it from cache2; all the symbols that remain
  # in cache2 at the end of the iteration over cache1 are the 'new' symbols.
  for key, bucket1 in cache1.items():
    bucket2 = cache2.get(key)
    if not bucket2:
      # A file was removed. Everything in bucket1 is dead.
      for symbol_name, symbol_size_list in bucket1.items():
        for symbol_size in symbol_size_list:
          removed.append((key[0], key[1], symbol_name, symbol_size, None))
    else:
      # File still exists, look for changes within.
      for symbol_name, symbol_size_list in bucket1.items():
        size_list2 = bucket2.get(symbol_name)
        if size_list2 is None:
          # Symbol no longer exists in bucket2.
          for symbol_size in symbol_size_list:
            removed.append((key[0], key[1], symbol_name, symbol_size, None))
        else:
          del bucket2[symbol_name] # Symbol is not new, delete from cache2.
          if len(symbol_size_list) == 1 and len(size_list2) == 1:
            symbol_size = symbol_size_list[0]
            size2 = size_list2[0]
            if symbol_size != size2:
              # Symbol has change size in bucket.
              changed.append((key[0], key[1], symbol_name, symbol_size, size2))
            else:
              # Symbol is unchanged.
              unchanged.append((key[0], key[1], symbol_name, symbol_size,
                                size2))
          else:
            # Complex comparison for when a symbol exists multiple times
            # in the same file (where file can be "unknown file").
            symbol_size_counter = collections.Counter(symbol_size_list)
            delta_counter = collections.Counter(symbol_size_list)
            delta_counter.subtract(size_list2)
            for symbol_size in sorted(delta_counter.keys()):
              delta = delta_counter[symbol_size]
              unchanged_count = symbol_size_counter[symbol_size]
              if delta > 0:
                unchanged_count -= delta
              for _ in range(unchanged_count):
                unchanged.append((key[0], key[1], symbol_name, symbol_size,
                                  symbol_size))
              if delta > 0: # Used to be more of these than there is now.
                for _ in range(delta):
                  removed.append((key[0], key[1], symbol_name, symbol_size,
                                  None))
              elif delta < 0: # More of this (symbol,size) now.
                for _ in range(-delta):
                  added.append((key[0], key[1], symbol_name, None, symbol_size))

          if len(bucket2) == 0:
            del cache1[key] # Entire bucket is empty, delete from cache2

  # We have now analyzed all symbols that are in cache1 and removed all of
  # the encountered symbols from cache2. What's left in cache2 is the new
  # symbols.
  for key, bucket2 in cache2.iteritems():
    for symbol_name, symbol_size_list in bucket2.items():
      for symbol_size in symbol_size_list:
        added.append((key[0], key[1], symbol_name, None, symbol_size))
  return (added, removed, changed, unchanged)

def DeltaStr(number):
  """Returns the number as a string with a '+' prefix if it's > 0 and
  a '-' prefix if it's < 0."""
  result = str(number)
  if number > 0:
    result = '+' + result
  return result


class CrunchStatsData(object):
  """Stores a summary of data of a certain kind."""
  def __init__(self, symbols):
    self.symbols = symbols
    self.sources = set()
    self.before_size = 0
    self.after_size = 0
    self.symbols_by_path = {}


def CrunchStats(added, removed, changed, unchanged, showsources, showsymbols):
  """Outputs to stdout a summary of changes based on the symbol lists."""
  # Split changed into grown and shrunk because that is easier to
  # discuss.
  grown = []
  shrunk = []
  for item in changed:
    file_path, symbol_type, symbol_name, size1, size2 = item
    if size1 < size2:
      grown.append(item)
    else:
      shrunk.append(item)

  new_symbols = CrunchStatsData(added)
  removed_symbols = CrunchStatsData(removed)
  grown_symbols = CrunchStatsData(grown)
  shrunk_symbols = CrunchStatsData(shrunk)
  sections = [new_symbols, removed_symbols, grown_symbols, shrunk_symbols]
  for section in sections:
    for file_path, symbol_type, symbol_name, size1, size2 in section.symbols:
      section.sources.add(file_path)
      if size1 is not None:
        section.before_size += size1
      if size2 is not None:
        section.after_size += size2
      bucket = section.symbols_by_path.setdefault(file_path, [])
      bucket.append((symbol_name, symbol_type, size1, size2))

  total_change = sum(s.after_size - s.before_size for s in sections)
  summary = 'Total change: %s bytes' % DeltaStr(total_change)
  print(summary)
  print('=' * len(summary))
  for section in sections:
    if not section.symbols:
      continue
    if section.before_size == 0:
      description = ('added, totalling %s bytes' % DeltaStr(section.after_size))
    elif section.after_size == 0:
      description = ('removed, totalling %s bytes' %
                     DeltaStr(-section.before_size))
    else:
      if section.after_size > section.before_size:
        type_str = 'grown'
      else:
        type_str = 'shrunk'
      description = ('%s, for a net change of %s bytes '
                     '(%d bytes before, %d bytes after)' %
            (type_str, DeltaStr(section.after_size - section.before_size),
             section.before_size, section.after_size))
    print('  %d %s across %d sources' %
          (len(section.symbols), description, len(section.sources)))

  maybe_unchanged_sources = set()
  unchanged_symbols_size = 0
  for file_path, symbol_type, symbol_name, size1, size2 in unchanged:
    maybe_unchanged_sources.add(file_path)
    unchanged_symbols_size += size1 # == size2
  print('  %d unchanged, totalling %d bytes' %
        (len(unchanged), unchanged_symbols_size))

  # High level analysis, always output.
  unchanged_sources = maybe_unchanged_sources
  for section in sections:
    unchanged_sources = unchanged_sources - section.sources
  new_sources = (new_symbols.sources -
    maybe_unchanged_sources -
    removed_symbols.sources)
  removed_sources = (removed_symbols.sources -
    maybe_unchanged_sources -
    new_symbols.sources)
  partially_changed_sources = (grown_symbols.sources |
    shrunk_symbols.sources | new_symbols.sources |
    removed_symbols.sources) - removed_sources - new_sources
  allFiles = set()
  for section in sections:
    allFiles = allFiles | section.sources
  allFiles = allFiles | maybe_unchanged_sources
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
  for section in sections:
    for path in section.symbols_by_path:
      entry = delta_by_path.get(path)
      if not entry:
        entry = {'plus': 0, 'minus': 0}
        delta_by_path[path] = entry
      for symbol_name, symbol_type, size1, size2 in \
            section.symbols_by_path[path]:
        if size1 is None:
          delta = size2
        elif size2 is None:
          delta = -size1
        else:
          delta = size2 - size1

        if delta > 0:
          entry['plus'] += delta
        else:
          entry['minus'] += (-1 * delta)

  def delta_sort_key(item):
    _path, size_data = item
    growth = size_data['plus'] - size_data['minus']
    return growth

  for path, size_data in sorted(delta_by_path.iteritems(), key=delta_sort_key,
                                reverse=True):
    gain = size_data['plus']
    loss = size_data['minus']
    delta = size_data['plus'] - size_data['minus']
    header = ' %s - Source: %s - (gained %d, lost %d)' % (DeltaStr(delta),
                                                          path, gain, loss)
    divider = '-' * len(header)
    print ''
    print divider
    print header
    print divider
    if showsymbols:
      if path in new_symbols.symbols_by_path:
        print '  New symbols:'
        for symbol_name, symbol_type, size1, size2 in \
            sorted(new_symbols.symbols_by_path[path],
                   key=operator.itemgetter(3),
                   reverse=True):
          print ('   %8s: %s type=%s, size=%d bytes' %
                 (DeltaStr(size2), symbol_name, symbol_type, size2))
      if path in removed_symbols.symbols_by_path:
        print '  Removed symbols:'
        for symbol_name, symbol_type, size1, size2 in \
            sorted(removed_symbols.symbols_by_path[path],
                   key=operator.itemgetter(2)):
          print ('   %8s: %s type=%s, size=%d bytes' %
                 (DeltaStr(-size1), symbol_name, symbol_type, size1))
      for (changed_symbols_by_path, type_str) in [
        (grown_symbols.symbols_by_path, "Grown"),
        (shrunk_symbols.symbols_by_path, "Shrunk")]:
        if path in changed_symbols_by_path:
          print '  %s symbols:' % type_str
          def changed_symbol_sortkey(item):
            symbol_name, _symbol_type, size1, size2 = item
            return (size1 - size2, symbol_name)
          for symbol_name, symbol_type, size1, size2 in \
              sorted(changed_symbols_by_path[path], key=changed_symbol_sortkey):
            print ('   %8s: %s type=%s, (was %d bytes, now %d bytes)'
                   % (DeltaStr(size2 - size1), symbol_name,
                      symbol_type, size1, size2))


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
