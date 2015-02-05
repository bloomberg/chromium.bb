#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to get and manipulate symbols from a binary."""

import collections
import os
import re
import subprocess
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                    'third_party', 'android_platform', 'development',
                    'scripts'))
import symbol


SymbolInfo = collections.namedtuple('SymbolInfo', ('name', 'offset', 'size',
                                                   'section'))

def SetArchitecture(arch):
  """Set the architecture for binaries to be symbolized."""
  symbol.ARCH = arch


def _FromObjdumpLine(line):
  """Create a SymbolInfo by parsing a properly formatted objdump output line.

  Args:
    line: line from objdump

  Returns:
    An instance of SymbolInfo if the line represents a symbol, None otherwise.
  """
  # All of the symbol lines we care about are in the form
  # 0000000000  g    F   .text.foo     000000000 [.hidden] foo
  # where g (global) might also be l (local) or w (weak).
  parts = line.split()
  if len(parts) < 6 or parts[2] != 'F':
    return None

  assert len(parts) == 6 or (len(parts) == 7 and parts[5] == '.hidden')
  accepted_scopes = set(['g', 'l', 'w'])
  assert parts[1] in accepted_scopes

  offset = int(parts[0], 16)
  section = parts[3]
  size = int(parts[4], 16)
  name = parts[-1].rstrip('\n')
  assert re.match('^[a-zA-Z0-9_.]+$', name)
  return SymbolInfo(name=name, offset=offset, section=section, size=size)


def _SymbolInfosFromStream(objdump_lines):
  """Parses the output of objdump, and get all the symbols from a binary.

  Args:
    objdump_lines: An iterable of lines

  Returns:
    A list of SymbolInfo.
  """
  symbol_infos = []
  for line in objdump_lines:
    symbol_info = _FromObjdumpLine(line)
    if symbol_info is not None:
      symbol_infos.append(symbol_info)
  return symbol_infos


def SymbolInfosFromBinary(binary_filename):
  """Runs objdump to get all the symbols from a binary.

  Args:
    binary_filename: path to the binary.

  Returns:
    A list of SymbolInfo from the binary.
  """
  command = (symbol.ToolPath('objdump'), '-t', '-w', binary_filename)
  p = subprocess.Popen(command, shell=False, stdout=subprocess.PIPE)
  try:
    result = _SymbolInfosFromStream(p.stdout)
    return result
  finally:
    p.wait()


def GroupSymbolInfosByOffset(symbol_infos):
  """Create a dict {offset: [symbol_info1, ...], ...}.

  As several symbols can be at the same offset, this is a 1-to-many
  relationship.

  Args:
    symbol_infos: iterable of SymbolInfo instances

  Returns:
    a dict {offset: [symbol_info1, ...], ...}
  """
  offset_to_symbol_infos = collections.defaultdict(list)
  for symbol_info in symbol_infos:
    offset_to_symbol_infos[symbol_info.offset].append(symbol_info)
  return dict(offset_to_symbol_infos)


def CreateNameToSymbolInfo(symbol_infos):
  """Create a dict {name: symbol_info, ...}.

  Args:
    symbol_infos: iterable of SymbolInfo instances

  Returns:
    a dict {name: symbol_info, ...}
  """
  return {symbol_info.name: symbol_info for symbol_info in symbol_infos}
