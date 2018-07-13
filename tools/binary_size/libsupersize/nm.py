# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs nm on every .o file that comprises an ELF (plus some analysis).

The design of this file is entirely to work around Python's lack of concurrency.

CollectAliasesByAddress():
  Runs "nm" on the elf to collect all symbol names. This reveals symbol names of
  identical-code-folded functions.

CollectAliasesByAddressAsync():
  Runs CollectAliasesByAddress in a subprocess and returns a promise.

RunNmOnIntermediates():
  BulkForkAndCall() target: Runs nm on a .a file or a list of .o files, parses
  the output, extracts symbol information, and (if available) extracts string
  offset information.
"""

import collections
import os
import subprocess

import concurrent
import demangle
import path_util


def _IsRelevantNmName(name):
  # Skip lines like:
  # 00000000 t $t
  # 00000000 r $d.23
  # 00000344 N
  return name and not name.startswith('$')


def _IsRelevantObjectFileName(name):
  # Prevent marking compiler-generated symbols as candidates for shared paths.
  # E.g., multiple files might have "CSWTCH.12", but they are different symbols.
  #
  # Find these via:
  #   size_info.symbols.GroupedByFullName(min_count=-2).Filter(
  #       lambda s: s.WhereObjectPathMatches('{')).SortedByCount()
  # and then search for {shared}.
  # List of names this applies to:
  #   startup
  #   __tcf_0  <-- Generated for global destructors.
  #   ._79
  #   .Lswitch.table, .Lswitch.table.12
  #   CSWTCH.12
  #   lock.12
  #   table.12
  #   __compound_literal.12
  #   .L.ref.tmp.1
  #   .L.str, .L.str.3
  #   .L__func__.main:  (when using __func__)
  #   .L__FUNCTION__._ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv
  #   .L__PRETTY_FUNCTION__._Unwind_Resume
  #   .L_ZZ24ScaleARGBFilterCols_NEONE9dx_offset  (an array literal)
  if name in ('__tcf_0', 'startup'):
    return False
  if name.startswith('._') and name[2:].isdigit():
    return False
  if name.startswith('.L') and name.find('.', 2) != -1:
    return False

  dot_idx = name.find('.')
  if dot_idx == -1:
    return True
  name = name[:dot_idx]

  return name not in ('CSWTCH', 'lock', '__compound_literal', 'table')


def CollectAliasesByAddress(elf_path, tool_prefix):
  """Runs nm on |elf_path| and returns a dict of address->[names]"""
  # Constructors often show up twice, so use sets to ensure no duplicates.
  names_by_address = collections.defaultdict(set)

  # About 60mb of output, but piping takes ~30s, and loading it into RAM
  # directly takes 3s.
  args = [path_util.GetNmPath(tool_prefix), '--no-sort', '--defined-only',
          elf_path]
  output = subprocess.check_output(args)
  for line in output.splitlines():
    space_idx = line.find(' ')
    address_str = line[:space_idx]
    section = line[space_idx + 1]
    mangled_name = line[space_idx + 3:]

    # To verify that rodata does not have aliases:
    #   nm --no-sort --defined-only libchrome.so > nm.out
    #   grep -v '\$' nm.out | grep ' r ' | sort | cut -d' ' -f1 > addrs
    #   wc -l < addrs; uniq < addrs | wc -l
    if section not in 'tTW' or not _IsRelevantNmName(mangled_name):
      continue

    address = int(address_str, 16)
    if not address:
      continue
    names_by_address[address].add(mangled_name)

  # Demangle all names.
  names_by_address = demangle.DemangleSetsInDicts(names_by_address, tool_prefix)

  # Since this is run in a separate process, minimize data passing by returning
  # only aliased symbols.
  # Also: Sort to ensure stable ordering.
  return {k: sorted(v) for k, v in names_by_address.iteritems() if len(v) > 1}


def _CollectAliasesByAddressAsyncHelper(elf_path, tool_prefix):
  result = CollectAliasesByAddress(elf_path, tool_prefix)
  return concurrent.EncodeDictOfLists(result, key_transform=str)


def CollectAliasesByAddressAsync(elf_path, tool_prefix):
  """Calls CollectAliasesByAddress in a helper process. Returns a Result."""
  def decode(encoded):
    return concurrent.DecodeDictOfLists(encoded, key_transform=int)
  return concurrent.ForkAndCall(
      _CollectAliasesByAddressAsyncHelper, (elf_path, tool_prefix),
      decode_func=decode)


def _ParseOneObjectFileNmOutput(lines):
  # Constructors are often repeated because they have the same unmangled
  # name, but multiple mangled names. See:
  # https://stackoverflow.com/questions/6921295/dual-emission-of-constructor-symbols
  symbol_names = set()
  string_addresses = []
  for line in lines:
    if not line:
      break
    space_idx = line.find(' ')  # Skip over address.
    section = line[space_idx + 1]
    mangled_name = line[space_idx + 3:]
    if _IsRelevantNmName(mangled_name):
      # Refer to _IsRelevantObjectFileName() for examples of names.
      if section == 'r' and (
          mangled_name.startswith('.L.str') or
          mangled_name.startswith('.L__') and mangled_name.find('.', 3) != -1):
        # Leave as a string for easier marshalling.
        string_addresses.append(line[:space_idx].lstrip('0') or '0')
      elif _IsRelevantObjectFileName(mangled_name):
        symbol_names.add(mangled_name)
  return string_addresses, symbol_names


# This is a target for BulkForkAndCall().
def RunNmOnIntermediates(target, tool_prefix, output_directory):
  """Returns encoded_symbol_names_by_path, encoded_string_addresses_by_path.

  Args:
    target: Either a single path to a .a (as a string), or a list of .o paths.
  """
  is_archive = isinstance(target, basestring)
  args = [path_util.GetNmPath(tool_prefix), '--no-sort', '--defined-only']
  if is_archive:
    args.append(target)
  else:
    args.extend(target)
  output = subprocess.check_output(args, cwd=output_directory)
  lines = output.splitlines()
  # Empty .a file has no output.
  if not lines:
    return concurrent.EMPTY_ENCODED_DICT, concurrent.EMPTY_ENCODED_DICT
  is_multi_file = not lines[0]
  lines = iter(lines)
  if is_multi_file:
    next(lines)
    path = next(lines)[:-1]  # Path ends with a colon.
  else:
    assert not is_archive
    path = target[0]

  string_addresses_by_path = {}
  symbol_names_by_path = {}
  while path:
    if is_archive:
      # E.g. foo/bar.a(baz.o)
      path = '%s(%s)' % (target, path)

    string_addresses, mangled_symbol_names = _ParseOneObjectFileNmOutput(lines)
    symbol_names_by_path[path] = mangled_symbol_names
    if string_addresses:
      string_addresses_by_path[path] = string_addresses
    path = next(lines, ':')[:-1]

  # The multiprocess API uses pickle, which is ridiculously slow. More than 2x
  # faster to use join & split.
  # TODO(agrieve): We could use path indices as keys rather than paths to cut
  #     down on marshalling overhead.
  return (concurrent.EncodeDictOfLists(symbol_names_by_path),
          concurrent.EncodeDictOfLists(string_addresses_by_path))
