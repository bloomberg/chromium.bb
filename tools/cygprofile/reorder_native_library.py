#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""From an ordered native library, puts ordered code in the middle of the code
segment.

Putting ordered code (a superset of frequently called code) in the
middle of the binary reduces the number of relative jumps with an offset >16MB,
which requires a thunk with the THUMB-2 instruction set.

This is done the following way:
- Look for the current ordering in an ordered libchrome.
- Crate a complete orderfile (with all the symbols), with the ordered
  symbols in the middle.

The middle is computed by counting symbols, irrespective of their sizes. Local
testing shows that this is sufficient.

Note: only works with LLD, as the output orderfile is tailored to it.
"""

import argparse
import logging
import operator

import symbol_extractor


def _ExtractAndProcessSymbols(native_library_filename):
  """Extracts, sorts and filters symbols.

  Args:
    native_library_filename: (str) Path to the native library

  Returns:
   [symbol_extractor.SymbolInfo], sorted by offset, without thunks.
  """
  logging.info('Extracting symbols')
  symbol_infos = symbol_extractor.SymbolInfosFromBinary(native_library_filename)
  logging.info('%d symbols' % len(symbol_infos))
  symbol_infos.sort(key=operator.attrgetter('offset'))
  real_symbols = [s for s in symbol_infos
                  if not s.name.startswith('__ThumbV7PILongThunk_')]
  logging.info('%d long jumps Thunks' % (len(symbol_infos) - len(real_symbols)))
  return real_symbols


def _ReorderSymbols(symbol_infos):
  """Puts ordered symbols in the middle.

  Args:
    symbols_infos: ([symbol_extractor.SymbolInfo]) as returned by
                   |_ExtractAndProcessSymbols()|.

  Returns:
    [str] Reordered list of symbols.
  """
  symbols = [s.name for s in symbol_infos]
  assert symbols[0] == 'dummy_function_to_anchor_text'
  start_of_ordered_index = symbols.index('dummy_function_start_of_ordered_text')
  end_of_ordered_index = symbols.index('dummy_function_end_of_ordered_text')
  assert end_of_ordered_index > start_of_ordered_index
  logging.info('%d ordered symbols' % (
      end_of_ordered_index - start_of_ordered_index))
  ordered = symbols[start_of_ordered_index:end_of_ordered_index + 1]
  others = symbols[:start_of_ordered_index] + symbols[end_of_ordered_index + 1:]
  middle = len(others) / 2
  reordered = others[:middle] + ordered + others[middle:]
  return reordered


def _CreateNewOrderfile(native_library_filename, output_orderfile, arch):
  """Creates a new reordered orderfile.

  Args:
    native_library_filename: (str) Native library filename.
    output_orderfile: (str) Path to the output orderfile.
    arch: (str) Architecture
  """
  symbol_extractor.SetArchitecture(arch)
  symbol_infos = _ExtractAndProcessSymbols(native_library_filename)
  reordered = _ReorderSymbols(symbol_infos)
  with open(output_orderfile, 'w') as f:
    for s in reordered:
      f.write('%s\n' % s)


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--native-library', help='Path to the native library',
                      required=True)
  parser.add_argument('--output-orderfile', help='Path to the output orderfile',
                      required=True)
  parser.add_argument('--arch', help='Architecture', default='arm')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  _CreateNewOrderfile(args.native_library, args.output_orderfile, args.arch)


if __name__ == '__main__':
  main()
