#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Patches redundant calls to function instrumentation with NOPs."""

import argparse
import copy
import logging
import os
import re
import struct
import subprocess
import sys

# Python has a symbol builtin module, so the android one needs to be first in
# the import path.
_SRC_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
sys.path.insert(0, os.path.join(
    _SRC_PATH, 'third_party', 'android_platform', 'development', 'scripts'))
import symbol

_OBJDUMP = symbol.ToolPath('objdump')
_STRIP = symbol.ToolPath('strip')


class SymbolData(object):
  """Data about a symbol, extracted from objdump output."""

  SYMBOL_RE = re.compile('^([0-9a-f]{8}) <(.*)>:$')
  assert SYMBOL_RE.match('002dcc84 <_ZN3net8QuicTime5Delta11FromSecondsEx>:')
  _BLX_RE = re.compile('^ {1,2}([0-9a-f]{6,7}):.*blx\t[0-9a-f]{6,7} <(.*)>')
  assert _BLX_RE.match(
      '  2dd03e:  f3f3 ee16       '
      'blx\t6d0c6c <_ZN16content_settings14PolicyProvider27UpdateManaged'
      'DefaultSettingERKNS0_30PrefsForManagedDefaultMapEntryE+0x120>')
  _BL_ENTER_RE = re.compile('^ {1,2}([0-9a-f]{6,7}):.*bl\t[0-9a-f]{6,7} '
                            '<__cyg_profile_func_enter>')
  _BL_EXIT_RE = re.compile('^ {1,2}([0-9a-f]{6,7}):.*bl\t[0-9a-f]{6,7} '
                           '<__cyg_profile_func_exit>')
  assert(_BL_ENTER_RE.match('  1a66d84:  f3ff d766       bl\t'
                            '2a66c54 <__cyg_profile_func_enter>'))

  def __init__(self, name, offset, bl_enter, bl_exit, blx):
    """Constructor.

    Args:
      name: (str) Mangled symbol name.
      offset: (int) Offset into the native library
      bl_enter: ([int]) List of offsets at which short jumps to the enter
                instrumentation are found.
      bl_exit: ([int]) List of offsets at which short jumps to the exit
                instrumentation are found.
      blx: ([(int, str)]) (instruction_offset, target_offset) for long jumps.
           The target is encoded, for instance _ZNSt6__ndk14ceilEf+0x28.
    """
    self.name = name
    self.offset = offset
    self.bl_enter = bl_enter
    self.bl_exit = bl_exit
    self.blx = blx

  @classmethod
  def FromObjdumpLines(cls, lines):
    """Returns an instance of SymbolData from objdump lines.

    Args:
      lines: ([str]) Symbol disassembly.
    """
    offset, name = cls.SYMBOL_RE.match(lines[0]).groups()
    symbol_data = cls(name, int(offset, 16), [], [], [])
    for line in lines[1:]:
      if cls._BL_ENTER_RE.match(line):
        offset = int(cls._BL_ENTER_RE.match(line).group(1), 16)
        symbol_data.bl_enter.append(offset)
      elif cls._BL_EXIT_RE.match(line):
        offset = int(cls._BL_EXIT_RE.match(line).group(1), 16)
        symbol_data.bl_exit.append(offset)
      elif cls._BLX_RE.match(line):
        offset, name = cls._BLX_RE.match(line).groups()
        offset = int(offset, 16)
        symbol_data.blx.append((offset, name))
    return symbol_data


def RunObjdumpAndParse(native_library_filename):
  """Calls Objdump and parses its output.

  Args:
    native_library_filename: (str) Path to the natve library.

  Returns:
    [SymbolData]
  """
  p = subprocess.Popen([_OBJDUMP, '-d', native_library_filename, '-j', '.text'],
                       stdout=subprocess.PIPE, bufsize=-1)
  result = []
  # Skip initial blank lines.
  while not p.stdout.readline().startswith('Disassembly of section .text'):
    continue

  next_line = p.stdout.readline()
  while True:
    if not next_line:
      break
    # skip to new symbol
    while not SymbolData.SYMBOL_RE.match(next_line):
      next_line = p.stdout.readline()

    symbol_lines = [next_line]
    next_line = p.stdout.readline()
    while next_line.strip():
      symbol_lines.append(next_line)
      next_line = p.stdout.readline()
    result.append(SymbolData.FromObjdumpLines(symbol_lines))
  # EOF
  p.wait()
  return result


