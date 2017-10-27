#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Lists all the reached symbols from an instrumentation dump."""

import argparse
import logging
import os
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))
path = os.path.join(_SRC_PATH, 'tools', 'cygprofile')
sys.path.append(path)
import symbol_extractor


def ProcessDump(filename):
  """Parses a process dump.

  Args:
    filename: (str) Process dump filename.

  Returns:
    [bool] Reached locations, each element representing 4 bytes in the binary,
    relative to the start of .text.
  """
  data = None
  with open(filename) as f:
    data = f.read().strip()
  result = [x == '1' for x in data]
  logging.info('Reached locations = %d', sum(result))
  return result


def MergeDumps(filenames):
  """Merges several dumps.

  Args:
    filenames: [str] List of dump filenames.

  Returns:
    A bitwise OR of all the dumps as returned by ProcessDump().
  """
  dumps = [ProcessDump(filename) for filename in filenames]
  assert len(set([len(d) for d in dumps])) == 1
  result = dumps[0]
  for d in dumps:
    for (i, x) in enumerate(d):
      result[i] |= x
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


def GetReachedSymbolsFromDump(dump, offset_to_symbol_info):
  """From a dump and an offset->symbol array, returns reached symbols.

  Args:
   dump: As returned by MergeDumps()
   offset_to_symbol_info: As returned by GetOffsetToSymbolArray()

  Returns:
    set(symbol_extractor.SymbolInfo) set of reached symbols.
  """
  logging.info('Dump size = %d', len(dump))
  logging.info('Offset to Symbol size = %d', len(offset_to_symbol_info))
  # It's OK for the dump to be larger if none is reached.
  if len(dump) > len(offset_to_symbol_info):
    assert sum(dump[len(offset_to_symbol_info):]) == 0
    dump = dump[:len(offset_to_symbol_info)]
  reached_symbols = set()
  reached_return_addresses_not_found = 0
  for (reached, symbol_info) in zip(dump, offset_to_symbol_info):
    if not reached:
      continue
    if symbol_info is None:
      reached_return_addresses_not_found += 1
      continue
    reached_symbols.add(symbol_info)
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
  matched_primary_symbols = set()
  for name in matched_names:
    matched_primary_symbols.add(symbol_name_to_primary[name])
  return matched_primary_symbols


def WriteReachedSymbolNames(filename, reached_symbols):
  """Writes the list of reached symbol names to |filename|."""
  with open(filename, 'w') as f:
    for s in reached_symbols:
      f.write(s.name + '\n')


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
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.info('Merging dumps')
  dump = MergeDumps(args.dumps.split(','))
  logging.info('Mapping offsets to symbols')
  instrumented_native_lib = os.path.join(args.instrumented_build_dir,
                                         'lib.unstripped', 'libchrome.so')
  regular_native_lib = os.path.join(args.build_dir,
                                    'lib.unstripped', 'libchrome.so')
  offset_to_symbol_info = GetOffsetToSymbolArray(instrumented_native_lib)
  logging.info('Matching symbols')
  reached_symbols = GetReachedSymbolsFromDump(dump, offset_to_symbol_info)
  logging.info('Reached Symbols = %d', len(reached_symbols))
  total_size = sum(s.size for s in reached_symbols)
  logging.info('Total reached size = %d', total_size)
  matched_in_regular_build = MatchSymbolsInRegularBuild(reached_symbols,
                                                        regular_native_lib)
  WriteReachedSymbolNames(args.output, matched_in_regular_build)


if __name__ == '__main__':
  main()
