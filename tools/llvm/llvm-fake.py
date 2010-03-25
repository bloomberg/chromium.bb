#!/usr/bin/python
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# this script is a replacement for llvm-gcc and llvm-g++ driver.
# It detects automatically which role it is supposed to assume.
# The point of the script is to redirect builds through our own tools,
# while making these tools appear like gnu tools.
#
# Compared to the gcc driver the usage patterns of this script
# are somewhat restricted:
# * you cannot compile and link at the same time
# * you cannot specify multiple source files in one invocation
# * ...
#


import os
import struct
import subprocess
import sys

# enable this for manual debugging of this script only
# NOTE: this HAS to be zero in order to work with the llvm-gcc
#       bootstrapping process
VERBOSE = 0
TOLERATE_COMPILATION_OF_ASM_CODE = 1
# NOTE: set this to something like:
OUT = open('/tmp/fake.log', 'a')
# if you want a log of all the action. otherwise:
#OUT = None

######################################################################
# Misc
######################################################################

BASE = os.path.dirname(os.path.dirname(sys.argv[0]))

LD_SCRIPT_ARM = BASE + '/arm-none-linux-gnueabi/ld_script_arm_untrusted'

LIBDIR = BASE + '/armsfi-lib'

LIBDIR2 = BASE + '/arm-newlib/arm-none-linux-gnueabi/lib'

# NOTE: ugly work around for some llvm-ld shortcomings:
#       we need to keep some symbols alive which are referenced by
#       native code libraries linked in at the very end
REACHABLE_FUNCTION_SYMBOLS = LIBDIR2 + '/reachable_function_symbols.o'

# Note: this works around an assembler bug that has been fixed only recently
# We probably can drop this once we have switched to codesourcery 2009Q4
HACK_ASM = ['sed', '-e', 's/vmrs.*apsr_nzcv, fpscr/fmrx r15, fpscr/g']

######################################################################
# FLAGS
######################################################################

LLVM_GCC_ASSEMBLER_FLAGS = [
    '-march=armv6',
    '-mfpu=vfp',
    # Possible other settings:
    #'-march=armv7',
    #'-mcpu=cortex-a8',
    '-D__native_client__=1',
    ]

# TODO(robertm): not yet used
LLVM_GCC_COMPILE_FLAGS = [
    '-nostdinc',
    '-D__native_client__=1',
    '-DNACL_TARGET_ARCH=arm',
    '-DNACL_TARGET_SUBARCH=32',
    '-DNACL_LINUX=1',
    '-ffixed-r9',
    '-march=armv6',
    '-isystem', BASE + '/arm-newlib/newlib_extra_header',
    '-isystem', BASE + '/arm-newlib/newlib_extra_header/c++/4.2.1',
    '-isystem',
    BASE + '/arm-newlib/newlib_extra_header/c++/4.2.1/arm-none-linux-gnueabi',

    '-isystem', BASE + '/arm-newlib/arm-none-linux-gnueabi/include',
    '-isystem', BASE + '/arm-newlib/arm-none-linux-gnueabi/include/sys',
    ]


LLC_SHARED_FLAGS = [
    '-march=arm',
    # c.f. lib/Target/ARM/ARMGenSubtarget.inc
    '-mcpu=arm1156t2f-s',
    #'-mcpu=cortex-a8',
    '-mtriple=armv6-*-*eabi*',
    #'-mtriple=armv7a-*-*eabi*',
    '-arm-reserve-r9',
    ]


LLC_SFI_SANDBOXING_FLAGS = [
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
    ]


OPT_FLAGS = [
    '-O3',
    '-std-compile-opts'
    ]


# We have our own linker script to make sure the section layout
# follows the nacl abi
LD_FLAGS = [
    '-nostdlib',
    '-T', LD_SCRIPT_ARM,
    '-static',
    ]

######################################################################
# Executables
######################################################################

LLVM_GCC = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/llvm-gcc'

LLVM_GXX = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/llvm-g++'

LLC = BASE + '/arm-none-linux-gnueabi/llvm/bin/llc'

LLC_SFI = BASE + '/arm-none-linux-gnueabi/llvm/bin/llc-sfi'

LLVM_LINK = BASE + '/arm-none-linux-gnueabi/llvm/bin/llvm-link'

LLVM_LD = BASE + '/arm-none-linux-gnueabi/llvm/bin/llvm-ld'

OPT = BASE + '/arm-none-linux-gnueabi/llvm/bin/opt'

# NOTE: from code sourcery
AS = BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-as'

# NOTE: from code sourcery
# TODO(robertm): get rid of the use of gcc here this uses cpp + as
AS_PRE = BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-gcc'

# NOTE: from code sourcery
LD = BASE + '/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-ld'

######################################################################
# Code
######################################################################

