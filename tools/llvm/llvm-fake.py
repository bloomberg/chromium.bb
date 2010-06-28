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
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/untrusted-toolchain-creator.sh driver-symlink
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.

import os
import struct
import subprocess
import sys

# enable this for manual debugging of this script only
# NOTE: Setting VERBOSE to "1" is useful for debugging.
#       But it HAS to be zero in order to work with the llvm-gcc
#       bootstrapping process. "Configure" is running the compiler in
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

# e.g. $NACL/toolchain/linux_arm-untrusted
BASE = os.path.dirname(os.path.dirname(sys.argv[0]))

BASE_ARM = BASE + '/arm-none-linux-gnueabi'


LD_SCRIPT_ARM = BASE_ARM + '/ld_script_arm_untrusted'

# NOTE: derived from
# toolchain/linux_x86/sdk/nacl-sdk/nacl64/lib/ldscripts/elf_nacl.x
LD_SCRIPT_X8632 = BASE + '/../../tools/llvm/ld_script_x8632_untrusted'
# NOTE: derived from
# toolchain/linux_x86/sdk/nacl-sdk/nacl64/lib/ldscripts/elf64_nacl.x
LD_SCRIPT_X8664 = BASE + '/../../tools/llvm/ld_script_x8664_untrusted'

# arm libstdc++
LIBDIR_ARM_3 = BASE_ARM + '/llvm-gcc-4.2/lib'

# arm startup code + libs (+ bitcode when in bitcode mode)
LIBDIR_ARM_2 = BASE + '/arm-newlib/arm-none-linux-gnueabi/lib'

# arm libgcc
LIBDIR_ARM_1 = BASE_ARM + '/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1/'

PNACL_ARM_ROOT =  BASE + '/../pnacl-untrusted/arm'

PNACL_X8632_ROOT = BASE + '/../pnacl-untrusted/x8632'

PNACL_X8664_ROOT = BASE + '/../pnacl-untrusted/x8664'

PNACL_BITCODE_ROOT = BASE + '/../pnacl-untrusted/bitcode'

######################################################################
# FLAGS (can be overwritten by
######################################################################
arm_flags = ['-mfpu=vfp3',
             '-march=armv7-a']

global_config_flags = {
  'AS_ARM': arm_flags,


  'AS_X8632': [
    '--32',
    '--nacl-align', '5',
    # turn off nop
    '-n',
    '-march=pentium4',
    '-mtune=i386',
  ],


  'AS_X8664': [
    '--64',
    '--nacl-align', '5',
    # turn off nop
    '-n',
    '-mtune=core2',
  ],


  'LLVM_GCC_COMPILE': [
    '-nostdinc',
    '-D__native_client__=1',
    '-DNACL_TARGET_ARCH=arm',
    # TODO: get rid of the next two lines
    '-DNACL_TARGET_SUBARCH=32',
    '-DNACL_LINUX=1',
    '-ffixed-r9',
  ] + arm_flags,


  'LLVM_GCC_COMPILE_HEADERS': [
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowloedge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem',
    BASE_ARM + '/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1/include',
    '-isystem',
    BASE_ARM +
    '/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include',
    '-isystem',
    BASE_ARM + '/llvm-gcc-4.2/include/c++/4.2.1',
    '-isystem',
    BASE_ARM + '/llvm-gcc-4.2/include/c++/4.2.1/arm-none-linux-gnueabi',
    '-isystem',
    BASE_ARM + '/llvm-gcc-4.2/arm-none-linux-gnueabi/include',
    # NOTE: order important
    # '-isystem',
    # BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem',
    BASE + '/arm-newlib/arm-none-linux-gnueabi/include',
    # '-isystem',
    #  BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include',
  ],


  'LLC_SHARED_ARM': [
    '-march=arm',
    # c.f. lib/Target/ARM/ARMGenSubtarget.inc
    '-mcpu=cortex-a8',
    '-mattr=-neon',
    '-mattr=+vfp3',
    '-mtriple=armv7a-*-*eabi*',
    '-arm-reserve-r9',
  ],


  'LLC_SHARED_X8632': [
    '-march=x86',
    '-mcpu=pentium4',
  ],



  'LLC_SHARED_X8664': [
    '-march=x86-64',
    '-mcpu=core2',
  ],


  'LLC_SFI_SANDBOXING': [
    # The following options might come in hand and are left
    # here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float ',
    #     '-aeabi-calls '
    #     '-sfi-zero-mask',
    '-sfi-cp-fudge',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    '-sfi-cp-fudge-percent=75',
    '-sfi-store',
    '-sfi-stack',
    '-sfi-branch',
    '-sfi-data',
    '-no-inline-jumptables'
  ],


  'OPT': [
    '-O3',
    '-std-compile-opts'
  ],


    # Our linker scripts ensure that the section layout follows the nacl abi
  'LD_ARM': [
    '--native-client',
    '-nostdlib',
    '-T', LD_SCRIPT_ARM,
    '-static',
  ],


  'LD_X8632': [
    '--native-client',
    '-nostdlib',
    '-T', LD_SCRIPT_X8632,
#    '-melf_nacl',
    '-static',
  ],



  'LD_X8664': [
    '--native-client',
    '-nostdlib',
    '-T', LD_SCRIPT_X8664,
    '-static',
  ],
}