def ResolveBlxTargets(symbols_data, native_lib_filename):
  """Parses the binary, and resolves all targets of long jumps.

  Args:
    symbols_data: ([SymbolData]) As returned by RunObjdumpAndParse().
    native_lib_filename: (str) Path to the unstripped native library.

  Returns:
    {"blx_target_name": "actual jump target symbol name"}
  """
  blx_targets = set()
  blx_target_to_offset = {}
  for symbol_data in symbols_data:
    for (_, target) in symbol_data.blx:
      blx_targets.add(target)
  logging.info('Found %d distinct BLX targets', len(blx_targets))
  name_to_offset = {s.name: s.offset for s in symbols_data}
  offset_to_name = {s.offset: s.name for s in symbols_data}
  unmatched_count = 0
  for target in blx_targets:
    if '+' not in target:
      continue
    # FunkySymbolName+0x12bc
    name, offset = target.split('+')
    if name not in name_to_offset:
      unmatched_count += 1
      continue
    offset = int(offset, 16)
    absolute_offset = name_to_offset[name] + offset
    blx_target_to_offset[target] = absolute_offset
  logging.info('Unmatched BLX offsets: %d', unmatched_count)

  logging.info('Reading the native library')
  content = bytearray(open(native_lib_filename, 'rb').read())

  # Expected instructions are:
  # ldr r12, [pc, #4] # Here + 12
  # add r12, pc, r12
  # bx r12
  # Some offset. (4 bytes)
  # Note that the first instructions loads from pc + 8 + 4, per ARM
  # documentation. See
  # http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0473e/Cacdbfji.html
  # Reversed for little endian.
  ldr_r12_constant = bytearray(b'\xe5\x9f\xc0\x04'[::-1])
  unmatched_loads_count = 0
  add_r12_pc_r12 = bytearray(b'\xe0\x8f\xc0\x0c'[::-1])
  unmatched_adds_count = 0
  bx_r12 = bytearray(b'\xe1\x2f\xff\x1c'[::-1])
  unmatched_bx_count = 0
  unmatched_targets_count = 0
  blx_target_name_to_symbol = {}
  for (target_name, offset) in blx_target_to_offset.items():
    actual_bytes = content[offset:offset+4]
    if actual_bytes != ldr_r12_constant:
      unmatched_loads_count += 1
      continue
    actual_bytes = content[offset+4:offset+8]
    if actual_bytes != add_r12_pc_r12:
      unmatched_adds_count += 1
      continue
    actual_bytes = content[offset+8:offset+12]
    if actual_bytes != bx_r12:
      unmatched_bx_count += 1
      continue
    # Congratulations, you've passed all the tests. The next value must be
    # an offset.
    offset_bytearray = content[offset+12:offset+16]
    offset_from_pc = struct.unpack('<i', offset_bytearray)[0]
    # Jumping to THUMB code, last bit is set to 1 to indicate the instruction
    # set. The actual address is aligned on 4 bytes though.
    assert offset_from_pc & 1
    offset_from_pc &= ~1
    if offset_from_pc % 4:
      unmatched_targets_count += 1
      continue
    # PC points 8 bytes ahead of the ADD instruction, which is itself 4 bytes
    # ahead of the jump target. Add 8 + 4 bytes to the destination.
    target_offset = offset + offset_from_pc + 8 + 4
    if target_offset not in offset_to_name:
      unmatched_targets_count += 1
      continue
    blx_target_name_to_symbol[target_name] = offset_to_name[target_offset]
  logging.info('Unmatched instruction sequence = %d %d %d',
               unmatched_loads_count, unmatched_adds_count, unmatched_bx_count)
  logging.info('Unmatched targets = %d', unmatched_targets_count)
  return blx_target_name_to_symbol