def LogInfo(m):
  if VERBOSE:
    print m
  if OUT:
    print >> OUT, m


def LogFatal(m, ret=-1):
  print
  print "FATAL:", m
  if OUT:
    print >> OUT, "FATAL:", m
  print
  sys.exit(ret)


def StringifyCommand(a):
  return repr(a)


def Run(a):
  LogInfo('\n' + StringifyCommand(a))
  ret = subprocess.call(a)
  if ret:
    LogFatal('failed command: ' + StringifyCommand(a), ret)



def SfiCompile(argv, out_pos, mode):
  filename = argv[out_pos]

  argv[out_pos] = filename + '.bc'
  # TOOD(robertm): needs to be synchronized with
  #                tools/llvm/setup_arm_untrusted_toolchain.sh
  #if '--nostdinc' not in argv:
  #  argv += LLVM_GCC_COMPILE_FLAGS
  argv.append('--emit-llvm')
  Run(argv)

  Run([OPT] + OPT_FLAGS + [filename + '.bc', '-f', '-o', filename + '.opt.bc'])

  if mode == 'sfi':
    llc = [LLC_SFI] + LLC_SFI_SANDBOXING_FLAGS
  else:
    llc = [LLC]
  llc += LLC_SHARED_FLAGS
  llc += ['-f', filename + '.opt.bc', '-o', filename + '.s']
  Run(llc)

  Run(HACK_ASM + ['-i.orig', filename + '.s'])

  Run([AS] + [filename + '.s', '-o', filename])


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
    if a.endswith('.S') or a.endswith('.s') or a.endswith('.asm'):
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

# NOTE: configure, especially when run as part of a toolchain bootstrap
#       process will invoke gcc with various diagnostic options and
#       parse the output.
#       In those cases we do not alter the incoming commandline.
#       It is also important to not emit spurious messages, e.g.
#       via VERBOSE in those case
def IsDiagnosticMode(argv):
  return (# used by configure
          'conftest.c' in argv or
          '--print-multi-lib' in argv or
          # used by scons harness
          '--v' in argv or
          '-v' in argv or
          '-dumpspecs' in argv or
          '-print-search-dirs' in argv or
          '-print-libgcc-file-name' in argv)


# TODO(robertm): maybe invoke CPP for here rather then depending on the
#                code sourcery driver
def Assemble(argv):
   assert TOLERATE_COMPILATION_OF_ASM_CODE
   Run([AS_PRE] + argv[1:] + LLVM_GCC_ASSEMBLER_FLAGS)


def Compile(argv, llvm_binary, mode):
  """ Compile to .o file."""
  argv[0] = llvm_binary

  if FindLinkPos(argv):
    # NOTE: this happens when using the toolchain builer scripts
    LogInfo('found .o -> exe compilation')
    Run(argv)

  obj_pos = FindObjectFilePos(argv)

  if not obj_pos:
    if IsDiagnosticMode(argv):
      LogInfo('operating in diagnostic mode')
      Run(argv)
    else:
      LogFatal('weird invocation ' + StringifyCommand(argv))
    return

  # TODO(robertm): remove support for .S files
  if HasAssemblerFiles(argv):
    assert TOLERATE_COMPILATION_OF_ASM_CODE
    Assemble(argv)
    return

  if mode == 'bitcode':
    argv.append('--emit-llvm')
    Run(argv)
  else:
    SfiCompile(argv, obj_pos, mode)



def Incarnation_sfigcc(argv):
  Compile(argv, LLVM_GCC, 'sfi')


def Incarnation_sfigplusplus(argv):
  Compile(argv, LLVM_GXX, 'sfi')


def Incarnation_gcc(argv):
  Compile(argv, LLVM_GCC, 'regular')


def Incarnation_gplusplus(argv):
  Compile(argv, LLVM_GXX, 'regular')


def Incarnation_bcgcc(argv):
  Compile(argv, LLVM_GCC, 'bitcode')


def Incarnation_bcgplusplus(argv):
  Compile(argv, LLVM_GXX, 'bitcode')


def Incarnation_nop(argv):
  LogInfo('\nIGNORING: ' + StringifyCommand(argv))
  return


def Incarnation_illegal(argv):
  LogFatal('illegal command ' + StringifyCommand(argv))


def MassageFinalLinkCommand(args):
  out = LD_FLAGS

  # add init code
  if '-nostdlib' not in args:
    out.append(LIBDIR2 + '/crt1.o')
    out.append(LIBDIR2 + '/crti.o')
    out.append(LIBDIR2 + '/intrinsics.o')

  out += args

  # add fini code
  if '-nostdlib' not in args:
    # NOTE: there is a circular dependency between libgcc and libc: raise()
    out.append(LIBDIR2 + '/crtn.o')
    out.append('-L' + LIBDIR)
    out.append('-lgcc')
  return out


