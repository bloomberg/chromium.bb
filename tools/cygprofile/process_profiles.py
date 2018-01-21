#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Lists all the reached symbols from an instrumentation dump."""

import argparse
import logging
import operator
import os
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))
path = os.path.join(_SRC_PATH, 'tools', 'cygprofile')
sys.path.append(path)
import symbol_extractor


class SymbolOffsetProcessor(object):
  """Utility for processing symbols in binaries.

  This class is used to translate between general offsets into a binary and the
  starting offset of symbols in the binary. Because later phases in orderfile
  generation have complicated strategies for resolving multiple symbols that map
  to the same binary offset, this class is concerned with locating a symbol
  containing a binary offset. If such a symbol exists, the start offset will be
  unique, even when there are multiple symbol names at the same location in the
  binary.

  In the function names below, "dump" is used to refer to arbitrary offsets in a
  binary (eg, from a profiling run), while "offset" refers to a symbol
  offset. The dump offsets are relative to the start of text, as returned by
  lightweight_cygprofile.cc.

  This class manages expensive operations like extracting symbols, so that
  higher-level operations can be done in different orders without the caller
  managing all the state.
  """

  def __init__(self, binary_filename):
    self._binary_filename = binary_filename
    self._symbol_infos = None
    self._name_to_symbol = None
    self._offset_to_primary = None

  def SymbolInfos(self):
    """The symbols associated with this processor's binary.

    The symbols are ordered by offset.

    Returns:
      [symbol_extractor.SymbolInfo]
    """
    if self._symbol_infos is None:
      self._symbol_infos = symbol_extractor.SymbolInfosFromBinary(
          self._binary_filename)
      self._symbol_infos.sort(key=lambda s: s.offset)
      logging.info('%d symbols from %s',
                   len(self._symbol_infos), self._binary_filename)
    return self._symbol_infos

  def NameToSymbolMap(self):
    """Map symbol names to their full information.

    Returns:
      {symbol name (str): symbol_extractor.SymbolInfo}
    """
    if self._name_to_symbol is None:
      self._name_to_symbol = {s.name: s for s in self.SymbolInfos()}
    return self._name_to_symbol

  def OffsetToPrimaryMap(self):
    """The map of a symbol offset in this binary to its primary symbol.

    Several symbols can be aliased to the same address, through ICF. This
    returns the first one. The order is consistent for a given binary, as it's
    derived from the file layout. We assert that all aliased symbols are the
    same size.

    Returns:
      {offset (int): primary (symbol_extractor.SymbolInfo)}
    """
    if self._offset_to_primary is None:
      self._offset_to_primary = {}
      for s in self.SymbolInfos():
        if s.offset not in self._offset_to_primary:
          self._offset_to_primary[s.offset] = s
        else:
          curr = self._offset_to_primary[s.offset]
          if curr.size != s.size:
            assert curr.size == 0 or s.size == 0, (
                'Nonzero size mismatch between {} and {}'.format(
                    curr.name, s.name))
            # Upgrade to a symbol with nonzero size, otherwise don't change
            # anything so that we use the earliest nonzero-size symbol.
            if curr.size == 0 and s.size != 0:
              self._offset_to_primary[s.offset] = s

    return self._offset_to_primary

  def GetReachedOffsetsFromDump(self, dump):
    """Find the symbol offsets from a list of binary offsets.

    The dump is a list offsets into a .text section. This finds the symbols
    which contain the dump offsets, and returns their offsets. Note that while
    usually a symbol offset corresponds to a single symbol, in some cases
    several symbols will map to the same offset. For that reason this function
    returns only the offset list. See cyglog_to_orderfile.py for computing more
    information about symbols.

    Args:
     dump: (int iterable) Dump offsets, for example as returned by MergeDumps().

    Returns:
      [int] Reached symbol offsets.
    """
    dump_offset_to_symbol_info = self._GetDumpOffsetToSymbolInfo()
    logging.info('Offset to Symbol size = %d', len(dump_offset_to_symbol_info))
    assert max(dump) / 4 <= len(dump_offset_to_symbol_info)
    already_seen = set()
    reached_offsets = []
    reached_return_addresses_not_found = 0
    for dump_offset in dump:
      symbol_info = dump_offset_to_symbol_info[dump_offset / 4]
      if symbol_info is None:
        reached_return_addresses_not_found += 1
        continue
      if symbol_info.offset in already_seen:
        continue
      reached_offsets.append(symbol_info.offset)
      already_seen.add(symbol_info.offset)
    if reached_return_addresses_not_found:
      logging.warning('%d return addresses don\'t map to any symbol',
                      reached_return_addresses_not_found)
    return reached_offsets

  def MatchSymbolNames(self, symbol_names):
    """Find the symbols in this binary which match a list of symbols.

    Args:
      symbol_names (str iterable) List of symbol names.

    Returns:
      [symbol_extractor.SymbolInfo] Symbols in this binary matching the names.
    """
    our_symbol_names = set(s.name for s in self.SymbolInfos())
    matched_names = our_symbol_names.intersection(set(symbol_names))
    return [self.NameToSymbolMap()[n] for n in matched_names]

  def _GetDumpOffsetToSymbolInfo(self):
    """Computes an array mapping each word in .text to a symbol.

    Returns:
      [symbol_extractor.SymbolInfo or None] For every 4 bytes of the .text
        section, maps it to a symbol, or None.
    """
    min_offset = min(s.offset for s in self.SymbolInfos())
    max_offset = max(s.offset + s.size for s in self.SymbolInfos())
    text_length_words = (max_offset - min_offset) / 4
    offset_to_symbol_info = [None for _ in xrange(text_length_words)]
    for s in self.SymbolInfos():
      offset = s.offset - min_offset
      for i in range(offset / 4, (offset + s.size) / 4):
        offset_to_symbol_info[i] = s
    return offset_to_symbol_info


