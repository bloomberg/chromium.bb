#!/usr/bin/python
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# this script is a replacement for llvm-gcc and llvm-g++, ar, etc
# It detects automatically which role it is supposed to assume.
# The point of the script is to redirect builds through our own tools,
# while making these tools appear like gnu tools.
#
# TODO(robertm): newlib does currently not compile with the cortex settings

import os
import struct
import subprocess
import sys

# enable this for manual debugging of this script only
VERBOSE = 0
TOLERATE_COMPILATION_OF_ASM_CODE = 1
BASE = '/usr/local/crosstool-untrusted'


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


LLVM_LINK = BASE + '/arm-none-linux-gnueabi/llvm/bin/llvm-link'

OPT = [BASE + '/arm-none-linux-gnueabi/llvm/bin/opt',
       '-O3',
       '-std-compile-opts'
       ]

AS = ([BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-as', ] +
      LLVM_GCC_ASSEMBLER_FLAGS)

AR = BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-ar'

# Note: this works around an assembler bug that has been fixed only recently
# We probably can drop this once we have switched to codesourcery 2009Q4
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

  ELF_OSABI_INDEX = 7
  ELF_ABIVERSION_INDEX = 8
  ELF_FLAGS_INDEX = 22
  t = list(struct.unpack(FORMAT, data))
  # c.f. nacl_elf.h
  t[ELF_OSABI_INDEX] = 123
  t[ELF_ABIVERSION_INDEX] = 7
  t[ELF_FLAGS_INDEX] = t[ELF_FLAGS_INDEX] | 0x100000
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
  """Determine whether we are linking a binary as opposed to an .o file."""
  if '-o' not in argv:
    return None

  pos = 1 + argv.index('-o')
  assert pos < len(argv)

  if argv[pos].endswith('.o'):
    return None

  return pos



def Link(argv, llvm_binary, is_sfi):
  """Generate a binary/executable.

  NOTE: you cannot specify source files here.
        Hence "-o foo.o" is not allowed
  """
  link_pos = FindLinkPos(argv)
  assert link_pos
  # see above
  obj_pos = FindObjectFilePos(argv)
  assert not obj_pos

  argv[0] = llvm_binary
  Run(argv)
  if is_sfi:
    PatchAbiVersionIntoElfHeader(argv[link_pos])


def IsDiagnosticMode(argv):
  return ('--v' in argv or
          '-print-search-dirs' in argv or
          '-print-libgcc-file-name' in argv)


def Compile(argv, llvm_binary, is_sfi):
  """ Compile to .o file."""
  # TODO(robertm): channel linking through separate program
  link_pos = FindLinkPos(argv)
  if link_pos:
    Link(argv, llvm_binary, is_sfi)
    return

  argv[0] = llvm_binary

  obj_pos = FindObjectFilePos(argv)

  if not obj_pos:
    assert IsDiagnosticMode(argv)
    Run(argv)
    return

  # TODO(robertm): remove support for .S files
  if HasAssemblerFiles(argv):
    assert TOLERATE_COMPILATION_OF_ASM_CODE
    Run(argv + LLVM_GCC_ASSEMBLER_FLAGS)
    return

  SfiCompile(argv, obj_pos, is_sfi)



def CompileToBitcode(argv, llvm_binary):
  """Compile to .o file which really is a bitcode files."""
  assert not FindLinkPos(argv)

  argv[0] = llvm_binary

  obj_pos = FindObjectFilePos(argv)

  if not obj_pos:
    assert IsDiagnosticMode(argv)
    Run(argv)
    return

  # TODO(robertm): remove support for .S files
  if HasAssemblerFiles(argv):
    assert TOLERATE_COMPILATION_OF_ASM_CODE
    Run(argv + LLVM_GCC_ASSEMBLER_FLAGS)
    return

  argv.append('--emit-llvm')
  Run(argv)


def Incarnation_sfigcc(argv):
  Compile(argv, LLVM_PREFIX + '-gcc', True)


def Incarnation_sfigplusplus(argv):
  Compile(argv, LLVM_PREFIX + '-g++', True)


def Incarnation_gcc(argv):
  Compile(argv, LLVM_PREFIX + '-gcc', False)


def Incarnation_gplusplus(argv):
  Compile(argv, LLVM_PREFIX + '-g++', False)


def Incarnation_bcgcc(argv):
  CompileToBitcode(argv, LLVM_PREFIX + '-gcc')


def Incarnation_bcgplusplus(argv):
  CompileToBitcode(argv, LLVM_PREFIX + '-g++')


def Incarnation_nop(argv):
  print "IGNORING " + repr(argv)
  return


def Incarnation_illegal(argv):
  raise Exception, "UNEXPECTED " + repr(argv)


def Incarnation_as(argv):
  raise Exception, "UNEXPECTED " + repr(argv)


def Incarnation_sfiar(argv):
  argv[0] = AR
  Run(argv)

def Incarnation_bcar(argv):
  """Emulate the behavior or ar on .o files with llvm-link on .bc files."""
  print '@' * 70
  print 'ar ', repr(argv)
  print '@' * 70

  # 'x' indicates "extract from archive"
  if argv[1] == 'x':
    src = argv[2]
    dst = src.replace('.', '').replace('/', '_')
    dst = dst + ".o"
    Run(['cp', src, dst])
  else:
    # 'c' indicates "create archive"
    # TODO(robertm): we may have to add more cases in the future
    assert argv[1].startswith('c') or argv[1].startswith('rc')
    # NOTE: the incomming command looks something like
    # ..../llvm-fake-bcar cru lib.a dummy.o add.o sep.o
    Run([LLVM_LINK, '-o'] + argv[2:])


INCARNATIONS = {
   'llvm-fake-sfigcc': Incarnation_sfigcc,
   'llvm-fake-sfig++': Incarnation_sfigplusplus,

   'llvm-fake-gcc': Incarnation_gcc,
   'llvm-fake-g++': Incarnation_gplusplus,

   'llvm-fake-bcgcc': Incarnation_bcgcc,
   'llvm-fake-bcg++': Incarnation_bcgplusplus,

   'llvm-fake-sfiar': Incarnation_sfiar,
   'llvm-fake-bcar': Incarnation_bcar,

   'llvm-fake-sfiranlib': Incarnation_nop,
   'llvm-fake-bcranlib': Incarnation_nop,

   'llvm-fake-as': Incarnation_as,

   'llvm-fake-nop': Incarnation_nop,
   'llvm-fake-illegal': Incarnation_illegal,
   }


def main(argv):
  basename = os.path.basename(argv[0])
  if basename not in INCARNATIONS:
    print  "ERROR: unknown command: " + repr(argv)
    return -1

  INCARNATIONS[basename](argv)
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv))