######################################################################
# Executables invoked by this driver
######################################################################

LLVM_GCC = BASE_ARM + '/llvm-gcc-4.2/bin/llvm-gcc'

LLVM_GXX = BASE_ARM + '/llvm-gcc-4.2/bin/llvm-g++'

LLC_ARM = BASE_ARM + '/llvm/bin/llc'

LLC_SFI_ARM = BASE_ARM + '/llvm/bin/llc'

# NOTE: this is currently a user provided binary
LLC_SFI_X8632 = BASE + '/llc-x86-32-sfi'

# NOTE: this is currently a user provided binary
LLC_SFI_X8664 = BASE + '/llc-x86-64-sfi'

LLVM_LINK = BASE_ARM + '/llvm/bin/llvm-link'

LLVM_LD = BASE_ARM + '/llvm/bin/llvm-ld'

OPT = BASE_ARM + '/llvm/bin/opt'

AS_ARM = BASE_ARM + '/llvm-gcc-4.2/bin/arm-none-linux-gnueabi-as'

# NOTE: hack, assuming presence of x86/32 toolchain (used for both 32/64)
AS_X86 = BASE + '/../linux_x86/sdk/nacl-sdk/bin/nacl64-as'

ELF_LD = BASE_ARM + '/llvm-gcc-4.2/bin/arm-none-linux-gnueabi-ld'

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

  Run([OPT] +
      global_config_flags['OPT'] +
      [filename + '.bc', '-f', '-o', filename + '.opt.bc'])

  if mode == 'sfi':
    llc = [LLC_SFI_ARM] + global_config_flags['LLC_SFI_SANDBOXING']
  else:
    llc = [LLC_ARM]
  llc += global_config_flags['LLC_SHARED_ARM']
  llc += ['-f', filename + '.opt.bc', '-o', filename + '.s']
  Run(llc)

  # we use llvm-gcc since it knows the correct mfpu and march to use
  Run([LLVM_GCC] + global_config_flags['LLVM_GCC_COMPILE']
      + [filename + '.s', '-c', '-o', filename])

def FindObjectFilePos(argv):
  """Return argv index if there is and object file is being generated
  by the commandline argv. Both .o and .bc count as object files.
  Return None if no object file is generated.
  """
  if '-c' not in argv:
    return None

  if '-o' not in argv:
    return None

  pos = 1 + argv.index('-o')
  assert pos < len(argv)

  if argv[pos].endswith('.o') or argv[pos].endswith('.bc'):
    return pos

  return None



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

  if argv[pos].endswith('.o') or argv[pos].endswith('.bc'):
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
  Assemble(AS_ARM, global_config_flags['AS_ARM'], argv)


def AssembleX8632(argv):
  Assemble(AS_X86, global_config_flags['AS_X8632'], argv)


def AssembleX8664(argv):
  Assemble(AS_X86, global_config_flags['AS_X8664'], argv)


