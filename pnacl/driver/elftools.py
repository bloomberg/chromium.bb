#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tools for parsing ELF headers.

from driver_log import DriverOpen, DriverClose, Log, FixArch

class ELFHeader(object):
  ELF_MAGIC = '\x7fELF'
  ELF_TYPES = { 1: 'REL',  # .o
                2: 'EXEC', # .exe
                3: 'DYN' } # .so
  ELF_MACHINES = {  3: '386',
                    8: 'MIPS',
                   40: 'ARM',
                   62: 'X86_64' }
  ELF_OSABI = { 0: 'UNIX',
                3: 'LINUX',
                123: 'NACL' }
  ELF_ABI_VER = { 0: 'NONE',
                  7: 'NACL' }

  def __init__(self, e_type, e_machine, e_osabi, e_abiver):
    self.type = self.ELF_TYPES[e_type]
    self.machine = self.ELF_MACHINES[e_machine]
    self.osabi = self.ELF_OSABI[e_osabi]
    self.abiver = self.ELF_ABI_VER[e_abiver]
    self.arch = FixArch(self.machine)  # For convenience

# If the file is not ELF, returns None.
# Otherwise, returns an ELFHeader object.
def GetELFHeader(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(16 + 2 + 2)
  DriverClose(fp)
  return DecodeELFHeader(header, filename)

def DecodeELFHeader(header, filename):
  # Pull e_ident, e_type, e_machine
  if header[0:4] != ELFHeader.ELF_MAGIC:
    return None

  e_osabi = DecodeLE(header[7])
  e_abiver = DecodeLE(header[8])
  e_type = DecodeLE(header[16:18])
  e_machine = DecodeLE(header[18:20])

  if e_osabi not in ELFHeader.ELF_OSABI:
    Log.Fatal('%s: ELF file has unknown OS ABI (%d)', filename, e_osabi)
  if e_abiver not in ELFHeader.ELF_ABI_VER:
    Log.Fatal('%s: ELF file has unknown ABI version (%d)', filename, e_abiver)
  if e_type not in ELFHeader.ELF_TYPES:
    Log.Fatal('%s: ELF file has unknown type (%d)', filename, e_type)
  if e_machine not in ELFHeader.ELF_MACHINES:
    Log.Fatal('%s: ELF file has unknown machine type (%d)', filename, e_machine)

  eh = ELFHeader(e_type, e_machine, e_osabi, e_abiver)
  return eh

# filetype.IsELF calls this IsElf. Top-level tools should prefer filetype.IsELF,
# both for consistency (i.e., all checks for file type come from that library),
# and because its results are cached.
def IsELF(filename):
  return GetELFHeader(filename) is not None

# Decode Little Endian bytes into an unsigned value
def DecodeLE(bytes):
  value = 0
  for b in reversed(bytes):
    value *= 2
    value += ord(b)
  return value
