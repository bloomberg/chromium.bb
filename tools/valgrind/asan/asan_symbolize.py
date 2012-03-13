#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import string
import subprocess

pipes = {}
vmaddrs = {}

# TODO(glider): need some refactoring here
def symbolize_addr2line(line):
  #0 0x7f6e35cf2e45  (/blah/foo.so+0x11fe45)
  match = re.match('^( *#[0-9]+ *0x[0-9a-f]+) *\((.*)\+(0x[0-9a-f]+)\)', line)
  if match:
    binary = match.group(2)
    addr = match.group(3)
    if not pipes.has_key(binary):
      pipes[binary] = subprocess.Popen(["addr2line", "-f", "-e", binary],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p = pipes[binary]
    try:
      print >>p.stdin, addr
      function_name = p.stdout.readline().rstrip()
      file_name     = p.stdout.readline().rstrip()
    except:
      function_name = ""
      file_name = ""
    for path_to_cut in sys.argv[1:]:
      file_name = re.sub(".*" + path_to_cut, "", file_name)
    file_name = re.sub(".*asan_rtl.cc:[0-9]*", "_asan_rtl_", file_name)
    file_name = re.sub(".*crtstuff.c:0", "???:0", file_name)

    print match.group(1), "in", function_name, file_name
  else:
    print line.rstrip()

def fix_binary_for_chromium(binary):
  BUILDTYPE = 'Release'
  # Chromium Helper EH is the same binary as Chromium Helper, but there's no
  # .dSYM directory for it.
  binary = binary.replace('Chromium Helper EH', 'Chromium Helper')
  pieces = binary.split(BUILDTYPE)
  if len(pieces) != 2:
    return binary
  prefix, suffix = pieces
  to_fix = { 'Chromium Framework': 'framework',
             'Chromium Helper': 'app' }
  for filename in to_fix:
    ext = to_fix[filename]
    if suffix.endswith(filename):
      binary = (prefix + BUILDTYPE + '/' + filename + '.' +
                ext + '.dSYM/Contents/Resources/DWARF/' + filename)
      return binary
  filename = suffix.split('/')[-1]
  binary = (prefix + BUILDTYPE + '/' + filename +
            '.dSYM/Contents/Resources/DWARF/' + filename)
  return binary


# Get the skew value to be added to the address.
# We're ooking for the following piece in otool -l output:
#   Load command 0
#   cmd LC_SEGMENT
#   cmdsize 736
#   segname __TEXT
#   vmaddr 0x00000000
def get_binary_vmaddr(binary):
  if binary in vmaddrs:
    return vmaddrs[binary]
  cmdline = ["otool", "-l", binary]
  pipe = subprocess.Popen(cmdline,
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE)
  is_text = False
  vmaddr = 0
  for line in pipe.stdout.readlines():
    line = line.strip()
    if line.startswith('segname'):
      is_text = (line == 'segname __TEXT')
      continue
    if line.startswith('vmaddr') and is_text:
      sv = line.split(' ')
      vmaddr = int(sv[-1], 16)
      break
  vmaddrs[binary] = vmaddr
  return vmaddr


def write_addr_to_pipe(pipe, addr, offset, binary):
  skew = get_binary_vmaddr(binary)
  print >>pipe, "0x%x" % (int(offset, 16) + skew)

def open_atos(binary):
  #print "atos -o %s -l %s" % (binary, hex(load_addr))
  cmdline = ["atos", "-o", binary]
  return subprocess.Popen(cmdline,
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE)

def symbolize_atos(line):
  #0 0x7f6e35cf2e45  (/blah/foo.so+0x11fe45)
  match = re.match('^( *#[0-9]+ *)(0x[0-9a-f]+) *\((.*)\+(0x[0-9a-f]+)\)', line)
  if match:
    #print line
    prefix = match.group(1)
    addr = match.group(2)
    binary = match.group(3)
    offset = match.group(4)
    load_addr = int(addr, 16) - int(offset, 16)
    original_binary = binary
    binary = fix_binary_for_chromium(binary)
    if not pipes.has_key(binary):
      pipes[binary] = open_atos(binary)
    p = pipes[binary]
    try:
      write_addr_to_pipe(p.stdin, addr, offset, binary)
    except:
      # Oops, atos failed. Trying to use the original binary.
      binary = original_binary
      pipes[binary] = open_atos(binary)
      p = pipes[binary]
      write_addr_to_pipe(p.stdin, addr, offset, binary)

    # TODO(glider): it's more efficient to make a batch atos run for each
    # binary.
    p.stdin.close()
    atos_line = p.stdout.readline().rstrip()
    del pipes[binary]

    print "%s%s in %s" % (prefix, addr, atos_line)
  else:
    print line.rstrip()

def main():
  system = os.uname()[0]
  if system in ['Linux', 'Darwin']:
    for line in sys.stdin:
      if system == 'Linux':
        symbolize_addr2line(line)
      elif system == 'Darwin':
        symbolize_atos(line)
  else:
    print 'Unknown system: ', system

if __name__ == '__main__':
  main()
