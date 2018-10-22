#!/usr/bin/env vpython
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Patch an orderfile.

Starting with a list of symbols in a binary and an orderfile (ordered list of
symbols), matches the symbols in the orderfile and augments each symbol with
the symbols residing at the same address (due to having identical code).  The
output is a list of symbols appropriate for the linker
option --symbol-ordering-file for lld. Note this is not usable with gold (which
uses section names to order the binary).

Note: It is possible to have.
- Several symbols mapping to the same offset in the binary.
- Several offsets for a given symbol (because we strip the ".clone." and other
  suffixes)

The general pipeline is:
1. Get the symbol infos (name, offset, size, section) from the binary
2. Get the symbol names from the orderfile
3. Find the orderfile symbol names in the symbols coming from the binary
4. For each symbol found, get all the symbols at the same address
5. Output them to an updated orderfile suitable lld
"""

import argparse
import collections
import logging
import sys

import cyglog_to_orderfile
import cygprofile_utils
import symbol_extractor

# Suffixes for symbols.  These are due to method splitting for inlining and
# method cloning for various reasons including constant propagation and
# inter-procedural optimization.
_SUFFIXES = ('.clone.', '.part.', '.isra.', '.constprop.')


def RemoveSuffixes(name):
  """Strips method name suffixes from cloning and splitting.

  .clone. comes from cloning in -O3.
  .part.  comes from partial method splitting for inlining.
  .isra.  comes from inter-procedural optimizations.
  .constprop. is cloning for constant propagation.
  """
  for suffix in _SUFFIXES:
    name = name.split(suffix)[0]
  return name


def _UniqueGenerator(generator):
  """Converts a generator to skip yielding elements already seen.

  Example:
    @_UniqueGenerator
    def Foo():
      yield 1
      yield 2
      yield 1
      yield 3

    Foo() yields 1,2,3.
  """
  def _FilteringFunction(*args, **kwargs):
    returned = set()
    for item in generator(*args, **kwargs):
      if item in returned:
        continue
      returned.add(item)
      yield item

  return _FilteringFunction


def _GroupSymbolsByOffset(binary_filename):
  """Produce a map symbol name -> all symbol names at same offset.

  Suffixes are stripped.
  """
  symbol_infos = [
      s._replace(name=RemoveSuffixes(s.name))
      for s in symbol_extractor.SymbolInfosFromBinary(binary_filename)]
  offset_map = symbol_extractor.GroupSymbolInfosByOffset(symbol_infos)
  missing_offsets = 0
  sym_to_matching = {}
  for sym in symbol_infos:
    if sym.offset not in offset_map:
      missing_offsets += 1
      continue
    matching = [s.name for s in offset_map[sym.offset]]
    assert sym.name in matching
    sym_to_matching[sym.name] = matching
  return sym_to_matching


def _StripSuffixes(section_list):
  """Remove all suffixes on items in a list of symbols."""
  return [RemoveSuffixes(section) for section in section_list]


@_UniqueGenerator
def ReadOrderfile(orderfile):
  """Reads an orderfile and cleans up symbols.

  Args:
    orderfile: The name of the orderfile.

  Yields:
    Symbol names, cleaned and unique.
  """
  with open(orderfile) as f:
    for line in f.xreadlines():
      line = line.strip()
      if line:
        yield line


def GeneratePatchedOrderfile(unpatched_orderfile, native_lib_filename,
                             output_filename):
  """Writes a patched orderfile.

  Args:
    unpatched_orderfile: (str) Path to the unpatched orderfile.
    native_lib_filename: (str) Path to the native library.
    output_filename: (str) Path to the patched orderfile.
  """
  symbol_to_matching = _GroupSymbolsByOffset(native_lib_filename)
  profiled_symbols = ReadOrderfile(unpatched_orderfile)
  missing_symbol_count = 0
  seen_symbols = set()
  patched_symbols = []
  for sym in profiled_symbols:
    if sym not in symbol_to_matching:
      missing_symbol_count += 1
      continue
    if sym in seen_symbols:
      continue
    for matching in symbol_to_matching[sym]:
      patched_symbols.append(matching)
      seen_symbols.add(matching)
    assert sym in seen_symbols
  logging.warning('missing symbol count = %d', missing_symbol_count)

  with open(output_filename, 'w') as f:
    # Make sure the anchor functions are located in the right place, here and
    # after everything else.
    # See the comment in //base/android/library_loader/anchor_functions.cc.
    #
    # __cxx_global_var_init is one of the largest symbols (~38kB as of May
    # 2018), called extremely early, and not instrumented.
    for first_section in ('dummy_function_start_of_ordered_text',
                          '__cxx_global_var_init'):
      f.write(first_section + '\n')

    for sym in patched_symbols:
      f.write(sym + '\n')

    f.write('dummy_function_end_of_ordered_text')


def _CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-arch', action='store',
                      choices=['arm', 'arm64', 'x86', 'x86_64', 'x64', 'mips'],
                      help='The target architecture for the library.')
  parser.add_argument('--unpatched-orderfile', required=True,
                      help='Path to the unpatched orderfile')
  parser.add_argument('--native-library', required=True,
                      help='Path to the native library')
  parser.add_argument('--output-file', required=True, help='Output filename')
  return parser


def main():
  parser = _CreateArgumentParser()
  options = parser.parse_args()
  if not options.target_arch:
    options.arch = cygprofile_utils.DetectArchitecture()
  symbol_extractor.SetArchitecture(options.target_arch)
  GeneratePatchedOrderfile(options.unpatched_orderfile, options.native_library,
                           options.output_file)
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
