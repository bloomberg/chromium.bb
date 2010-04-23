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
#       bootstrapping process because the compiler is used in
#       diagnostic mode and configure reads information that the driver
#       generates on stdout and stderr
VERBOSE = 0
TOLERATE_COMPILATION_OF_ASM_CODE = 1

OUT=[sys.stderr]
# if you want a log of all the action, remove comment below:
OUT.append(open('/tmp/fake.log', 'a'))


######################################################################
# Misc
######################################################################

BASE = os.path.dirname(os.path.dirname(sys.argv[0]))

LD_SCRIPT_ARM = BASE + '/arm-none-linux-gnueabi/ld_script_arm_untrusted'

# TODO(robertm) clean this up
LD_SCRIPT_X8632 = BASE + '/../../tools/llvm/ld_script_x86_untrusted'

# arm libstdc++
LIBDIR_ARM_3 = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/lib'

# arm startup code + libs (+ bitcode when in bitcode mode)
LIBDIR_ARM_2 = BASE + '/arm-newlib/arm-none-linux-gnueabi/lib'

# arm libgcc
LIBDIR_ARM_1 = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1/'

# NOTE: ugly work around for some llvm-ld shortcomings:
#       we need to keep some symbols alive which are referenced by
#       native code libraries linked in at the very end
REACHABLE_FUNCTION_SYMBOLS = LIBDIR_ARM_2 + '/reachable_function_symbols.o'

# Note: this works around an assembler bug that has been fixed only recently
# We probably can drop this once we have switched to codesourcery 2009Q4
HACK_ASM = ['sed', '-e', 's/vmrs.*apsr_nzcv, fpscr/fmrx r15, fpscr/g']

PNACL_ARM_ROOT =  BASE + '/../pnacl-untrusted/arm'

PNACL_X8632_ROOT = BASE + '/../pnacl-untrusted/x8632'

PNACL_X8664_ROOT = BASE + '/../pnacl-untrusted/x8664'

PNACL_BITCODE_ROOT = BASE + '/../pnacl-untrusted/bitcode'

REACHABLE_FUNCTION_SYMBOLS_BC = PNACL_BITCODE_ROOT + '/reachable_function_symbols.o'
######################################################################
# FLAGS
######################################################################

AS_FLAGS_ARM = [
    '-march=armv6',
    '-mfpu=vfp',
    # Possible other settings:
    #'-march=armv7',
    #'-mcpu=cortex-a8',
    ]

AS_FLAGS_X8632 = [
    '--32',
    '--nacl-align', '5',
    # turn off nop
    '-n',
    '-march=pentium4',
    '-mtune=i386',
    ]

LLVM_GCC_COMPILE_FLAGS = [
    '-nostdinc',
    '-D__native_client__=1',
    '-DNACL_TARGET_ARCH=arm',
    # TODO: get rid of the next two lines
    '-DNACL_TARGET_SUBARCH=32',
    '-DNACL_LINUX=1',
    '-ffixed-r9',
    '-march=armv6',
    ]

BASE_ARM = BASE + '/arm-none-linux-gnueabi'

BASE_ARM_GCC = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2'

LLVM_GCC_COMPILE_FLAGS_HEADERS = [
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowloedge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem',
    BASE_ARM_GCC + '/lib/gcc/arm-none-linux-gnueabi/4.2.1/include',
    '-isystem',
    BASE_ARM_GCC + '/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include',
    '-isystem',
    BASE_ARM_GCC + '/include/c++/4.2.1',
    '-isystem',
    BASE_ARM_GCC + '/include/c++/4.2.1/arm-none-linux-gnueabi',
    '-isystem',
    BASE_ARM_GCC + '/arm-none-linux-gnueabi/include',

    # NOTE: order important
#    '-isystem',
#    BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem',
    BASE + '/arm-newlib/arm-none-linux-gnueabi/include',
#    '-isystem',
#    BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include',
    ]


LLC_SHARED_FLAGS_ARM = [
    '-march=arm',
    # c.f. lib/Target/ARM/ARMGenSubtarget.inc
    '-mcpu=arm1156t2f-s',
    #'-mcpu=cortex-a8',
    '-mtriple=armv6-*-*eabi*',
    #'-mtriple=armv7a-*-*eabi*',
    '-arm-reserve-r9',
    ]


