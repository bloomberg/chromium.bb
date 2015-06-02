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

import cyglog_to_orderfile
import cygprofile_utils
import symbol_extractor

# Prefixes for the symbols. We strip them from the incoming symbols, and add
# them back in the output file.
_PREFIXES = ('.text.startup.', '.text.hot.', '.text.unlikely.', '.text.')


def UniqueGenerator(generator):
  """Make the output of a generator unique."""
  def Wrapper(*args, **kwargs):
    returned = set()
    for item in generator(*args, **kwargs):
      if item in returned:
        continue
      returned.add(item)
      yield item
  return Wrapper


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
  for prefix in _PREFIXES:
    if line.startswith(prefix):
      return line[len(prefix):]
  return line  # Unprefixed case


def _SectionNameToSymbols(section_name, section_to_symbols_map):
  """Returns all symbols which could be referred to by section_name."""
  if (not section_name or
      section_name == '.text' or
      section_name.endswith('*')):
    return  # Don't return anything for catch-all sections
  if section_name in section_to_symbols_map:
    for symbol in section_to_symbols_map[section_name]:
      yield symbol
  else:
    section_name = _StripPrefix(section_name)
    name = _RemoveClone(section_name)
    if name:
      yield section_name


def GetSectionsFromOrderfile(filename):
  """Yields the sections from an orderfile.

  Args:
    filename: The name of the orderfile.

  Yields:
    A list of symbol names.
  """
  with open(filename, 'r') as f:
    for line in f.xreadlines():
      line = line.rstrip('\n')
      if line:
        yield line


@UniqueGenerator
def GetSymbolsFromOrderfile(filename, section_to_symbols_map):
  """Yields the symbols from an orderfile.

  Args:
    filename: The name of the orderfile.
    section_to_symbols_map: The mapping from section to symbol name.  If a
                            section isn't in the mapping, it is assumed the
                            section name is the prefixed symbol name.

  Yields:
    A list of symbol names.
  """
  for section in GetSectionsFromOrderfile(filename):
    for symbol in _SectionNameToSymbols(section, section_to_symbols_map):
      yield symbol


def _SymbolsWithSameOffset(profiled_symbol, name_to_symbol_info,
                           offset_to_symbol_info):
  """Expand a symbol to include all symbols with the same offset.

  Args:
    profiled_symbol: the string symbol name to be expanded.
    name_to_symbol_info: {name: [symbol_info1], ...}, as returned by
        GetSymbolInfosFromBinary
    offset_to_symbol_info: {offset: [symbol_info1, ...], ...}

  Returns:
    A list of symbol names, or an empty list if profiled_symbol was not in
    name_to_symbol_info.
  """
  if profiled_symbol not in name_to_symbol_info:
    return []
  symbol_infos = name_to_symbol_info[profiled_symbol]
  expanded = []
  for symbol_info in symbol_infos:
    expanded += (s.name for s in offset_to_symbol_info[symbol_info.offset])
  return expanded


@UniqueGenerator
def _ExpandSection(section_name,
                   name_to_symbol_infos, offset_to_symbol_infos,
                   section_to_symbols_map, symbol_to_sections_map):
  """Get all the sections which might contain the same code as section_name.

  Args:
    section_name: The section to expand.
    name_to_symbol_infos: {name: [symbol_info1], ...}, as returned by
        GetSymbolInfosFromBinary.
    offset_to_symbol_infos: {offset: [symbol_info1, ...], ...}
    section_to_symbols_map: The mapping from section to symbol name.  If a
        section isn't in the mapping, it is assumed the section name is the
        prefixed symbol name.
    symbol_to_sections_map: The mapping from symbol name to names of linker
        sections containing the symbol.  If a symbol isn't in the mapping, the
        set of _PREFIXES on symbol is assumed for the section names.

  Yields:
    Section names including at least section_name.
  """
  yield section_name
  for first_sym in _SectionNameToSymbols(section_name,
                                         section_to_symbols_map):
    for symbol in _SymbolsWithSameOffset(first_sym, name_to_symbol_infos,
                                         offset_to_symbol_infos):
      if symbol in symbol_to_sections_map:
        for section in symbol_to_sections_map[symbol]:
          yield section
      else:
        for prefix in _PREFIXES:
          yield prefix + symbol


@UniqueGenerator
def _ExpandSections(section_names,
                    name_to_symbol_infos, offset_to_symbol_infos,
                    section_to_symbols_map, symbol_to_sections_map):
  """Expand the set of sections with sections which contain the same symbols.

  Args:
    section_names: The sections to expand.
    name_to_symbol_infos: {name: [symbol_info1], ...}, as returned by
        GetSymbolInfosFromBinary.
    offset_to_symbol_infos: {offset: [symbol_info1, ...], ...}
    section_to_symbols_map: The mapping from section to symbol names.  If a
                            section isn't in the mapping, it is assumed the
                            section name is the prefixed symbol name.
    symbol_to_sections_map: The mapping from symbol name to names of linker
                            sections containing the symbol.  If a symbol isn't
                            in the mapping, the set of _PREFIXES on symbol is
                            assumed for the section names.

  Yields:
    Section names including at least section_names.
  """
  for profiled_section in section_names:
    for section in _ExpandSection(
        profiled_section,
        name_to_symbol_infos, offset_to_symbol_infos,
        section_to_symbols_map, symbol_to_sections_map):
      yield section


def InvertMapping(x_to_ys):
  """Given a map x -> [y1, y2...] return inverse mapping y->[x1, x2...]."""
  y_to_xs = {}
  for x, ys in x_to_ys.items():
    for y in ys:
      y_to_xs.setdefault(y, []).append(x)
  return y_to_xs


def main(argv):
  parser = optparse.OptionParser(usage=
      'usage: %prog [options] <unpatched_orderfile> <library>')
  parser.add_option('--target-arch', action='store', dest='arch',
                    choices=['arm', 'arm64', 'x86', 'x86_64', 'x64', 'mips'],
                    help='The target architecture for the library.')
  options, argv = parser.parse_args(argv)
  if not options.arch:
    options.arch = cygprofile_utils.DetectArchitecture()
  if len(argv) != 3:
    parser.print_help()
    return 1
  orderfile_filename = argv[1]
  binary_filename = argv[2]
  symbol_extractor.SetArchitecture(options.arch)
  (offset_to_symbol_infos, name_to_symbol_infos) = _GroupSymbolInfosFromBinary(
      binary_filename)
  obj_dir = cygprofile_utils.GetObjDir(binary_filename)
  symbol_to_sections_map = \
      cyglog_to_orderfile.GetSymbolToSectionsMapFromObjectFiles(obj_dir)
  section_to_symbols_map = InvertMapping(symbol_to_sections_map)
  profiled_sections = GetSectionsFromOrderfile(orderfile_filename)
  expanded_sections = _ExpandSections(
      profiled_sections,
      name_to_symbol_infos, offset_to_symbol_infos,
      section_to_symbols_map, symbol_to_sections_map)
  for section in expanded_sections:
    print section
  # The following is needed otherwise Gold only applies a partial sort.
  print '.text'  # gets methods not in a section, such as assembly
  for prefix in _PREFIXES:
    print prefix + '*'  # gets everything else
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main(sys.argv))
