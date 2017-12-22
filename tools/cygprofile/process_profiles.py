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


def _SortedFilenames(filenames):
  """Returns filenames in ascending timestamp order.

  Args:
    filenames: ([str]) List of filenames, matching.  *-TIMESTAMP.*.

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
    filenames: [str] List of dump filenames.

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


def GetOffsetToSymbolInfo(symbol_infos):
  """From a list of symbol infos, returns a offset -> symbol info array.

  Args:
    symbol_infos: ([symbol_extractor.SymbolInfo]) List of sumbols extracted from
                  the native library.

  Returns:
    [symbol_extractor.SymbolInfo or None] For every 4 bytes of the .text
    section, maps it to a symbol, or None.
  """
  min_offset = min(s.offset for s in symbol_infos)
  max_offset = max(s.offset + s.size for s in symbol_infos)
  text_length_words = (max_offset - min_offset) / 4
  offset_to_symbol_info = [None for _ in xrange(text_length_words)]
  for s in symbol_infos:
    offset = s.offset - min_offset
    for i in range(offset / 4, (offset + s.size) / 4):
      offset_to_symbol_info[i] = s
  return offset_to_symbol_info


def GetOffsetToSymbolArray(instrumented_native_lib_filename):
  """From the native library, maps .text offsets to symbols.

  Args:
    instrumented_native_lib_filename: (str) Native library filename.
                                      Has to be the instrumented version.

  Returns:
    [symbol_extractor.SymbolInfo or None] For every 4 bytes of the .text
    section, maps it to a symbol, or None.
  """
  symbol_infos = symbol_extractor.SymbolInfosFromBinary(
      instrumented_native_lib_filename)
  logging.info('%d Symbols', len(symbol_infos))
  return GetOffsetToSymbolInfo(symbol_infos)


def GetReachedSymbolsFromDump(offsets, offset_to_symbol_info):
  """From a dump and an offset->symbol array, returns reached symbols.

  Args:
   offsets: ([int]) As returned by MergeDumps()
   offset_to_symbol_info: As returned by GetOffsetToSymbolArray()

  Returns:
    [symbol_extractor.SymbolInfo] Reached symbols.
  """
  logging.info('Reached offsets = %d', len(offsets))
  logging.info('Offset to Symbol size = %d', len(offset_to_symbol_info))
  assert max(offsets) / 4 <= len(offset_to_symbol_info)
  already_seen = set()
  reached_symbols = []
  reached_return_addresses_not_found = 0
  for offset in offsets:
    symbol_info = offset_to_symbol_info[offset / 4]
    if symbol_info is None:
      reached_return_addresses_not_found += 1
      continue
    if symbol_info in already_seen:
      continue
    reached_symbols.append(symbol_info)
    already_seen.add(symbol_info)
  if reached_return_addresses_not_found:
    logging.warning('%d return addresses don\'t map to any symbol',
                    reached_return_addresses_not_found)
  return reached_symbols


def SymbolNameToPrimary(symbol_infos):
  """Maps a symbol names to a "primary" symbol.

  Several symbols can be aliased to the same address, through ICF. This returns
  the first one. The order is consistent for a given binary, as it's derived
  from the file layout.

  Args:
    symbol_infos: ([symbol_extractor.SymbolInfo])

  Returns:
    {name (str): primary (symbol_extractor.SymbolInfo)}
  """
  symbol_name_to_primary = {}
  offset_to_symbol_info = {}
  for s in symbol_infos:
    if s.offset not in offset_to_symbol_info:
      offset_to_symbol_info[s.offset] = s
  for s in symbol_infos:
    symbol_name_to_primary[s.name] = offset_to_symbol_info[s.offset]
  return symbol_name_to_primary


def MatchSymbolsInRegularBuild(reached_symbol_infos,
                               regular_native_lib_filename):
  """Match a list of symbols to canonical ones on the regular build.

  Args:
    reached_symbol_infos: ([symbol_extractor.SymbolInfo]) Reached symbol
      in the instrumented build.
    regular_native_lib_filename: (str) regular build filename.

  Returns:
    [symbol_extractor.SymbolInfo] list of matched canonical symbols.
  """
  regular_build_symbol_infos = symbol_extractor.SymbolInfosFromBinary(
      regular_native_lib_filename)
  regular_build_symbol_names = set(s.name for s in regular_build_symbol_infos)
  reached_symbol_names = set(s.name for s in reached_symbol_infos)
  logging.info('Reached symbols = %d', len(reached_symbol_names))
  matched_names = reached_symbol_names.intersection(regular_build_symbol_names)
  logging.info('Matched symbols = %d', len(matched_names))

  symbol_name_to_primary = SymbolNameToPrimary(regular_build_symbol_infos)
  matched_primary_symbols = []
  for symbol in reached_symbol_names:
    if symbol in matched_names:
      matched_primary_symbols.append(symbol_name_to_primary[symbol])
  return matched_primary_symbols


def GetReachedSymbolsFromDumpsAndMaybeWriteOffsets(
    dump_filenames, native_lib_filename, output_filename):
  """Merges a list of dumps, returns reached symbols and maybe writes offsets.

  Args:
    dump_filenames: ([str]) List of dump filenames.
    native_lib_filename: (str) Path to the native library.
    output_filename: (str or None) Offset output path, if not None.

  Returns:
    [symbol_extractor.SymbolInfo] Reached symbols.
  """
  offsets = MergeDumps(dump_filenames)
  offset_to_symbol_info = GetOffsetToSymbolArray(native_lib_filename)
  reached_symbols = GetReachedSymbolsFromDump(offsets, offset_to_symbol_info)
  if output_filename:
    offsets = [s.offset for s in reached_symbols]
    with open(output_filename, 'w') as f:
      f.write('\n'.join('%d' % offset for offset in offsets))
  return reached_symbols


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
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.info('Merging dumps')
  dumps = args.dumps.split(',')
  sorted_dumps = _SortedFilenames(dumps)

  instrumented_native_lib = os.path.join(args.instrumented_build_dir,
                                         'lib.unstripped', 'libchrome.so')
  regular_native_lib = os.path.join(args.build_dir,
                                    'lib.unstripped', 'libchrome.so')

  reached_symbols = GetReachedSymbolsFromDumpsAndMaybeWriteOffsets(
      sorted_dumps, instrumented_native_lib, args.offsets_output)
  logging.info('Reached Symbols = %d', len(reached_symbols))
  matched_in_regular_build = MatchSymbolsInRegularBuild(reached_symbols,
                                                        regular_native_lib)
  total_size = sum(s.size for s in matched_in_regular_build)
  logging.info('Total reached size = %d', total_size)

  with open(args.output, 'w') as f:
    for s in matched_in_regular_build:
      f.write(s.name + '\n')


if __name__ == '__main__':
  main()