def _SortedFilenames(filenames):
  """Returns filenames in ascending timestamp order.

  Args:
    filenames: (str iterable) List of filenames, matching.  *-TIMESTAMP.*.

  Returns:
    [str] Ordered by ascending timestamp.
  """
  filename_timestamp = []
  for filename in filenames:
    dash_index = filename.rindex('-')
    dot_index = filename.rindex('.')
    timestamp = int(filename[dash_index+1:dot_index])
    filename_timestamp.append((filename, timestamp))
  filename_timestamp.sort(key=operator.itemgetter(1))
  return [x[0] for x in filename_timestamp]


def MergeDumps(filenames):
  """Merges several dumps.

  Args:
    filenames: (str iterable) List of dump filenames.

  Returns:
    [int] Ordered list of reached offsets. Each offset only appears
    once in the output, in the order of the first dump that contains it.
  """
  dumps = [[int(x.strip()) for x in open(filename)] for filename in filenames]
  seen_offsets = set()
  result = []
  for dump in dumps:
    for offset in dump:
      if offset not in seen_offsets:
        result.append(offset)
        seen_offsets.add(offset)
  return result


def GetReachedOffsetsFromDumpFiles(dump_filenames, library_filename):
  """Produces a list of symbol offsets reached by the dumps.

  Args:
    dump_filenames (str iterable) A list of dump filenames.
    library_filename (str) The library file which the dumps refer to.

  Returns:
    [int] A list of symbol offsets. This order of symbol offsets produced is
      given by the deduplicated order of offsets found in dump_filenames (see
      also MergeDumps().
  """
  dump = MergeDumps(dump_filenames)
  logging.info('Reached offsets = %d', len(dump))
  processor = SymbolOffsetProcessor(library_filename)
  return processor.GetReachedOffsetsFromDump(dump)


def CreateArgumentParser():
  """Returns an ArgumentParser."""
  parser = argparse.ArgumentParser(description='Outputs reached symbols')
  parser.add_argument('--instrumented-build-dir', type=str,
                      help='Path to the instrumented build', required=True)
  parser.add_argument('--build-dir', type=str, help='Path to the build dir',
                      required=True)
  parser.add_argument('--dumps', type=str, help='A comma-separated list of '
                      'files with instrumentation dumps', required=True)
  parser.add_argument('--output', type=str, help='Output filename',
                      required=True)
  parser.add_argument('--offsets-output', type=str,
                      help='Output filename for the symbol offsets',
                      required=False, default=None)
  parser.add_argument('--library-name', default='libchrome.so',
                      help=('Chrome shared library name (usually libchrome.so '
                            'or libmonochrome.so'))
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.info('Merging dumps')
  dump_files = args.dumps.split(',')
  dumps = MergeDumps(_SortedFilenames(dump_files))

  instrumented_native_lib = os.path.join(args.instrumented_build_dir,
                                         'lib.unstripped', args.library_name)
  regular_native_lib = os.path.join(args.build_dir,
                                    'lib.unstripped', args.library_name)

  instrumented_processor = SymbolOffsetProcessor(instrumented_native_lib)

  reached_offsets = instrumented_processor.GetReachedOffsetsFromDumps(dumps)
  if args.offsets_output:
    with file(args.offsets_output, 'w') as f:
      f.write('\n'.join(map(str, reached_offsets)))
  logging.info('Reached Offsets = %d', len(reached_offsets))

  primary_map = instrumented_processor.OffsetToPrimaryMap()
  reached_primary_symbols = set(
      primary_map[offset] for offset in reached_offsets)
  logging.info('Reached symbol names = %d', len(reached_primary_symbols))

  regular_processor = SymbolOffsetProcessor(regular_native_lib)
  matched_in_regular_build = regular_processor.MatchSymbolNames(
      s.name for s in reached_primary_symbols)
  logging.info('Matched symbols = %d', len(matched_in_regular_build))
  total_size = sum(s.size for s in matched_in_regular_build)
  logging.info('Total reached size = %d', total_size)

  with open(args.output, 'w') as f:
    for s in matched_in_regular_build:
      f.write(s.name + '\n')


if __name__ == '__main__':
  main()