LLC_SHARED_FLAGS_X8632 = [
    '-march=x86',
    '-mcpu=pentium4',
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


# Our linker script makes sure that the section layout follows the nacl abi
LD_FLAGS_ARM = [
    '-nostdlib',
    '-T', LD_SCRIPT_ARM,
    '-static',
    ]

# Our linker script makes sure that the section layout follows the nacl abi
LD_FLAGS_X8632 = [
    '-nostdlib',
    '-T', LD_SCRIPT_X8632,
#    '-melf_nacl',
    '-static',
    ]

######################################################################
# Executables invoked by this driver
######################################################################

LLVM_GCC = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/llvm-gcc'

LLVM_GXX = BASE + '/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/llvm-g++'

LLC_ARM = BASE + '/arm-none-linux-gnueabi/llvm/bin/llc'

LLC_SFI_ARM = BASE + '/arm-none-linux-gnueabi/llvm/bin/llc-sfi'

# Path to llc executable for x86-32.
LLC_SFI_X86 = os.getenv('LLC_SFI_X86', None)

LLVM_LINK = BASE + '/arm-none-linux-gnueabi/llvm/bin/llvm-link'

LLVM_LD = BASE + '/arm-none-linux-gnueabi/llvm/bin/llvm-ld'

OPT = BASE + '/arm-none-linux-gnueabi/llvm/bin/opt'

# NOTE: from code sourcery
AS_ARM = BASE + '/codesourcery/arm-2007q3/arm-none-linux-gnueabi/bin/as'

# NOTE: hack, assuming presence of x86/32 toolchain
AS_X8632 = BASE + '/../linux_x86/sdk/nacl-sdk/bin/nacl64-as'

LD_ARM = BASE + '/codesourcery/arm-2007q3/arm-none-linux-gnueabi/bin/ld'

# NOTE: hack, assuming presence of x86/32 toolchain expected
# TODO(robertm): clean this up - we may not need separate linkers
# NOTE(robertm): the nacl linker sometimes creates empty sections like .plt
#                so we use the system linker for now
#LD_X8632 = BASE + '/../linux_x86-32/sdk/nacl-sdk/bin/nacl-ld'
LD_X8632 = '/usr/bin/ld'
# NOTE(adonovan): this should _not_ be the Gold linker, e.g. 2.18.*.
# Beware, one of the Chrome installation scripts may have changed
# /usr/bin/ld (which is evil).

CPP = '/usr/bin/cpp'
######################################################################
# Code
######################################################################

def LogInfo(m):
  if VERBOSE:
    for o in OUT:
      print >> o, m


def LogFatal(m, ret=-1):
  for o in OUT:
    print >> o, 'FATAL:', m
    print >> o, ''
  sys.exit(ret)

def LogWarning(m):
  for o in OUT:
    print >> o, 'WARNING:', m
    print >> o, ''

def StringifyCommand(a):
  return " ".join(a)


def Run(args):
  "Run the commandline give by the list args system()-style"
  LogInfo('\n' + StringifyCommand(args))
  try:
    ret = subprocess.call(args)
  except Exception, e:
    LogFatal('failed (%s) to run: ' % str(e) + StringifyCommand(args))

  if ret:
    LogFatal('failed command: ' + StringifyCommand(args), ret)



def SfiCompile(argv, out_pos, mode):
  filename = argv[out_pos]

  argv[out_pos] = filename + '.bc'

  argv.append('--emit-llvm')
  Run(argv)

  Run([OPT] + OPT_FLAGS + [filename + '.bc', '-f', '-o', filename + '.opt.bc'])

  if mode == 'sfi':
    llc = [LLC_SFI_ARM] + LLC_SFI_SANDBOXING_FLAGS
  else:
    llc = [LLC_ARM]
  llc += LLC_SHARED_FLAGS_ARM
  llc += ['-f', filename + '.opt.bc', '-o', filename + '.s']
  Run(llc)

  Run(HACK_ASM + ['-i.orig', filename + '.s'])

  Run([AS_ARM] + [filename + '.s', '-o', filename])


def PatchAbiVersionIntoElfHeader(filename, alignment):
  # modeling Elf32_Ehdr
  FORMAT = '16BHH5I6H'
  if alignment == 16:
    alignment_bitmask = 0x100000
  elif alignment == 32:
    alignment_bitmask = 0x200000
  else:
    LogFatal('unsupported alignment: ' + alignment)

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
  t[ELF_FLAGS_INDEX] = t[ELF_FLAGS_INDEX] | alignment_bitmask
  data = struct.pack(FORMAT, *t)
  fp = open(filename, 'rb+')
  fp.write(data)
  fp.close()

def FindObjectFilePos(argv):
  """Return argv index if there is and object file is being generated
  by the commandline argv.
  Return None if no object file is generated.
  """
  if '-c' not in argv:
    return None

  if '-o' not in argv:
    return None

  pos = 1 + argv.index('-o')
  assert pos < len(argv)

  if not argv[pos].endswith('.o'):
    return None

  return pos


def FindAssemblerFilePos(argv):
  """Return argv index if an assembler file is compiled
  by the commandline argv.
  Return None if no assembler is compiled.
  """
  for n, a in enumerate(argv):
    if a.endswith('.S') or a.endswith('.s') or a.endswith('.asm'):
      return n
  else:
    return None


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
  for a in argv:
    # used to dump out various internal information for gcc
    if a.startswith('-print-') or a.startswith('--print'):
      return True
    if a == '-dumpspecs':
      return True
    # dump versioning info - not sure about "-V"
    if a in ['-v', '--version', '--v', '-V']:
      return True
    # used by configure
    if a == 'conftest.c':
      return True
  return False


def Assemble(asm, flags, argv):
  # TODO(robertm): this needs to be a lot more robust
  #                we also might want to pass some options thru to the
  #                assembler
  cpp_flags = [a for a in argv
               if a.startswith('-D') or a.startswith('-I')]

  cpp_flags += ['-D__native_client__=1']

  s_file = FindAssemblerFilePos(argv)
  if s_file is None:
     LogFatal('cannot find assembler source  file ' + StringifyCommand(argv))
  else:
    s_file = argv[s_file]

  obj_file = FindObjectFilePos(argv)
  if obj_file is None:
    LogFatal('cannot find object file ' + StringifyCommand(argv))
  else:
    obj_file = argv[obj_file]

  cpp_file = obj_file + ".cpp"

  Run([CPP] + cpp_flags + [s_file, cpp_file])
  Run([asm] + flags + ['-o', obj_file, cpp_file])


def AssembleArm(argv):
  Assemble(AS_ARM, AS_FLAGS_ARM, argv)


def AssembleX8632(argv):
  Assemble(AS_X8632, AS_FLAGS_X8632, argv)



def Compile(argv, llvm_binary, mode):
  """ Compile to .o file."""
  argv[0] = llvm_binary

  if FindLinkPos(argv):
    # NOTE: this happens when using the toolchain builer scripts
    LogInfo('found .o -> exe compilation')
    Run(argv)

  obj_pos = FindObjectFilePos(argv)

  if obj_pos is None:
    if IsDiagnosticMode(argv):
      #LogWarning('diagnostic mode:' + StringifyCommand(argv))
      Run(argv)
    else:
      LogFatal('weird invocation ' + StringifyCommand(argv))
    return

  # TODO(robertm): remove support for .S files
  if FindAssemblerFilePos(argv) is not None:
    assert TOLERATE_COMPILATION_OF_ASM_CODE
    if '-raw-mode' in  argv:
      argv.remove('-raw-mode')
    Run(argv)
    return

  # NOTE:
  # In raw mode we do not force our own flags on the underlying compiler.
  # This is used for toolchain bootstrapping.
  # Otherwise, we add out own options and overwrite system include paths
  # TODO(robertm): clean this up
  if '-raw-mode' in  argv:
    argv.remove('-raw-mode')
  elif '-nostdinc' in argv:
    argv += LLVM_GCC_COMPILE_FLAGS
  else:
    argv += LLVM_GCC_COMPILE_FLAGS
    argv += LLVM_GCC_COMPILE_FLAGS_HEADERS

  if mode == 'bitcode':
    argv.append('--emit-llvm')
    Run(argv)
  else:
    SfiCompile(argv, obj_pos, mode)


def Incarnation_sfigcc(argv):
  Compile(argv, LLVM_GCC, 'sfi')


def Incarnation_sfigplusplus(argv):
  Compile(argv, LLVM_GXX, 'sfi')

# in raw mode we do not force our own flags on the underlying compiler
def Incarnation_rawsfigcc(argv):
  Compile(argv + ['-raw-mode'], LLVM_GCC, 'sfi')


def Incarnation_rawsfigplusplus(argv):
  Compile(argv + ['-raw-mode'], LLVM_GXX, 'sfi')


def Incarnation_gcc(argv):
  Compile(argv, LLVM_GCC, 'regular')


def Incarnation_gplusplus(argv):
  Compile(argv, LLVM_GXX, 'regular')


def Incarnation_bcgcc(argv):
  Compile(argv, LLVM_GCC, 'bitcode')


def Incarnation_bcgplusplus(argv):
  Compile(argv, LLVM_GXX, 'bitcode')


def Incarnation_cppasarm(argv):
  AssembleArm(argv)


def Incarnation_cppasx8632(argv):
  AssembleX8632(argv)


def Incarnation_nop(argv):
  LogInfo('\nIGNORING: ' + StringifyCommand(argv))


def Incarnation_illegal(argv):
  LogFatal('illegal command ' + StringifyCommand(argv))


def MassageFinalLinkCommandArm(args):
  out = LD_FLAGS_ARM

  # add init code
  if '-nostdlib' not in args:
    out.append(LIBDIR_ARM_2 + '/crt1.o')
    out.append(LIBDIR_ARM_2 + '/crti.o')
    out.append(LIBDIR_ARM_2 + '/intrinsics.o')

  out += args

  # add fini code
  if '-nostdlib' not in args:
    # NOTE: there is a circular dependency between libgcc and libc: raise()
    out.append(LIBDIR_ARM_2 + '/libcrt_platform.a')
    out.append(LIBDIR_ARM_2 + '/crtn.o')
    out.append('-L' + LIBDIR_ARM_3)
    out.append('-lstdc++')
    out.append('-lc')
    out.append('-L' + LIBDIR_ARM_1)
    out.append('-lgcc')
  return out



def MassageFinalLinkCommandPnacl(args, native_dir, flags):
  out = flags

  # add init code
  if '-nostdlib' not in args:
    out.append(native_dir + '/crt1.o')
    out.append(native_dir + '/crti.o')
    out.append(native_dir + '/intrinsics.o')

  out += args

  # add fini code
  if '-nostdlib' not in args:
    # NOTE: there is a circular dependency between libgcc and libc: raise()
    out.append(native_dir + '/libcrt_platform.a')
    out.append(native_dir + '/crtn.o')
    out.append('-L' + native_dir)
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

def GenerateCombinedBitcodeFile(argv, arch):
  """Run llvm-ld to produce a single bitcode file
  Returns:
  name of resulting bitcode file without .bc extension.

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
    elif a.endswith('.bc'):
       args_bit_ld.append(a)
       last_bitcode_pos = len(args_bit_ld)
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

  # NOTE: LLVM_LD automagically appends .bc to the output
  # NOTE: without -disable-internalize only the symbol 'main'
  #       is exported, but we need some more for the startup code
  #       which kept alive via REACHABLE_FUNCTION_SYMBOLS
  if '-nostdlib' not in argv:
    if last_bitcode_pos != None:
        # Splice in the extra symbols.
        args_bit_ld = (args_bit_ld[:last_bitcode_pos] +
                       [REACHABLE_FUNCTION_SYMBOLS_BC] +
                       args_bit_ld[last_bitcode_pos:] +
                       [ #'-lstdc++',
                       '-lc',
                        ])

  # NOTE: .bc will be appended output by LLVM_LD
  Run([LLVM_LD] + args_bit_ld + ['-disable-internalize', '-o', output])
  return output, args_native_ld


def Incarnation_bcldarm(argv):
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
  output, args_native_ld = GenerateCombinedBitcodeFile(argv, 'arm')

  bitcode_combined = output + ".bc"
  asm_combined = output + ".bc.s"
  obj_combined = output + ".bc.o"

  Run([LLC_SFI_ARM] + LLC_SHARED_FLAGS_ARM + LLC_SFI_SANDBOXING_FLAGS +
      ['-f', bitcode_combined, '-o', asm_combined])

  Run(HACK_ASM + ['-i.orig', asm_combined])

  Run([AS_ARM] + AS_FLAGS_ARM + [asm_combined, '-o', obj_combined])

  args_native_ld = MassageFinalLinkCommandPnacl([obj_combined] + args_native_ld,
                                                PNACL_ARM_ROOT,
                                                LD_FLAGS_ARM)

  Run([LD_ARM] +  args_native_ld)

  PatchAbiVersionIntoElfHeader(output, 16)


# NOTE: this has never been tested and is intended to server as
#       a starting point for future x86 explorations
def Incarnation_bcldx8632(argv):
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
  output, args_native_ld = GenerateCombinedBitcodeFile(argv, 'x86-32')

  bitcode_combined = output + ".bc"
  asm_combined = output + ".bc.s"
  obj_combined = output + ".bc.o"

  # TODO(adonovan): use external env var until we complete llc SFI for x86.
  if not LLC_SFI_X86:
    LogFatal('You must set LLC_SFI_X86 to your llc executable for x86-32.')

  Run([LLC_SFI_X86] +
      LLC_SHARED_FLAGS_X8632 +
      ['-f', bitcode_combined, '-o', asm_combined])

  Run([AS_X8632] + AS_FLAGS_X8632 + [asm_combined, '-o', obj_combined])

  args_native_ld = MassageFinalLinkCommandPnacl([obj_combined] + args_native_ld,
                                                PNACL_X8632_ROOT,
                                                LD_FLAGS_X8632)

  Run([LD_X8632] +  args_native_ld)

  PatchAbiVersionIntoElfHeader(output, 32)


def Incarnation_sfild(argv):
  """Run the regular linker and then patch the ABI"""
  pos = FindLinkPos(argv)
  assert pos
  output = argv[pos]
  extra = []
  # force raise() to be live (needed by libgcc's div routine)
  if '-nostdlib' not in argv:
    extra = [REACHABLE_FUNCTION_SYMBOLS]
  Run([LD_ARM] +  MassageFinalLinkCommandArm(extra + argv[1:]))

  PatchAbiVersionIntoElfHeader(output, 16)

######################################################################
# Dispatch based on name the scripts is invoked with
######################################################################

INCARNATIONS = {
   'llvm-fake-sfigcc': Incarnation_sfigcc,
   'llvm-fake-sfig++': Incarnation_sfigplusplus,

   'llvm-fake-rawsfigcc': Incarnation_rawsfigcc,
   'llvm-fake-rawsfig++': Incarnation_rawsfigplusplus,

   'llvm-fake-gcc': Incarnation_gcc,
   'llvm-fake-g++': Incarnation_gplusplus,

   'llvm-fake-bcgcc': Incarnation_bcgcc,
   'llvm-fake-bcg++': Incarnation_bcgplusplus,

   'llvm-fake-sfild': Incarnation_sfild,

   'llvm-fake-bcld-arm': Incarnation_bcldarm,
   'llvm-fake-bcld-x86-32': Incarnation_bcldx8632,

   'llvm-fake-cppas-arm': Incarnation_cppasarm,
   'llvm-fake-cppas-x86-32': Incarnation_cppasx8632,

   'llvm-fake-illegal': Incarnation_illegal,
   'llvm-fake-nop': Incarnation_nop,
   }


def main(argv):
  LogInfo('\nRUNNNG\n ' + StringifyCommand(argv))
  basename = os.path.basename(argv[0])
  if basename not in INCARNATIONS:
    LogFatal("unknown command incarnation: " + StringifyCommand(argv))


  INCARNATIONS[basename](argv)
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv))
