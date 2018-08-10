#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check that symbols are ordered into a binary as they appear in the orderfile.
"""

import logging
import optparse
import sys

import cyglog_to_orderfile
import cygprofile_utils
import patch_orderfile
import symbol_extractor


def _VerifySymbolOrder(orderfile_symbols, symbol_infos):
  """Verify symbol ordering.

  Checks that the non-section symbols in |orderfile_filename| are consistent
  with the offsets |symbol_infos|.

  Args:
    orderfile_symbols: ([str]) list of symbols from orderfile.
    symbol_infos: ([SymbolInfo]) symbol infos from binary.

  Returns:
    True iff the ordering is consistent.
  """
  last_offset = 0
  name_to_offset = {si.name: si.offset for si in symbol_infos}
  missing_count = 0
  for sym in orderfile_symbols:
    if '.' in sym:
      continue  # sym is a section name.
    if sym not in name_to_offset:
      missing_count += 1
      continue
    next_offset = name_to_offset[sym]
    if next_offset < last_offset:
      logging.error('Out of order at %s (%x/%x)', sym, next_offset, last_offset)
      return False
    last_offset = next_offset
  logging.warning('Missing symbols in verification: %d', missing_count)
  return True


def main():
  parser = optparse.OptionParser(usage=
      'usage: %prog [options] <binary> <orderfile>')
  parser.add_option('--target-arch', action='store', dest='arch',
                    choices=['arm', 'arm64', 'x86', 'x86_64', 'x64', 'mips'],
                    help='The target architecture for the binary.')
  parser.add_option('--threshold', action='store', dest='threshold', default=20,
                    help='The maximum allowed number of out-of-order symbols.')
  options, argv = parser.parse_args(sys.argv)
  if not options.arch:
    options.arch = cygprofile_utils.DetectArchitecture()
  if len(argv) != 3:
    parser.print_help()
    return 1
  (binary_filename, orderfile_filename) = argv[1:]

  symbol_extractor.SetArchitecture(options.arch)
  symbol_infos = symbol_extractor.SymbolInfosFromBinary(binary_filename)

  if not _VerifySymbolOrder([sym.strip() for sym in file(orderfile_filename)],
                            symbol_infos):
    return 1


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