def Compile(argv, llvm_binary, mode):
  """ Compile to .o file."""
  argv[0] = llvm_binary

  if FindLinkPos(argv):
    # NOTE: this happens when using the toolchain builder scripts
    LogInfo('found .o -> exe compilation')
    Run(argv)

  obj_pos = FindObjectFilePos(argv)

  if obj_pos is None:
    if IsDiagnosticMode(argv):
      #LogWarning('diagnostic mode:' + StringifyCommand(argv))
      Run(argv)
    else:
      LogFatal('weird invocation without .o:' + StringifyCommand(argv))
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
    argv += global_config_flags['LLVM_GCC_COMPILE']
  else:
    argv += global_config_flags['LLVM_GCC_COMPILE']
    argv += global_config_flags['LLVM_GCC_COMPILE_HEADERS']

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


def Incarnation_cppasx8664(argv):
  AssembleX8664(argv)


def Incarnation_nop(argv):
  LogInfo('\nIGNORING: ' + StringifyCommand(argv))


def Incarnation_illegal(argv):
  LogFatal('illegal command ' + StringifyCommand(argv))


def MassageFinalLinkCommandArm(args):
  out = global_config_flags['LD_ARM']

  # add init code
  if '-nostdlib' not in args:
    out.append(LIBDIR_ARM_2 + '/crt1.o')
    out.append(LIBDIR_ARM_2 + '/crti.o')
    out.append(LIBDIR_ARM_2 + '/nacl_startup.o')
    out.append(LIBDIR_ARM_2 + '/intrinsics.o')

  out += args

  # add fini code
  if '-nostdlib' not in args:
    # NOTE: there is a circular dependency between libgcc and libc: raise()
    out.append(LIBDIR_ARM_2 + '/crtn.o')
    out.append('-L' + LIBDIR_ARM_3)
    out.append('-lstdc++')
    out.append('-L' + LIBDIR_ARM_1)
    # NOTE: bad things happen if ''-lc' is not first
    out.append('--start-group')
    out.append('-lc')
    out.append('-lnacl')
    out.append('-lgcc')
    out.append('-lnosys')
    out.append('-lcrt_platform')
    out.append('--end-group')
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
    out.append('-lc')
  return out


MAGIC_OBJS = set([
    # special hack for tests/syscall_return_sandboxing
    'sandboxed_x86_32.o',
    'sandboxed_x86_64.o',
    'sandboxed_arm.o',
    ])


DROP_ARGS = set([])


NATIVE_ARGS = set(['-nostdlib'])

def GenerateCombinedBitcodeFile(argv):
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
      args_bit_ld.append(a)
    elif a.startswith('-o$'):
      tokens = a.split('$')
      output = tokens[1]
      args_native_ld += tokens
    else:
      LogFatal('Unexpected ld arg: %s' % a)

  assert output

  # NOTE: LLVM_LD automagically appends .bc to the output
  if '-nostdlib' not in argv:
    if last_bitcode_pos != None:
        # Splice in the extra symbols.
        args_bit_ld = ([PNACL_BITCODE_ROOT + "/nacl_startup.o"] +
                       args_bit_ld[:last_bitcode_pos] +
                       args_bit_ld[last_bitcode_pos:] +
                       [# NOTE: bad things happen when '-lc' is not first
                        '-lc',
                        '-lnacl',
                        '-lstdc++',
                        '-lnosys',
                        ])

  # NOTE:.bc will be appended to the output name by LLVM_LD
  # NOTE:These are to insure that dependencies for libgcc.a are satified
  #      c.f. http://code.google.com/p/nativeclient/issues/detail?id=639
  public_functions = ['environ',
                      'memset',
                      'abort',
                      'raise',
                      ]

  # if we are not in barebones mode keep some symbols other than main
  # alive which are called form crtX.o

  if '-nostdlib' not in argv:
    args_bit_ld += [
       '-internalize-public-api-list=' + ','.join(public_functions),
       '-referenced-list=' + ','.join(public_functions),
       # NOTE: without this we still get a few miscompiles for pnacl-x86-32.
       # (LLVM's inliner will be more aggressives when functions are not
       # internalized.)
       '-disable-internalize',
       ]
  else:
     args_bit_ld += ['-internalize-public-api-list=_start',
                     # NOTE: without this llvm will be able
                     # to evaluate too much at compile time
                     # TODO(robertm): make all tests depend
                     # on argc to fool llvm
                     '-disable-internalize',
                     ]

  Run([LLVM_LD] + args_bit_ld +
       ['-o', output])



  return output, args_native_ld


