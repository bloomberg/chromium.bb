#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import sys

from static_symbols import StaticSymbolsInFile
from proc_maps import ProcMaps


_MAPS_FILENAME = 'maps'
_FILES_FILENAME = 'files.json'


class _ListOutput(object):
  def __init__(self, result):
    self.result = result

  def output(self, address, symbol):  # pylint: disable=W0613
    self.result.append(symbol)


class _DictOutput(object):
  def __init__(self, result):
    self.result = result

  def output(self, address, symbol):
    self.result[address] = symbol


class _FileOutput(object):
  def __init__(self, result, with_address):
    self.result = result
    self.with_address = with_address

  def output(self, address, symbol):
    if self.with_address:
      self.result.write('%016x %s\n' % (address, symbol))
    else:
      self.result.write('%s\n' % symbol)


class RuntimeSymbolsInProcess(object):
  def __init__(self):
    self._maps = None
    self._static_symbols_in_filse = {}

  def find_procedure(self, runtime_address):
    for vma in self._maps.iter(ProcMaps.executable):
      if vma.begin <= runtime_address < vma.end:
        static_symbols = self._static_symbols_in_filse.get(vma.name)
        if static_symbols:
          return static_symbols.find_procedure_by_runtime_address(
              runtime_address, vma)
        else:
          return None
    return None

  def find_typeinfo(self, runtime_address):
    for vma in self._maps.iter(ProcMaps.constants):
      if vma.begin <= runtime_address < vma.end:
        static_symbols = self._static_symbols_in_filse.get(vma.name)
        if static_symbols:
          return static_symbols.find_typeinfo_by_runtime_address(
              runtime_address, vma)
        else:
          return None
    return None

  @staticmethod
  def load(prepared_data_dir):
    symbols_in_process = RuntimeSymbolsInProcess()

    with open(os.path.join(prepared_data_dir, _MAPS_FILENAME), mode='r') as f:
      symbols_in_process._maps = ProcMaps.load(f)
    with open(os.path.join(prepared_data_dir, _FILES_FILENAME), mode='r') as f:
      files = json.load(f)

    # pylint: disable=W0212
    for vma in symbols_in_process._maps.iter(ProcMaps.executable_and_constants):
      file_entry = files.get(vma.name)
      if not file_entry:
        continue

      static_symbols = StaticSymbolsInFile(vma.name)

      nm_entry = file_entry.get('nm')
      if nm_entry and nm_entry['format'] == 'bsd':
        with open(os.path.join(prepared_data_dir, nm_entry['file']), 'r') as f:
          static_symbols.load_nm_bsd(f, nm_entry['mangled'])

      readelf_entry = file_entry.get('readelf-e')
      if readelf_entry:
        with open(os.path.join(prepared_data_dir, readelf_entry['file']),
                  'r') as f:
          static_symbols.load_readelf_ew(f)

      symbols_in_process._static_symbols_in_filse[vma.name] = static_symbols

    return symbols_in_process


def _find_runtime_symbols(symbols_in_process, addresses, outputter):
  for address in addresses:
    if isinstance(address, basestring):
      address = int(address, 16)
    found = symbols_in_process.find_procedure(address)
    if found:
      outputter.output(address, found.name)
    else:
      outputter.output(address, '0x%016x' % address)


def _find_runtime_typeinfo_symbols(symbols_in_process, addresses, outputter):
  for address in addresses:
    if isinstance(address, basestring):
      address = int(address, 16)
    if address == 0:
      outputter.output(address, 'no typeinfo')
    else:
      found = symbols_in_process.find_typeinfo(address)
      if found:
        if found.startswith('typeinfo for '):
          outputter.output(address, found[13:])
        else:
          outputter.output(address, found)
      else:
        outputter.output(address, '0x%016x' % address)


def find_runtime_typeinfo_symbols_list(symbols_in_process, addresses):
  result = []
  _find_runtime_typeinfo_symbols(
      symbols_in_process, addresses, _ListOutput(result))
  return result


def find_runtime_typeinfo_symbols_dict(symbols_in_process, addresses):
  result = {}
  _find_runtime_typeinfo_symbols(
      symbols_in_process, addresses, _DictOutput(result))
  return result


def find_runtime_typeinfo_symbols_file(symbols_in_process, addresses, f):
  _find_runtime_typeinfo_symbols(
      symbols_in_process, addresses, _FileOutput(f, False))


def find_runtime_symbols_list(symbols_in_process, addresses):
  result = []
  _find_runtime_symbols(symbols_in_process, addresses, _ListOutput(result))
  return result


def find_runtime_symbols_dict(symbols_in_process, addresses):
  result = {}
  _find_runtime_symbols(symbols_in_process, addresses, _DictOutput(result))
  return result


def find_runtime_symbols_file(symbols_in_process, addresses, f):
  _find_runtime_symbols(
      symbols_in_process, addresses, _FileOutput(f, False))


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

  symbols_in_process = RuntimeSymbolsInProcess.load(prepared_data_dir)
  return find_runtime_symbols_file(symbols_in_process, sys.stdin, sys.stdout)


if __name__ == '__main__':
  sys.exit(main())
