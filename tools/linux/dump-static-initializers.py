#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dump functions called by static intializers in a Linux Release binary.

Usage example:
  tools/linux/dump-static-intializers.py out/Release/chrome

A brief overview of static initialization:
1) the compiler writes out, per object file, a function that contains
   the static intializers for that file.
2) the compiler also writes out a pointer to that function in a special
   section.
3) at link time, the linker concatenates the function pointer sections
   into a single list of all initializers.
4) at run time, on startup the binary runs all function pointers.

The functions in (1) all have mangled names of the form
  _GLOBAL__I_foobar.cc
using objdump, we can disassemble those functions and dump all symbols that
they reference.
"""

import optparse
import re
import subprocess
import sys

# A map of symbol => informative text about it.
NOTES = {
  '__cxa_atexit@plt': 'registers a dtor to run at exit',
  'std::__ioinit': '#includes <iostream>, use <ostream> instead',
}

class Demangler(object):
  """A wrapper around c++filt to provide a function to demangle symbols."""
  def __init__(self):
    self.cppfilt = subprocess.Popen(['c++filt'],
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE)

  def Demangle(self, sym):
    """Given mangled symbol |sym|, return its demangled form."""
    self.cppfilt.stdin.write(sym + '\n')
    return self.cppfilt.stdout.readline().strip()


# Regex matching nm output for the symbols we're interested in.
# Example line:
#   0000000001919920 0000000000000008 b _ZN12_GLOBAL__N_119g_nine_box_prelightE
nm_re = re.compile(r'(\S+) (\S+) t _GLOBAL__I_(.*)')
def ParseNm(binary):
  """Given a binary, yield static initializers as (start, size, file) pairs."""

  nm = subprocess.Popen(['nm', '-S', binary], stdout=subprocess.PIPE)
  for line in nm.stdout:
    match = nm_re.match(line)
    if match:
      addr, size, filename = match.groups()
      yield int(addr, 16), int(size, 16), filename


# Regex matching objdump output for the symbols we're interested in.
# Example line:
#     12354ab:  (disassembly, including <FunctionReference>)
disassembly_re = re.compile(r'^\s+[0-9a-f]+:.*<(\S+)>')
def ExtractSymbolReferences(binary, start, end):
  """Given a span of addresses, yields symbol references from disassembly."""
  cmd = ['objdump', binary, '--disassemble',
         '--start-address=0x%x' % start, '--stop-address=0x%x' % end]
  objdump = subprocess.Popen(cmd, stdout=subprocess.PIPE)

  refs = set()
  for line in objdump.stdout:
    if '__static_initialization_and_destruction' in line:
      raise RuntimeError, ('code mentions '
                           '__static_initialization_and_destruction; '
                           'did you accidentally run this on a Debug binary?')
    match = disassembly_re.search(line)
    if match:
      (ref,) = match.groups()
      if ref.startswith('.LC') or ref.startswith('_DYNAMIC'):
        # Ignore these, they are uninformative.
        continue
      if ref.startswith('_GLOBAL__I_'):
        # Probably a relative jump within this function.
        continue
      refs.add(ref)
      continue

  for ref in sorted(refs):
    yield ref


def main():
  parser = optparse.OptionParser(usage='%prog filename')
  parser.add_option('-i', '--instances', dest='calculate_instances',
                    action='store_true', default=False,
                    help='Only print out the number of static initializers')
  opts, args = parser.parse_args()
  if len(args) != 1:
    parser.error('missing filename argument')
    return 1
  binary = args[0]

  demangler = Demangler()
  static_initializers_count = 0
  for addr, size, filename in ParseNm(binary):
    if size == 2:
      # gcc generates a two-byte 'repz retq' initializer when there is nothing
      # to do.  jyasskin tells me this is fixed in gcc 4.6.
      # Two bytes is too small to do anything, so just ignore it.
      continue

    if (opts.calculate_instances):
      static_initializers_count += 1
      continue

    print '%s (initializer offset 0x%x size 0x%x)' % (filename, addr, size)
    for ref in ExtractSymbolReferences(binary, addr, addr+size):
      ref = demangler.Demangle(ref)
      if ref in NOTES:
        print ' ', '%s [%s]' % (ref, NOTES[ref])
      else:
        print ' ', ref
    print

  if opts.calculate_instances:
    print static_initializers_count
  return 0


if '__main__' == __name__:
  sys.exit(main())
