#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import sys

from parse_proc_maps import parse_proc_maps
from procedure_boundaries import get_procedure_boundaries_from_nm_bsd
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


def _find_runtime_symbols(
    prepared_data_dir, addresses, outputter, loglevel=logging.WARN):
  log = logging.getLogger('find_runtime_symbols')
  log.setLevel(loglevel)
  handler = logging.StreamHandler()
  handler.setLevel(loglevel)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  log.addHandler(handler)

  if not os.path.exists(prepared_data_dir):
    log.warn("Nothing found: %s" % prepared_data_dir)
    return 1
  if not os.path.isdir(prepared_data_dir):
    log.warn("Not a directory: %s" % prepared_data_dir)
    return 1

  with open(os.path.join(prepared_data_dir, 'maps'), mode='r') as f:
    maps = parse_proc_maps(f)

  with open(os.path.join(prepared_data_dir, 'nm.json'), mode='r') as f:
    nm_files = json.load(f)

  symbol_table = {}
  for entry in maps.iter(executable_condition):
    if nm_files.has_key(entry.name):
      if nm_files[entry.name]['format'] == 'bsd':
        with open(os.path.join(prepared_data_dir,
                               nm_files[entry.name]['file']), mode='r') as f:
          symbol_table[entry.name] = get_procedure_boundaries_from_nm_bsd(
              f, nm_files[entry.name]['mangled'])

  for address in addresses:
    if isinstance(address, str):
      address = int(address, 16)
    is_found = False
    for entry in maps.iter(executable_condition):
      if entry.begin <= address < entry.end:
        if entry.name in symbol_table:
          found = symbol_table[entry.name].find_procedure(
              address - (entry.begin - entry.offset))
          outputter.output(address, found)
        else:
          outputter.output(address)
        is_found = True
        break
    if not is_found:
      outputter.output(address)

  return 0


def find_runtime_symbols_list(prepared_data_dir, addresses):
  result = []
  _find_runtime_symbols(prepared_data_dir, addresses, _ListOutput(result))
  return result


def find_runtime_symbols_dict(prepared_data_dir, addresses):
  result = {}
  _find_runtime_symbols(prepared_data_dir, addresses, _DictOutput(result))
  return result


def find_runtime_symbols_file(prepared_data_dir, addresses, f):
  _find_runtime_symbols(
      prepared_data_dir, addresses, _FileOutput(f, False))


def main():
  # FIX: Accept only .pre data
  if len(sys.argv) < 2:
    sys.stderr.write("""Usage:
%s /path/to/prepared_data_dir/ < addresses.txt
""" % sys.argv[0])
    return 1

  return find_runtime_symbols_file(sys.argv[1], sys.stdin, sys.stdout)


if __name__ == '__main__':
  sys.exit(main())
