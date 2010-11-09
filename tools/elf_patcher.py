#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import getopt
import struct
import sys

import elf
import nacl_elf_constants

def StringSubst(in_str, pos, replacement):
  return in_str[:pos] + replacement + in_str[pos+1:]

def UpdateFile(fname, align, constants):
  data = open(fname).read()

  elf_obj = elf.Elf(data)

  format_map = elf.ehdr_format_map[elf_obj.wordsize]

  ehdr_data = list(struct.unpack_from(format_map, data))

  ehdr_data[0] = StringSubst(ehdr_data[0],
                             elf.ehdr_ident['osabi'],
                             chr(constants['ELFOSABI_NACL']))
  ehdr_data[0] = StringSubst(ehdr_data[0],
                             elf.ehdr_ident['osabi'],
                             chr(constants['ELFOSABI_NACL']))
  ehdr_data[0] = StringSubst(ehdr_data[0],
                             elf.ehdr_ident['abiversion'],
                             chr(constants['EF_NACL_ABIVERSION']))
  flag_pos = elf.MemberPositionMap(elf.ehdr_member_and_type)['flags']
  ehdr_data[flag_pos] = ehdr_data[flag_pos] & ~constants['EF_NACL_ALIGN_MASK']
  ehdr_data[flag_pos] = ehdr_data[flag_pos] | constants[
      'EF_NACL_ALIGN_' + align]
  new_hdr = struct.pack(format_map, *ehdr_data)
  open(fname, 'w').write(
      new_hdr + data[len(new_hdr):])

def main(argv):
  try:
    (opts, args) = getopt.getopt(argv[1:], 'a:h:')
  except getopt.error, e:
    print >>sys.stderr, str(e)
    print >>sys.stderr, ('Usage: elf_patcher [-h /path/to/nacl_elf.h]' +
                         ' filename...')
    return 1
  # endtry

  constants = None
  valid_aligns = ['16', '32', 'LIB']

  align = '32'

  for opt, val in opts:
    if opt == '-a':
      align = val
    elif opt == '-h':
      constants = nacl_elf_constants.GetNaClElfConstants(val)
    # endif
  # endfor

  if constants is None:
    print >>sys.stderr, 'Current NaCl OS, ABI, align mask values unknown'
    return 1

  if align not in valid_aligns:
    print >>sys.stderr, ('bundle size / alignment must be one of %s' %
                         ', '.join(valid_aligns))
    return 2

  for filename in args:
    UpdateFile(filename, align, constants)
  # endfor

  return 0
# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
