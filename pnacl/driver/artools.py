#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# Tool for reading archive (.a) files

# TODO(pdox): Refactor driver_tools so that there is no circular dependency.
import driver_tools

AR_MAGIC = '!<arch>\n'

def IsArchive(filename):
  fp = driver_tools.DriverOpen(filename, "rb")
  magic = fp.read(8)
  fp.close()
  return magic == AR_MAGIC

def GetArchiveType(filename):
  fp = driver_tools.DriverOpen(filename, "rb")

  # Read the archive magic header
  magic = fp.read(8)
  assert(magic == AR_MAGIC)

  # Find a regular file or symbol table
  found_type = ''
  while not found_type:
    member = MemberHeader(fp)
    if member.error == 'EOF':
      break
    elif member.error:
      driver_tools.Log.Fatal("%s: %s", filename, member.error)

    data = fp.read(member.size)
    if member.is_regular_file:
      if data.startswith('BC'):
        found_type = 'archive-bc'
      else:
        elf_header = driver_tools.DecodeELFHeader(data, filename)
        if elf_header:
          found_type = 'archive-%s' % elf_header.arch

  if not found_type:
    driver_tools.Log.Fatal("%s: Unable to determine archive type", filename)

  fp.close()
  return found_type


# For information about the .ar file format, see:
#   http://en.wikipedia.org/wiki/Ar_(Unix)
class MemberHeader(object):
  def __init__(self, fp):
    self.error = ''
    header = fp.read(60)
    if len(header) == 0:
      self.error = "EOF"
      return

    if len(header) != 60:
      self.error = 'Short count reading archive member header';
      return

    self.name = header[0:16]
    self.size = header[48:48 + 10]
    self.fmag = header[58:60]

    if self.fmag != '`\n':
      self.error = 'Invalid archive member header magic string'
      return

    self.size = int(self.size)

    self.is_svr4_symtab = (self.name == '/               ')
    self.is_llvm_symtab = (self.name == '#_LLVM_SYM_TAB_#')
    self.is_bsd4_symtab = (self.name == '__.SYMDEF SORTED')
    self.is_strtab      = (self.name == '//              ')
    self.is_regular_file = not (self.is_svr4_symtab or
                                self.is_llvm_symtab or
                                self.is_bsd4_symtab or
                                self.is_strtab)

    # BSD style long names (not supported)
    if self.name.startswith('#1/'):
      self.error = "BSD-style long file names not supported"
      return

    # If it's a GNU long filename, note this, but don't actually
    # look it up. (we don't need it)
    self.is_long_name = (self.is_regular_file and self.name.startswith('/'))

    if self.is_regular_file and not self.is_long_name:
      # Filenames end with '/' and are padded with spaces up to 16 bytes
      self.name = self.name.strip()[:-1]
