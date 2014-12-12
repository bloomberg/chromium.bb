#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Symbolize log file produced by cypgofile instrumentation.

Given a log file and the binary being profiled (e.g. executable, shared
library), the script can produce three different outputs: 1) symbols for the
addresses, 2) function and line numbers for the addresses, or 3) an order file.
"""

import optparse
import os
import string
import subprocess
import sys


def ParseLogLines(log_file_lines):
  """Parse a log file produced by the profiled run of clank.

  Args:
    log_file_lines: array of lines in log file produced by profiled run
    lib_name: library or executable containing symbols

    Below is an example of a small log file:
    5086e000-52e92000 r-xp 00000000 b3:02 51276      libchromeview.so
    secs       usecs      pid:threadid    func
    START
    1314897086 795828     3587:1074648168 0x509e105c
    1314897086 795874     3587:1074648168 0x509e0eb4
    1314897086 796326     3587:1074648168 0x509e0e3c
    1314897086 796552     3587:1074648168 0x509e07bc
    END

  Returns:
    call_info list with list of tuples of the format (sec, usec, call id,
    function address called)
  """
  call_lines = []
  vm_start = 0
  line = log_file_lines[0]
  assert("r-xp" in line)
  end_index = line.find('-')
  vm_start = int(line[:end_index], 16)
  for line in log_file_lines[2:]:
  # print hex(vm_start)
    fields = line.split()
    if len(fields) == 4:
      call_lines.append(fields)

  # Convert strings to int in fields.
  call_info = []
  for call_line in call_lines:
    (sec_timestamp, usec_timestamp) = map(int, call_line[0:2])
    callee_id = call_line[2]
    addr = int(call_line[3], 16)
    if vm_start < addr:
      addr -= vm_start
      call_info.append((sec_timestamp, usec_timestamp, callee_id, addr))

  return call_info

def GetStdOutputLines(cmd):
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  output = p.communicate()[0]
  return output.split('\n')

def ParseLibSymbols(lib_file):
  """Get output from running nm and greping for text symbols.

  Args:
    lib_file: the library or executable that contains the profiled code

  Returns:
    list of sorted unique addresses and corresponding size of function symbols
    in lib_file and map of addresses to all symbols at a particular address
  """
  cmd = ['nm', '-S', '-n', lib_file]
  nm_lines = GetStdOutputLines(cmd)

  nm_symbols = []
  for nm_line in nm_lines:
    if any(str in nm_line for str in (' t ', ' W ', ' T ')):
      nm_symbols.append(nm_line)

  nm_index = 0
  unique_addrs = []
  address_map = {}
  while nm_index < len(nm_symbols):

    # If the length of the split line is not 4, then it does not contain all the
    # information needed to symbolize (i.e. address, size and symbol name).
    if len(nm_symbols[nm_index].split()) == 4:
      (addr, size) = [int(x, 16) for x in nm_symbols[nm_index].split()[0:2]]

      # Multiple symbols may be at the same address.  This is do to aliasing
      # done by the compiler.  Since there is no way to be sure which one was
      # called in profiled run, we will symbolize to include all symbol names at
      # a particular address.
      fnames = []
      while (nm_index < len(nm_symbols) and
             addr == int(nm_symbols[nm_index].split()[0], 16)):
        if len(nm_symbols[nm_index].split()) == 4:
          fnames.append(nm_symbols[nm_index].split()[3])
        nm_index += 1
      address_map[addr] = fnames
      unique_addrs.append((addr, size))
    else:
      nm_index += 1

  return (unique_addrs, address_map)

class SymbolNotFoundException(Exception):
  def __init__(self,value):
    super(SymbolNotFoundException,self).__init__(value)
    self.value = value
  def __str__(self):
    return repr(self.value)

def BinarySearchAddresses(addr, start, end, arr):
  """Find starting address of a symbol at a particular address.

  The reason we can not directly use the address provided by the log file is
  that the log file may give an address after the start of the symbol.  The
  logged address is often one byte after the start.  By using this search
  function rather than just subtracting one from the logged address allows
  the logging instrumentation to log any address in a function.

  Args:
    addr: the address being searched for
    start: the starting index for the binary search
    end: the ending index for the binary search
    arr: the list being searched containing tuple of address and size

  Returns:
    the starting address of the symbol at address addr

  Raises:
    Exception: if address not found.  Functions expects all logged addresses
    to be found
  """
  # print "addr: " + str(addr) + " start: " + str(start) + " end: " + str(end)
  if start >= end or start == end - 1:
    # arr[i] is a tuple of address and size.  Check if addr inside range
    if addr >= arr[start][0] and addr < arr[start][0] + arr[start][1]:
      return arr[start][0]
    elif addr >= arr[end][0] and addr < arr[end][0] + arr[end][1]:
      return arr[end][0]
    else:
      raise SymbolNotFoundException(addr)
  else:
    halfway = (start + end) / 2
    (nm_addr, size) = arr[halfway]
    # print "nm_addr: " + str(nm_addr) + " halfway: " + str(halfway)
    if addr >= nm_addr and addr < nm_addr + size:
      return nm_addr
    elif addr < nm_addr:
      return BinarySearchAddresses(addr, start, halfway-1, arr)
    else:
      # Condition (addr >= nm_addr + size) must be true.
      return BinarySearchAddresses(addr, halfway+1, end, arr)


def FindFunctions(addr, unique_addrs, address_map):
  """Find function symbol names at address addr."""
  return address_map[BinarySearchAddresses(addr, 0, len(unique_addrs) - 1,
                                           unique_addrs)]


def AddrToLine(addr, lib_file):
  """Use addr2line to determine line info of a particular address."""
  cmd = ['addr2line', '-f', '-e', lib_file, hex(addr)]
  output = GetStdOutputLines(cmd)
  assert(len(output) == 2)
  return ':'.join(output)

def GetObjectFileNames(obj_dir):
  """ Gets the list of object files in the output directory. """
  obj_files = []
  for (dirpath, _, filenames) in os.walk(obj_dir):
    for file_name in filenames:
      if file_name.endswith('.o'):
        obj_files.append(os.path.join(dirpath, file_name))
  return obj_files

class WarningCollector(object):
  def __init__(self, max_warnings):
    self._warnings = 0
    self._max_warnings = max_warnings

  def Write(self, message):
    if self._warnings < self._max_warnings:
      sys.stderr.write(message + '\n')
    self._warnings += 1

  def WriteEnd(self, message):
    if self._warnings > self._max_warnings:
      sys.stderr.write(str(self._warnings - self._max_warnings) +
                       ' more warnings for: ' + message + '\n')

def SymbolToSection(obj_dir):
  """ Gets a mapping from symbol to linker section name by scanning all
      of the object files. """
  object_files = GetObjectFileNames(obj_dir)
  symbol_to_section_map = {}
  symbol_warnings = WarningCollector(300)
  for obj_file in object_files:
    cmd = ['objdump', '-w', '-t', obj_file]
    symbol_lines = GetStdOutputLines(cmd)
    for symbol_line in symbol_lines:
      items = symbol_line.split()
      # All of the symbol lines we care about are in the form
      # 0000000000  g    F   .text.foo     000000000 [.hidden] foo
      # where g (global) might also be l (local) or w (weak).
      if len(items) > 4 and items[2] == 'F':
        # This symbol is a function
        symbol = items[len(items) - 1]
        if symbol.startswith('.LTHUNK'):
          continue
        section = items[3]
        if ((symbol in symbol_to_section_map) and
            (symbol_to_section_map[symbol] != section)):
          symbol_warnings.Write('WARNING: Symbol ' + symbol +
                                ' in conflicting sections ' + section +
                                ' and ' + symbol_to_section_map[symbol])
        elif not section.startswith('.text.'):
          symbol_warnings.Write('WARNING: Symbol ' + symbol +
                                ' in incorrect section ' + section)
        else:
          symbol_to_section_map[symbol] = section
  symbol_warnings.WriteEnd('bad sections')
  return symbol_to_section_map

def main():
  """Write output for profiled run to standard out.

  The format of the output depends on the output type specified as the third
  command line argument.  The default output type is to symbolize the addresses
  of the functions called.
  """
  parser = optparse.OptionParser('usage: %prog [options] log_file lib_file')
  parser.add_option('-t', '--outputType', dest='output_type',
                    default='symbolize', type='string',
                    help='lineize or symbolize or orderfile')

  # Option for output type.  The log file and lib file arguments are required
  # by the script and therefore are not options.
  (options, args) = parser.parse_args()
  if len(args) != 2:
    parser.error('expected 2 args: log_file lib_file')

  (log_file, lib_file) = args
  output_type = options.output_type

  obj_dir = os.path.abspath(os.path.join(os.path.dirname(lib_file), '../obj'))

  log_file_lines = map(string.rstrip, open(log_file).readlines())
  call_info = ParseLogLines(log_file_lines)
  (unique_addrs, address_map) = ParseLibSymbols(lib_file)

  # Check for duplicate addresses in the log file, and print a warning if
  # duplicates are found. The instrumentation that produces the log file
  # should only print the first time a function is entered.
  addr_list = []
  for call in call_info:
    addr = call[3]
    if addr not in addr_list:
      addr_list.append(addr)
    else:
      print('WARNING: Address ' + hex(addr) + ' (line= ' +
            AddrToLine(addr, lib_file) + ') already profiled.')

  symbol_to_section_map = SymbolToSection(obj_dir)

  unknown_symbol_warnings = WarningCollector(300)
  symbol_not_found_warnings = WarningCollector(300)
  for call in call_info:
    addr = call[3]
    if output_type == 'lineize':
      symbol = AddrToLine(addr, lib_file)
      print(str(call[0]) + ' ' + str(call[1]) + '\t' + str(call[2]) + '\t'
            + symbol)
    elif output_type == 'orderfile':
      try:
        symbols = FindFunctions(addr, unique_addrs, address_map)
        for symbol in symbols:
          if symbol in symbol_to_section_map:
            print symbol_to_section_map[symbol]
          else:
            unknown_symbol_warnings.Write(
                'WARNING: No known section for symbol ' + symbol)
        print ''
      except SymbolNotFoundException:
        symbol_not_found_warnings.Write(
            'WARNING: Did not find function in binary. addr: '
            + hex(addr))
    else:
      try:
        symbols = FindFunctions(addr, unique_addrs, address_map)
        print(str(call[0]) + ' ' + str(call[1]) + '\t' + str(call[2]) + '\t'
              + symbols[0])
        first_symbol = True
        for symbol in symbols:
          if not first_symbol:
            print '\t\t\t\t\t' + symbol
          else:
            first_symbol = False
      except SymbolNotFoundException:
        symbol_not_found_warnings.Write(
            'WARNING: Did not find function in binary. addr: '
            + hex(addr))
  unknown_symbol_warnings.WriteEnd('no known section for symbol')
  symbol_not_found_warnings.WriteEnd('did not find function')

if __name__ == '__main__':
  main()
