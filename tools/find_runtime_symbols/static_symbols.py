# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bisect
import json
import os
import re
import sys

from parse_proc_maps import parse_proc_maps
from util import executable_condition


_ARGUMENT_TYPE_PATTERN = re.compile('\([^()]*\)(\s*const)?')
_TEMPLATE_ARGUMENT_PATTERN = re.compile('<[^<>]*>')
_LEADING_TYPE_PATTERN = re.compile('^.*\s+(\w+::)')


class ParsingException(Exception):
  def __str__(self):
    return repr(self.args[0])


class StaticSymbols(object):
  """Represents static symbol information."""

  def __init__(self, maps, procedure_boundaries):
    self.maps = maps
    self.procedure_boundaries = procedure_boundaries

  # TODO(dmikurube): It will be deprecated.
  @staticmethod
  def _load_nm(prepared_data_dir, maps_filename, nm_json_filename):
    with open(os.path.join(prepared_data_dir, maps_filename), mode='r') as f:
      maps = parse_proc_maps(f)
    with open(os.path.join(prepared_data_dir, nm_json_filename), mode='r') as f:
      nm_files = json.load(f)

    symbol_tables = {}
    for entry in maps.iter(executable_condition):
      if nm_files.has_key(entry.name):
        if nm_files[entry.name]['format'] == 'bsd':
          with open(os.path.join(prepared_data_dir,
                                 nm_files[entry.name]['file']), mode='r') as f:
            symbol_tables[entry.name] = _get_static_symbols_from_nm_bsd(
                f, nm_files[entry.name]['mangled'])

    return StaticSymbols(maps, symbol_tables)

  @staticmethod
  def _load_files(prepared_data_dir, maps_filename, files_filename):
    with open(os.path.join(prepared_data_dir, maps_filename), mode='r') as f:
      maps = parse_proc_maps(f)
    with open(os.path.join(prepared_data_dir, files_filename), mode='r') as f:
      files = json.load(f)

    symbol_tables = {}
    for entry in maps.iter(executable_condition):
      if entry.name in files:
        if 'nm' in files[entry.name]:
          nm_entry = files[entry.name]['nm']
          if nm_entry['format'] == 'bsd':
            with open(os.path.join(prepared_data_dir, nm_entry['file']),
                      mode='r') as f:
              symbol_tables[entry.name] = _get_static_symbols_from_nm_bsd(
                  f, nm_entry['mangled'])
        if 'readelf-e' in files:
          readelf_entry = files[entry.name]['readelf-e']
          # TODO(dmikurube) Implement it.

    return StaticSymbols(maps, symbol_tables)

  @staticmethod
  def load(prepared_data_dir):
    if os.path.exists(os.path.join(prepared_data_dir, 'nm.json')):
      return StaticSymbols._load_nm(prepared_data_dir, 'maps', 'nm.json')
    else:
      return StaticSymbols._load_files(prepared_data_dir, 'maps', 'files.json')


class ProcedureBoundary(object):
  """A class for a procedure symbol and an address range for the symbol."""

  def __init__(self, start, end, name):
    self.start = start
    self.end = end
    self.name = name


class ProcedureBoundaryTable(object):
  """A class of a set of ProcedureBoundary."""

  def __init__(self):
    self.sorted_value_list = []
    self.dictionary = {}
    self.sorted = True

  def append(self, entry):
    if self.sorted_value_list:
      if self.sorted_value_list[-1] > entry.start:
        self.sorted = False
      elif self.sorted_value_list[-1] == entry.start:
        return
    self.sorted_value_list.append(entry.start)
    self.dictionary[entry.start] = entry

  def find_procedure(self, address):
    if not self.sorted:
      self.sorted_value_list.sort()
      self.sorted = True
    found_index = bisect.bisect_left(self.sorted_value_list, address)
    found_start_address = self.sorted_value_list[found_index - 1]
    return self.dictionary[found_start_address]


def _get_short_function_name(function):
  while True:
    function, number = _ARGUMENT_TYPE_PATTERN.subn('', function)
    if not number:
      break
  while True:
    function, number = _TEMPLATE_ARGUMENT_PATTERN.subn('', function)
    if not number:
      break
  return _LEADING_TYPE_PATTERN.sub('\g<1>', function)


def _parse_nm_bsd_line(line):
  if line[8] == ' ':
    return line[0:8], line[9], line[11:]
  elif line[16] == ' ':
    return line[0:16], line[17], line[19:]
  raise ParsingException('Invalid nm output.')


def _get_static_symbols_from_nm_bsd(f, mangled=False):
  """Gets procedure boundaries from a result of nm -n --format bsd.

  Args:
      f: A file object containing a result of nm.  It must be sorted and
          in BSD-style.  (Use "[eu-]nm -n --format bsd")

  Returns:
      A result ProcedureBoundaryTable object.
  """
  symbol_table = ProcedureBoundaryTable()

  last_start = 0
  routine = ''

  for line in f:
    sym_value, sym_type, sym_name = _parse_nm_bsd_line(line)

    if sym_value[0] == ' ':
      continue

    start_val = int(sym_value, 16)

    # It's possible for two symbols to share the same address, if
    # one is a zero-length variable (like __start_google_malloc) or
    # one symbol is a weak alias to another (like __libc_malloc).
    # In such cases, we want to ignore all values except for the
    # actual symbol, which in nm-speak has type "T".  The logic
    # below does this, though it's a bit tricky: what happens when
    # we have a series of lines with the same address, is the first
    # one gets queued up to be processed.  However, it won't
    # *actually* be processed until later, when we read a line with
    # a different address.  That means that as long as we're reading
    # lines with the same address, we have a chance to replace that
    # item in the queue, which we do whenever we see a 'T' entry --
    # that is, a line with type 'T'.  If we never see a 'T' entry,
    # we'll just go ahead and process the first entry (which never
    # got touched in the queue), and ignore the others.
    if start_val == last_start and (sym_type == 't' or sym_type == 'T'):
      # We are the 'T' symbol at this address, replace previous symbol.
      routine = sym_name
      continue
    elif start_val == last_start:
      # We're not the 'T' symbol at this address, so ignore us.
      continue

    # Tag this routine with the starting address in case the image
    # has multiple occurrences of this routine.  We use a syntax
    # that resembles template paramters that are automatically
    # stripped out by ShortFunctionName()
    sym_name += "<%016x>" % start_val

    if not mangled:
      routine = _get_short_function_name(routine)
    symbol_table.append(ProcedureBoundary(last_start, start_val, routine))

    last_start = start_val
    routine = sym_name

  if not mangled:
    routine = _get_short_function_name(routine)
  symbol_table.append(ProcedureBoundary(last_start, last_start, routine))
  return symbol_table
