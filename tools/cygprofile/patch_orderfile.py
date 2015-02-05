#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Patch an orderfile.

Starting with a list of symbols in a binary and an orderfile (ordered list of
symbols), matches the symbols in the orderfile and augments each symbol with the
symbols residing at the same address (due to having identical code).

Note: It is possible to have.
- Several symbols mapping to the same offset in the binary.
- Several offsets for a given symbol (because we strip the ".clone." suffix)

TODO(lizeb): Since the suffix ".clone." is only used with -O3 that we don't
currently use, simplify the logic by removing the suffix handling.

The general pipeline is:
1. Get the symbol infos (name, offset, size, section) from the binary
2. Get the symbol names from the orderfile
3. Find the orderfile symbol names in the symbols coming from the binary
4. For each symbol found, get all the symbols at the same address
5. Output them to an updated orderfile, with several different prefixes
"""

import collections
import logging
import optparse
import sys

import symbol_extractor

# Prefixes for the symbols. We strip them from the incoming symbols, and add
# them back in the output file.
_PREFIXES = ('.text.startup.', '.text.hot.', '.text.unlikely.', '.text.')


def _RemoveClone(name):
  """Return name up to the ".clone." marker."""
  clone_index = name.find('.clone.')
  if clone_index != -1:
    return name[:clone_index]
  return name


def _GroupSymbolInfos(symbol_infos):
  """Group the symbol infos by name and offset.

  Args:
    symbol_infos: an iterable of SymbolInfo

  Returns:
    The same output as _GroupSymbolInfosFromBinary.
  """
  # Map the addresses to symbols.
  offset_to_symbol_infos = collections.defaultdict(list)
  name_to_symbol_infos = collections.defaultdict(list)
  for symbol in symbol_infos:
    symbol = symbol_extractor.SymbolInfo(name=_RemoveClone(symbol.name),
                                         offset=symbol.offset,
                                         size=symbol.size,
                                         section=symbol.section)
    offset_to_symbol_infos[symbol.offset].append(symbol)
    name_to_symbol_infos[symbol.name].append(symbol)
  return (dict(offset_to_symbol_infos), dict(name_to_symbol_infos))


def _GroupSymbolInfosFromBinary(binary_filename):
  """Group all the symbols from a binary by name and offset.

  Args:
    binary_filename: path to the binary.

  Returns:
    A tuple of dict:
    (offset_to_symbol_infos, name_to_symbol_infos):
    - offset_to_symbol_infos: {offset: [symbol_info1, ...]}
    - name_to_symbol_infos: {name: [symbol_info1, ...]}
  """
  symbol_infos = symbol_extractor.SymbolInfosFromBinary(binary_filename)
  return _GroupSymbolInfos(symbol_infos)


def _StripPrefix(line):
  """Get the symbol from a line with a linker section name.

  Args:
    line: a line from an orderfile, usually in the form:
          .text.SymbolName

  Returns:
    The symbol, SymbolName in the example above.
  """
  line = line.rstrip('\n')
  for prefix in _PREFIXES:
    if line.startswith(prefix):
      return line[len(prefix):]
  return line  # Unprefixed case


def _GetSymbolsFromStream(lines):
  """Get the symbols from an iterable of lines.
     Filters out wildcards and lines which do not correspond to symbols.

  Args:
    lines: iterable of lines from an orderfile.

  Returns:
    Same as GetSymbolsFromOrderfile
  """
  # TODO(lizeb): Retain the prefixes later in the processing stages.
  symbols = []
  unique_symbols = set()
  for line in lines:
    line = _StripPrefix(line)
    name = _RemoveClone(line)
    if name == '' or name == '*' or name == '.text':
      continue
    if not line in unique_symbols:
      symbols.append(line)
      unique_symbols.add(line)
  return symbols


def GetSymbolsFromOrderfile(filename):
  """Return the symbols from an orderfile.

  Args:
    filename: The name of the orderfile.

  Returns:
    A list of symbol names.
  """
  with open(filename, 'r') as f:
    return _GetSymbolsFromStream(f.xreadlines())

def _SymbolsWithSameOffset(profiled_symbol, name_to_symbol_info,
                           offset_to_symbol_info):
  """Expand a profiled symbol to include all symbols which share an offset
     with that symbol.
  Args:
    profiled_symbol: the string symbol name to be expanded.
    name_to_symbol_info: {name: [symbol_info1], ...}, as returned by
        GetSymbolInfosFromBinary
    offset_to_symbol_info: {offset: [symbol_info1, ...], ...}

  Returns:
    A list of symbol names, or an empty list if profiled_symbol was not in
    name_to_symbol_info.
  """
  if not profiled_symbol in name_to_symbol_info:
    return []
  symbol_infos = name_to_symbol_info[profiled_symbol]
  expanded = []
  for symbol_info in symbol_infos:
    expanded += (s.name for s in offset_to_symbol_info[symbol_info.offset])
  return expanded

def _ExpandSymbols(profiled_symbols, name_to_symbol_infos,
                   offset_to_symbol_infos):
  """Expand all of the symbols in profiled_symbols to include any symbols which
     share the same address.

  Args:
    profiled_symbols: Symbols to match
    name_to_symbol_infos: {name: [symbol_info1], ...}, as returned by
        GetSymbolInfosFromBinary
    offset_to_symbol_infos: {offset: [symbol_info1, ...], ...}

  Returns:
    A list of the symbol names.
  """
  found_symbols = 0
  missing_symbols = []
  all_symbols = []
  for name in profiled_symbols:
    expansion = _SymbolsWithSameOffset(name,
        name_to_symbol_infos, offset_to_symbol_infos)
    if expansion:
      found_symbols += 1
      all_symbols += expansion
    else:
      all_symbols.append(name)
      missing_symbols.append(name)
  logging.info('symbols found: %d\n' % found_symbols)
  if missing_symbols > 0:
    logging.warning('%d missing symbols.' % len(missing_symbols))
    missing_symbols_to_show = min(100, len(missing_symbols))
    logging.warning('First %d missing symbols:\n%s' % (
        missing_symbols_to_show,
        '\n'.join(missing_symbols[:missing_symbols_to_show])))
  return all_symbols


def _PrintSymbolsWithPrefixes(symbol_names, output_file):
  """For each symbol, outputs it to output_file with the prefixes."""
  unique_outputs = set()
  for name in symbol_names:
    for prefix in _PREFIXES:
      linker_section = prefix + name
      if not linker_section in unique_outputs:
        output_file.write(linker_section + '\n')
        unique_outputs.add(linker_section)


def main(argv):
  parser = optparse.OptionParser(usage=
      'usage: %prog [options] <unpatched_orderfile> <library>')
  parser.add_option('--target-arch', action='store', dest='arch',
                    default='arm',
                    choices=['arm', 'arm64', 'x86', 'x86_64', 'x64', 'mips'],
                    help='The target architecture for the library.')
  options, argv = parser.parse_args(argv)
  if len(argv) != 3:
    parser.print_help()
    return 1
  orderfile_filename = argv[1]
  binary_filename = argv[2]
  symbol_extractor.SetArchitecture(options.arch)
  (offset_to_symbol_infos, name_to_symbol_infos) = _GroupSymbolInfosFromBinary(
      binary_filename)
  profiled_symbols = GetSymbolsFromOrderfile(orderfile_filename)
  expanded_symbols = _ExpandSymbols(
      profiled_symbols, name_to_symbol_infos, offset_to_symbol_infos)
  _PrintSymbolsWithPrefixes(expanded_symbols, sys.stdout)
  # The following is needed otherwise Gold only applies a partial sort.
  print '.text'    # gets methods not in a section, such as assembly
  print '.text.*'  # gets everything else
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main(sys.argv))
