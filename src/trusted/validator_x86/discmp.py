#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
"""discmp.py - compare disassembler output

This script compares output from the Native Client decoder to output
from GNU objdump to make sure they agree on instruction lengths. It
returns a zero status on success and non-zero on failure.


Usage:
  discmp.py <ncdis-exe> <test-binary>*
"""

import logging
import os
import re
import sys


def Usage():
  print("USAGE:  discmp.py <ncdis-exe>  <test-binary>*\n")
  sys.exit(-1)

class DisFile(object):
  """Processes variants of disassembler output from various sources,
   providing a list of instruction lengths, based on addresses."""

  def __init__(self, stream):
    self.fd = stream
    self.lastline = None
    self.lastaddr = None
    self.thisaddr = None
    self.thisline = None
    # note we ignore lines that have only continuation bytes;
    # no opcode
    self.dislinefmt = re.compile("\s*([0-9a-f]+):\t(.*\t+.*)")
    self.data16fmt = re.compile("\s*([0-9a-f]+):\s+66\s+data16")
    self.waitfmt = re.compile("\s*([0-9a-f]+):\s+9b\s+[f]?wait")

  def NextDisInst(self):
    while (True):
      line = self.fd.readline()
      if line == "": return 0, None
      match = self.data16fmt.match(line)
      if (match):
        addr = match.group(1)
        line = self.fd.readline()
        match = self.dislinefmt.match(line)
        return (int(addr, 16),
                " " + addr + ":\t60 " + match.group(2) + "\n")
      match = self.waitfmt.match(line)
      if (match):
        addr = match.group(1)
        line = self.fd.readline()
        match = self.dislinefmt.match(line)
        return (int(addr, 16),
                " " + addr + ":\t9b " + match.group(2) + "\n")
      match = self.dislinefmt.match(line)
      if (match):
        return int(match.group(1), 16), line

  def NextInst(self):
    if self.lastaddr is None:
      self.lastaddr, self.lastline = self.NextDisInst()
    else:
      self.lastaddr = self.thisaddr
      self.lastline = self.thisline
    self.thisaddr, self.thisline = self.NextDisInst()
    if self.thisline is None:
      # don't know how long the last line was, so just return 1
      return (1, self.lastline)
    if self.lastline is None:
      return 0, None
    else:
      return (self.thisaddr - self.lastaddr, self.lastline)


def IsElfBinary(fname):
  fd = open(fname)
  iselfbinary = fd.readline().startswith("\x7fELF")
  fd.close()
  return iselfbinary


def CheckOneFile(fname, ncdis, objdump):
  logging.info("processing %s", fname)
  if not IsElfBinary(fname):
    print("Error:", fname, "is not an ELF binary\nContinuing...")
    return
  df1 = DisFile(os.popen(objdump + " -dz " + fname))
  df2 = DisFile(os.popen(ncdis + " " + fname))
  instcount = 0
  while (1):
    instcount += 1
    len1, line1 = df1.NextInst()
    len2, line2 = df2.NextInst()
    if line1 is None: break
    if line2 is None: break
    if (len1 != len2):
      logging.error("inst length mistmatch %d != %d\n", len1, len2)
      logging.error(line1)
      logging.error(line2)
      sys.exit(-1)
  if line1 or line2:
    logging.error("disasm output is different lengths\n")
    sys.exit(-1)
  logging.info("%s: %d instructions; 0 errors!\n", fname, instcount)


def main(argv):
  logging.basicConfig(level=logging.DEBUG,
                      stream=sys.stdout)
  ncdis = argv[1]
  logging.info("using ncdis at: %s", ncdis)
  objdump="objdump"
  logging.info("using objdump at: %s", objdump)
  for filename in argv[2:]:
    if os.path.isdir(filename):
      logging.info("traversing dir: %s", filename)
      for dirpath, dirlist, filelist in os.walk(filename):
        for fname in filelist:
          CheckOneFile(os.path.join(dirpath, fname), ncdis, objdump)
    else:
      CheckOneFile(filename, ncdis, objdump)
  return 0

if '__main__' == __name__:
  sys.exit(main(sys.argv))
