#!/usr/bin/python
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# this script is a replacement for llvm-gcc and llvm-g++, it detects
# automatically which role it is supposed to assume.
# The point of the script is to force compiler output through llc.
#
# TODO(robertm): newlib does currently not compile with the cortex settings

import struct
import subprocess
import sys

# this for manual debugging of this script only
VERBOSE = 0

BASE = '/usr/local/crosstool'


LLVM_GCC_ASSEMBLER_FLAGS = ['-march=armv6',
                            '-mfpu=vfp',
                            # Possible other settings:
                            #'-march=armv7',
                            #'-mcpu=cortex-a8',
                            ]

LLVM_PREFIX = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/llvm'

LLC_FLAGS_SHARED = ['-march=arm',
                    # c.f. lib/Target/ARM/ARMGenSubtarget.inc
                    '-mcpu=arm1156t2f-s',
                    #'-mcpu=cortex-a8',
                    '-mtriple=armv6-*-*eabi*',
                    #'-mtriple=armv7a-*-*eabi*',
                    '-arm-reserve-r9',
                    ]

LLC = [BASE + '/arm-none-linux-gnueabi/llvm/bin/llc', ] + LLC_FLAGS_SHARED

LLC_SFI = [BASE + '/arm-none-linux-gnueabi/llvm/bin/llc-sfi',
           # The following options might come in hand and are left
           # here as comments:
           # TODO(robertm): describe their purpose
           #     '-soft-float ',
           #     '-aeabi-calls '
           #     '-sfi-zero-mask',
           '-sfi-cp-fudge',
           # NOTE: we need a fairly high fudge factor because of
           # some vfp instructions which only have a 9bit offset
           '-sfi-cp-fudge-percent=80',
           '-sfi-store',
           '-sfi-stack',
           '-sfi-branch',
           '-sfi-data',
           '-no-inline-jumptables'
           ] + LLC_FLAGS_SHARED

OPT = [BASE + '/arm-none-linux-gnueabi/llvm/bin/opt',
       '-O3',
       '-std-compile-opts'
       ]

AS = ([BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-as', ] +
      LLVM_GCC_ASSEMBLER_FLAGS)


# Note: this works around an assembler bug that has been fixed only recently
# versions of binutils - we probably can drop this once we are switch to
# codesourcery 2009Q4
HACK_ASM = ['sed', '-e', 's/vmrs.*apsr_nzcv, fpscr/fmrx r15, fpscr/g']


def Run(a):
  if VERBOSE: print a
  ret = subprocess.call(a)
  if ret:
    print
    print "ERROR"
    print
    sys.exit(ret)



def SfiCompile(argv, out_pos, is_sfi):
  filename = argv[out_pos]

  argv[out_pos] = filename + '.bc'
  argv.append('--emit-llvm')
  Run(argv)

  OPT.append(filename + '.bc')
  OPT.append('-f')
  OPT.append('-o')
  OPT.append(filename + '.opt.bc')
  Run(OPT)

  if is_sfi:
    llc = LLC_SFI
  else:
    llc = LLC
  llc.append('-f')
  llc.append(filename + '.opt.bc')
  llc.append('-o')
  llc.append(filename + '.s')
  Run(llc)

  HACK_ASM.append('-i.orig')
  HACK_ASM.append(filename + '.s')
  Run(HACK_ASM)

  AS.append(filename + '.s')
  AS.append('-o')
  AS.append(filename )
  Run(AS)


def PatchAbiVersionIntoElfHeader(filename):
  # modeling Elf32_Ehdr
  FORMAT = '16BHH5I6H'

  fp = open(filename, 'rb')
  data = fp.read(struct.calcsize(FORMAT))
  fp.close()

  ELF_OSABI = 7
  ELF_ABIVERSION = 8
  # ELF_FLAGS = 22
  t = list(struct.unpack(FORMAT, data))
  # c.f. nacl_elf.h
  t[ELF_OSABI] = 123
  t[ELF_ABIVERSION] = 6
  # TODO(robertm): this currently breaks running images in sel_ldr
  # t[ELF_FLAGS] = t[ELF_FLAGS] | 0x100000
  data = struct.pack(FORMAT, *t)
  fp = open(filename, 'rb+')
  fp.write(data)
  fp.close()

# check whether compiler is invoked with -c but not on an .S file,
# hijack the corresponding .o file
def FindObjectFilePos(argv):
  if '-c' not in argv:
    return None

  if '-o' not in argv:
    return None

  pos = 1 + argv.index('-o')
  assert pos < len(argv)

  if not argv[pos].endswith('.o'):
    return None

  return pos


def HasAssemblerFiles(argv):
  for a in argv:
    if a.endswith('.S') or a.endswith('.s'):
      return True
  else:
    return False


def FindLinkPos(argv):
  if '-o' not in argv:
    return None

  pos = 1 + argv.index('-o')
  assert pos < len(argv)

  if not argv[pos].endswith('.nexe'):
    return None

  return pos


def GetActualLlvmBinary(argv):
  if argv[0].endswith('gcc'):
    return LLVM_PREFIX + '-gcc'
  else:
    assert argv[0].endswith('g++')
    return LLVM_PREFIX + '-g++'


def IsSfiInvocation(argv):
  return argv[0].endswith('sfigcc') or argv[0].endswith('sfig++')


def main(argv):
  obj_pos = FindObjectFilePos(argv)
  link_pos = FindLinkPos(argv)
  is_sfi = IsSfiInvocation(argv)
  # NOTE: we need to call this last, o.w. IsSfiInvocation() does not work
  argv[0] = GetActualLlvmBinary(argv)

  if link_pos:
    Run(argv)
    PatchAbiVersionIntoElfHeader(argv[link_pos])
  elif obj_pos:
    if HasAssemblerFiles(argv):
      Run(argv + LLVM_GCC_ASSEMBLER_FLAGS)
    else:
        SfiCompile(argv, obj_pos, is_sfi)
  else:
    Run(argv)

  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv))
