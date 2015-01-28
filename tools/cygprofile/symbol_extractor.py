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
    0, os.path.join(sys.path[0], '..', '..', 'third_party', 'android_platform',
                    'development', 'scripts'))
import symbol


# TODO(lizeb): Change symbol.ARCH to the proper value when "arm" is no longer
# the only possible value.
_NM_BINARY = symbol.ToolPath('nm')


SymbolInfo = collections.namedtuple('SymbolInfo', ('name', 'offset', 'size'))


def FromNmLine(line):
  """Create a SymbolInfo by parsing a properly formatted nm output line.

  Args:
    line: line from nm

  Returns:
    An instance of SymbolInfo if the line represents a symbol, None otherwise.
  """
  # We are interested in two types of lines:
  # This:
  # 00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev
  # offset size <symbol_type> symbol_name
  # And that:
  # 0070ee8c T WebRtcSpl_ComplexBitReverse
  # In the second case we don't have a size, so use -1 as a sentinel
  if not re.search(' (t|W|T) ', line):
    return None
  parts = line.split()
  if len(parts) == 4:
    return SymbolInfo(
        offset=int(parts[0], 16), size=int(parts[1], 16), name=parts[3])
  elif len(parts) == 3:
    return SymbolInfo(
        offset=int(parts[0], 16), size=-1, name=parts[2])
  else:
    return None


def SymbolInfosFromStream(nm_lines):
  """Parses the output of nm, and get all the symbols from a binary.

  Args:
    nm_lines: An iterable of lines

  Returns:
    A list of SymbolInfo.
  """
  # TODO(lizeb): Consider switching to objdump to simplify parsing.
  symbol_infos = []
  for line in nm_lines:
    symbol_info = FromNmLine(line)
    if symbol_info is not None:
      symbol_infos.append(symbol_info)
  return symbol_infos


def SymbolInfosFromBinary(binary_filename):
  """Runs nm to get all the symbols from a binary.

  Args:
    binary_filename: path to the binary.

  Returns:
    A list of SymbolInfo from the binary.
  """
  command = (_NM_BINARY, '-S', '-n', binary_filename)
  p = subprocess.Popen(command, shell=False, stdout=subprocess.PIPE)
  try:
    result = SymbolInfosFromStream(p.stdout)
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