MAGIC_OBJS = set([
    # special hack for tests/syscall_return_sandboxing
    'sandboxed_x86_32.o',
    'sandboxed_x86_64.o',
    'sandboxed_arm.o',
    ])


DROP_ARGS = set([])


NATIVE_ARGS = set(['-nostdlib'])


def Incarnation_bcld(argv):
  """The ld step for bitcode is quite elaborate:
     1) Run llvm-ld to produce a single bitcode file
     2) Run llc to convert to .s
     3) Run as to convert to .o
     4) Run ld
     5) Patch ABI

     The tricky part is that some args apply to phase 1 and some to phase 4.
     Most of the code below is dedicated to pick those apart.


     TODO(robertm): llvm-ld does NOT complain when passed a native .o file.
                    It will discard it silently.
  """
  args_bit_ld = []
  args_native_ld = []
  last_bitcode_pos = None
  output = None
  last = None

  for a in argv[1:]:
    if last:
      # combine current item with prev
      a = last + '$' + a
      last = None

    if a in ['-o', '-T']:
      # combine two arg items into one arg
      last = a
    elif a in DROP_ARGS:
      pass
    elif a.endswith('.o'):
      _, base = os.path.split(a)
      if base in MAGIC_OBJS:
        args_native_ld.append(a)
      else:
        args_bit_ld.append(a)
        last_bitcode_pos = len(args_bit_ld)
    elif a in NATIVE_ARGS:
      args_native_ld.append(a)
    elif a.startswith('-l'):
      args_bit_ld.append(a)
    elif a.startswith('-L'):
      # we replicate library search paths to both arg lists
      args_bit_ld.append(a)
      args_native_ld.append(a)
    elif a.startswith('-o$'):
      tokens = a.split('$')
      output = tokens[1]
      args_native_ld += tokens
    else:
      LogFatal('Unexpected ld arg: %s' % a)

  assert output

  bitcode_combined = output + ".bc"
  asm_combined = output + ".bc.s"
  native_combined = output + ".bc.o"


  # NOTE: LLVM_LD automagically appends .bc to the output
  # NOTE: without -disable-internalize only the symbol 'main'
  #       is exported, but we need some more for the startup code
  #       which kept alive via REACHABLE_FUNCTION_SYMBOLS
  if '-nostdlib' not in argv:
    if last_bitcode_pos != None:
      args_bit_ld = (args_bit_ld[:last_bitcode_pos] +
                     [REACHABLE_FUNCTION_SYMBOLS] +
                     args_bit_ld[last_bitcode_pos:])

  Run([LLVM_LD] + args_bit_ld + ['-disable-internalize', '-o', output])

  Run([LLC_SFI] + LLC_SHARED_FLAGS + LLC_SFI_SANDBOXING_FLAGS +
      ['-f', bitcode_combined, '-o', asm_combined])

  Run(HACK_ASM + ['-i.orig', asm_combined])

  Run([AS] + LLVM_GCC_ASSEMBLER_FLAGS + [asm_combined, '-o', native_combined])

  args_native_ld = MassageFinalLinkCommand([native_combined] + args_native_ld)

  Run([LD] +  args_native_ld)

  PatchAbiVersionIntoElfHeader(output)


def Incarnation_sfild(argv):
  """Run the regular linker and then patch the ABI"""
  pos = FindLinkPos(argv)
  assert pos
  output = argv[pos]
  extra = []
  # force raise() to be live (needed by libgcc's div routine)
  if '-nostdlib' not in argv:
    extra = [REACHABLE_FUNCTION_SYMBOLS]
  Run([LD] +  MassageFinalLinkCommand(extra + argv[1:]))

  PatchAbiVersionIntoElfHeader(output)

######################################################################
# Dispatch based on name the scripts is invoked with
######################################################################

INCARNATIONS = {
   'llvm-fake-sfigcc': Incarnation_sfigcc,
   'llvm-fake-sfig++': Incarnation_sfigplusplus,

   'llvm-fake-gcc': Incarnation_gcc,
   'llvm-fake-g++': Incarnation_gplusplus,

   'llvm-fake-bcgcc': Incarnation_bcgcc,
   'llvm-fake-bcg++': Incarnation_bcgplusplus,

   'llvm-fake-sfild': Incarnation_sfild,
   'llvm-fake-bcld': Incarnation_bcld,

   'llvm-fake-illegal': Incarnation_illegal,
   'llvm-fake-nop': Incarnation_nop,
   }


def main(argv):
  LogInfo('\nRUNNNG\n ' + StringifyCommand(argv))
  basename = os.path.basename(argv[0])
  if basename not in INCARNATIONS:
    LogFatal("unknown command: " + StringifyCommand(argv))


  INCARNATIONS[basename](argv)
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv))