def FindDuplicatedInstrumentationCalls(symbols_data, blx_target_to_symbol_name):
  """Finds the extra instrumentation calls.

  Besides not needing the exit instrumentation calls, each function should only
  contain one instrumentation call. However since instrumentation calls are
  inserted before inlining, some functions contain tens of them.  This function
  returns the location of the instrumentation calls except the first one, for
  all functions.

  Args:
    symbols_data: As returned by RunObjdumpAndParse().
    blx_target_to_symbol_name: ({str: str}) As returned by ResolveBlxTargets().

  Returns:
    [int] A list of offsets containing duplicated instrumentation calls.
  """
  offsets_to_patch = []
  # Instrumentation calls can be short (bl) calls, or long calls.
  # In the second case, the compiler inserts a call using blx to a location
  # containing trampoline code. The disassembler doesn't know about that, so
  # we use the resolved locations.
  for symbol_data in symbols_data:
    enter_call_offsets = copy.deepcopy(symbol_data.bl_enter)
    exit_call_offsets = copy.deepcopy(symbol_data.bl_exit)
    for (offset, target) in symbol_data.blx:
      if target not in blx_target_to_symbol_name:
        continue
      final_target = blx_target_to_symbol_name[target]
      if final_target == '__cyg_profile_func_enter':
        enter_call_offsets.append(offset)
      elif final_target == '__cyg_profile_func_exit':
        exit_call_offsets.append(offset)
    offsets_to_patch += exit_call_offsets
    # Not the first one.
    offsets_to_patch += sorted(enter_call_offsets)[1:]
  return sorted(offsets_to_patch)


def PatchBinary(filename, output_filename, offsets):
  """Inserts 4-byte NOPs inside the native library at a list of offsets.

  Args:
    filename: (str) File to patch.
    output_filename: (str) Path to the patched file.
    offsets: ([int]) List of offsets to patch in the binary.
  """
  # NOP.w is 0xf3 0xaf 0x80 0x00 for THUMB-2, but the CPU is little endian,
  # so reverse bytes (but 2 bytes at a time).
  _THUMB_2_NOP = bytearray('\xaf\xf3\x00\x80')
  content = bytearray(open(filename, 'rb').read())
  for offset in offsets:
    # TODO(lizeb): Assert that it's a BL or BLX
    content[offset:offset+4] = _THUMB_2_NOP
  open(output_filename, 'wb').write(content)


def StripLibrary(unstripped_library_filename):
  """Strips a native library.

  Args:
    unstripped_library_filename: (str) Path to the library to strip in place.
  """
  subprocess.call([_STRIP, unstripped_library_filename])


def Go(build_directory):
  unstripped_library_filename = os.path.join(build_directory, 'lib.unstripped',
                                             'libchrome.so')
  logging.info('Running objdump')
  symbols_data = RunObjdumpAndParse(unstripped_library_filename)
  logging.info('Symbols = %d', len(symbols_data))

  blx_target_to_symbol_name = ResolveBlxTargets(symbols_data,
                                                unstripped_library_filename)
  offsets = FindDuplicatedInstrumentationCalls(symbols_data,
                                               blx_target_to_symbol_name)
  logging.info('%d offsets to patch', len(offsets))
  patched_library_filename = unstripped_library_filename + '.patched'
  logging.info('Patching the library')
  PatchBinary(unstripped_library_filename, patched_library_filename, offsets)
  logging.info('Stripping the patched library')
  StripLibrary(patched_library_filename)
  stripped_library_filename = os.path.join(build_directory, 'libchrome.so')
  os.rename(patched_library_filename, stripped_library_filename)


def CreateArgumentParser():
  parser = argparse.ArgumentParser(description='Patch the native library')
  parser.add_argument('--build_directory', type=str, required=True)
  return parser


def main():
  logging.basicConfig(level=logging.INFO,
                      format='%(asctime)s %(levelname)s:%(message)s')
  parser = CreateArgumentParser()
  args = parser.parse_args()
  Go(args.build_directory)


if __name__ == '__main__':
  main()
