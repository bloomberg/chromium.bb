#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys

from static_symbols import StaticSymbols
from util import executable_condition


def _determine_symbol_name(address, symbol):
  if symbol:
    return symbol.name
  else:
    return '0x%016x' % address


class _ListOutput(object):
  def __init__(self, result):
    self.result = result

  def output(self, address, symbol=None):
    self.result.append(_determine_symbol_name(address, symbol))


class _DictOutput(object):
  def __init__(self, result):
    self.result = result

  def output(self, address, symbol=None):
    self.result[address] = _determine_symbol_name(address, symbol)


class _FileOutput(object):
  def __init__(self, result, with_address):
    self.result = result
    self.with_address = with_address

  def output(self, address, symbol=None):
    symbol_name = _determine_symbol_name(address, symbol)
    if self.with_address:
      self.result.write('%016x %s\n' % (address, symbol_name))
    else:
      self.result.write('%s\n' % symbol_name)


def _find_runtime_symbols(static_symbols, addresses, outputter):
  maps = static_symbols.maps
  symbol_tables = static_symbols.procedure_boundaries

  for address in addresses:
    if isinstance(address, str):
      address = int(address, 16)
    is_found = False
    for entry in maps.iter(executable_condition):
      if entry.begin <= address < entry.end:
        if entry.name in symbol_tables:
          found = symbol_tables[entry.name].find_procedure(
              address - (entry.begin - entry.offset))
          outputter.output(address, found)
        else:
          outputter.output(address)
        is_found = True
        break
    if not is_found:
      outputter.output(address)

  return 0


def find_runtime_symbols_list(static_symbols, addresses):
  result = []
  _find_runtime_symbols(static_symbols, addresses, _ListOutput(result))
  return result


def find_runtime_symbols_dict(static_symbols, addresses):
  result = {}
  _find_runtime_symbols(static_symbols, addresses, _DictOutput(result))
  return result


def find_runtime_symbols_file(static_symbols, addresses, f):
  _find_runtime_symbols(
      static_symbols, addresses, _FileOutput(f, False))


def main():
  # FIX: Accept only .pre data
  if len(sys.argv) < 2:
    sys.stderr.write("""Usage:
%s /path/to/prepared_data_dir/ < addresses.txt
""" % sys.argv[0])
    return 1

  log = logging.getLogger('find_runtime_symbols')
  log.setLevel(logging.WARN)
  handler = logging.StreamHandler()
  handler.setLevel(logging.WARN)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  log.addHandler(handler)

  prepared_data_dir = sys.argv[1]
  if not os.path.exists(prepared_data_dir):
    log.warn("Nothing found: %s" % prepared_data_dir)
    return 1
  if not os.path.isdir(prepared_data_dir):
    log.warn("Not a directory: %s" % prepared_data_dir)
    return 1

  static_symbols = StaticSymbols.load(prepared_data_dir)
  return find_runtime_symbols_file(static_symbols, sys.stdin, sys.stdout)


if __name__ == '__main__':
  sys.exit(main())