def BitcodeToNative(argv, llc, llc_flags, ascom, as_flags, ld, ld_flags, root):
  """The ld step for bitcode is quite elaborate:
     1) Run llc to convert to .s
     2) Run as to convert to .o
     3) Run ld
  """
  output, args_native_ld = GenerateCombinedBitcodeFile(argv)

  bitcode_combined = output + ".bc"
  asm_combined = output + ".bc.s"
  obj_combined = output + ".bc.o"

  if not os.path.isfile(llc):
    LogFatal('You must create a symlink "%s" to your llc executable' % llc)

  Run([llc] + llc_flags + ['-f', bitcode_combined, '-o', asm_combined])

  Run([ascom] + as_flags + [asm_combined, '-o', obj_combined])

  args_native_ld = MassageFinalLinkCommandPnacl([obj_combined] + args_native_ld,
                                                root,
                                                ld_flags)
  Run([ld] +  args_native_ld)
  return output


def Incarnation_bcldarm(argv):
  output = BitcodeToNative(argv,
                           LLC_SFI_ARM,
                           global_config_flags['LLC_SHARED_ARM'] +
                           global_config_flags['LLC_SFI_SANDBOXING'],
                           AS_ARM,
                           global_config_flags['AS_ARM'],
                           ELF_LD,
                           global_config_flags['LD_ARM'],
                           PNACL_ARM_ROOT)


def Incarnation_bcldx8632(argv):
  output = BitcodeToNative(argv,
                           LLC_SFI_X8632,
                           global_config_flags['LLC_SHARED_X8632'],
                           AS_X86,
                           global_config_flags['AS_X8632'],
                           ELF_LD,
                           global_config_flags['LD_X8632'],
                           PNACL_X8632_ROOT)


def Incarnation_bcldx8664(argv):
  output = BitcodeToNative(argv,
                           LLC_SFI_X8664,
                           global_config_flags['LLC_SHARED_X8664'],
                           AS_X86,
                           global_config_flags['AS_X8664'],
                           ELF_LD,
                           global_config_flags['LD_X8664'],
                           PNACL_X8664_ROOT)


def Incarnation_sfild(argv):
  """Run the regular linker and then patch the ABI"""
  pos = FindLinkPos(argv)
  assert pos
  output = argv[pos]
  extra = []
  Run([ELF_LD] +  MassageFinalLinkCommandArm(extra + argv[1:]))

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
   'llvm-fake-bcld-x86-64': Incarnation_bcldx8664,

   'llvm-fake-cppas-arm': Incarnation_cppasarm,
   'llvm-fake-cppas-x86-32': Incarnation_cppasx8632,
   'llvm-fake-cppas-x86-64': Incarnation_cppasx8664,

   'llvm-fake-illegal': Incarnation_illegal,
   'llvm-fake-nop': Incarnation_nop,
   }


def main(argv):
  global VERBOSE
  # NOTE: we do not hook onto the "--v" flags since it may have other uses
  #       in connection with bootstrapping the gcc toolchain
  for pos, arg in enumerate(argv):
    if arg == '--pnacl-driver-verbose':
      VERBOSE = 1
      del argv[pos]

  # mechanism to overwrite some global settings, e.g.:
  #  --pnacl-driver-set-AS_X8632=-a,-b,3
  # NOTE: this currently is lacking a proper escaping mechanism for commas
  global global_config_flags
  for pos, arg in enumerate(argv):
    electric_prefix = '--pnacl-driver-set-'
    if arg.startswith(electric_prefix):
      tag, value = arg.split('=', 1)
      tag = tag[len(electric_prefix):]
      if ',' in value:
        value = value.split(',')
      assert tag in global_config_flags
      global_config_flags[tag] = value
      del argv[pos]

  LogInfo('\nRUNNNG\n ' + StringifyCommand(argv))
  basename = os.path.basename(argv[0])
  if basename not in INCARNATIONS:
    LogFatal("unknown command incarnation: " + StringifyCommand(argv))


  INCARNATIONS[basename](argv)
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv))
