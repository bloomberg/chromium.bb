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


######################################################################
# User-configurable Script Settings
#

LOG_FILE_ENABLE = 1
LOG_FILE_NAME = 'toolchain/hg-log/llvm-wrapper.log'
LOG_SIZE_LIMIT = 20*(2**20)     # 20 MB

PRETTY_PRINT_COMMANDS = 1


######################################################################
# This script resides in directory:
# native_client/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi
# BASE is "native_client/toolchain/linux_arm-untrusted"
BASE = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0])))

# BASE_NACL is "native_client" directory.
BASE_NACL = os.path.dirname(os.path.dirname(BASE))

assert os.path.basename(BASE) == "linux_arm-untrusted"
assert os.path.basename(BASE_NACL) == "native_client"

# It's not safe to output debug information to stderr, because "configure"
# runs this script and parses the output.
LOG_OUT = []
ERROR_OUT = [sys.stderr]

######################################################################
# Misc
######################################################################

BASE_ARM = BASE + '/arm-none-linux-gnueabi'

LD_SCRIPT_ARM = BASE_ARM + '/ld_script_arm_untrusted'

GOLD_PLUGIN = BASE_ARM + '/lib/LLVMgold.so'

# NOTE: derived from
# toolchain/linux_x86/nacl64/lib/ldscripts/elf_nacl.x
LD_SCRIPT_X8632 = BASE_NACL + '/tools/llvm/ld_script_x8632_untrusted'
# NOTE: derived from
# toolchain/linux_x86/nacl64/lib/ldscripts/elf64_nacl.x
LD_SCRIPT_X8664 = BASE_NACL + '/tools/llvm/ld_script_x8664_untrusted'

# arm libstdc++
LIBDIR_ARM_3 = BASE_ARM + '/lib'

# arm startup code + libs (+ bitcode when in bitcode mode)
LIBDIR_ARM_2 = BASE + '/arm-newlib/arm-none-linux-gnueabi/lib'

# arm libgcc
LIBDIR_ARM_1 = BASE_ARM + '/lib/gcc/arm-none-linux-gnueabi/4.2.1/'

PNACL_ARM_ROOT =  BASE + '/libs-arm'

PNACL_X8632_ROOT = BASE + '/libs-x8632'

PNACL_X8664_ROOT = BASE + '/libs-x8664'

global_pnacl_roots = {
  'arm' : PNACL_ARM_ROOT,
  'x86-32' : PNACL_X8632_ROOT,
  'x86-64' : PNACL_X8664_ROOT
}

global_pnacl_triples = {
  'arm' : 'armv7a-none-linux-gnueabi',
  'x86-32' : 'i686-none-linux-gnu',
  'x86-64' : 'x86_64-none-linux-gnu'
}

PNACL_BITCODE_ROOT = BASE + '/libs-bitcode'

######################################################################
# FLAGS (can be overwritten by
######################################################################
arm_flags = ['-mfpu=vfp3',
             '-march=armv7-a']
cpp_flags = ['-D__native_client__=1']

global_config_flags = {
  'CPP_FLAGS' : cpp_flags,
  'LLVM_GCC_COMPILE': [
    '-nostdinc',
    '-DNACL_TARGET_ARCH=arm',
    # TODO: get rid of the next two lines
    '-DNACL_TARGET_SUBARCH=32',
    '-DNACL_LINUX=1'
  ] + cpp_flags,

  'LLVM_GCC_COMPILE_HEADERS': [
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowloedge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem',
    BASE_ARM + '/lib/gcc/arm-none-linux-gnueabi/4.2.1/include',
    '-isystem',
    BASE_ARM + '/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include',
    '-isystem', BASE_ARM + '/arm-none-linux-gnueabi/include/c++/4.2.1',
    '-isystem', BASE_ARM + '/arm-none-linux-gnueabi/include/c++/4.2.1/arm-none-linux-gnueabi',
    '-isystem', BASE_ARM + '/arm-none-linux-gnueabi/include',
    # NOTE: order important
    # '-isystem',
    # BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem',
    BASE + '/arm-newlib/arm-none-linux-gnueabi/include',
  ],

  'OPT': [
    '-O3',
    '-std-compile-opts'
  ],

  'AS': {
    'arm' : arm_flags,
    'x86-32': [
      '--32',
      '--nacl-align', '5',
      # turn off nop
      '-n',
      '-march=pentium4',
      '-mtune=i386',
      ],
    'x86-64': [
      '--64',
      '--nacl-align', '5',
      # turn off nop
      '-n',
      '-mtune=core2',
      ]
    },

  'LLC': {
    'arm': [
      '-march=arm',
      # c.f. lib/Target/ARM/ARMGenSubtarget.inc
      '-mcpu=cortex-a8',
      '-mattr=-neon',
      '-mattr=+vfp3',
      '-arm-reserve-r9',
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
    'x86-32': [
      '-march=x86',
      '-mcpu=pentium4',
      ],
    'x86-64': [
      '-march=x86-64',
      '-mcpu=core2',
      ],
    },

  'LD': {
    'arm': [
      '--native-client',
      '-nostdlib',
      '-T', LD_SCRIPT_ARM,
      '-static',
      ],
    'x86-32': [
      '--native-client',
      '-nostdlib',
      '-T', LD_SCRIPT_X8632,
      #    '-melf_nacl',
      '-static',
      ],
    'x86-64': [
      '--native-client',
      '-nostdlib',
      '-T', LD_SCRIPT_X8664,
      '-static',
      ],
    }
}

######################################################################
# Executables invoked by this driver
######################################################################

LLVM_GCC = BASE_ARM + '/bin/arm-none-linux-gnueabi-gcc'

LLC = BASE_ARM + '/bin/llc'

OPT = BASE_ARM + '/bin/opt'

AS_ARM = BASE_ARM + '/bin/arm-none-linux-gnueabi-as'

# NOTE: hack, assuming presence of x86/32 toolchain (used for both 32/64)
AS_X86 = BASE_NACL + '/toolchain/linux_x86/bin/nacl64-as'

ELF_LD = BASE_ARM + '/bin/arm-none-linux-gnueabi-ld'

global_assemblers = {
  'arm' : AS_ARM,
  'x86-32' : AS_X86,
  'x86-64' : AS_X86
}

######################################################################
# Code
######################################################################

def AddLogFile(filename):
    global LOG_OUT
    if os.path.isfile(filename) and os.path.getsize(filename) > LOG_SIZE_LIMIT:
        mode = 'w'
    else:
        mode = 'a'
    try:
        fp = open(filename, mode)
    except Exception:
        return
    LOG_OUT.append(fp)

def LogStart():
    for o in LOG_OUT:
      print >> o, '-'*60

def LogInfo(m):
    for o in LOG_OUT:
      print >> o, m

def LogFatal(m, ret=-1):
  for o in LOG_OUT + ERROR_OUT:
    print >> o, 'FATAL:', m
    print >> o, ''
  sys.exit(ret)

def StringifyCommand(a):
  global PRETTY_PRINT_COMMANDS
  if PRETTY_PRINT_COMMANDS:
    return PrettyStringify(a)
  else:
    return SimpleStringify(a)

def SimpleStringify(args):
  return " ".join(args)

def PrettyStringify(args):
  ret = ''
  grouping = 0
  for a in args:
    if grouping == 0 and len(ret) > 0:
      ret += " \\\n    "
    elif grouping > 0:
      ret += " "
    if grouping == 0:
      grouping = 1
      if a.startswith('-') and len(a) == 2:
        grouping = 2
    ret += a
    grouping -= 1
  return ret

def Run(args):
  "Run the commandline give by the list args system()-style"
  LogInfo('\n' + StringifyCommand(args))
  try:
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
    buf_stdout, buf_stderr = p.communicate()
    ret = p.returncode
  except Exception, e:
    buf_stdout = ''
    buf_stderr = ''
    LogFatal('failed (%s) to run: ' % str(e) + StringifyCommand(args))

  sys.stdout.write(buf_stdout)
  sys.stderr.write(buf_stderr)

  if ret:
    LogFatal('failed command: ' + StringifyCommand(args) + '\n' +
             'stdout        :\n' + buf_stdout + '\n' +
             'stderr        :\n' + buf_stderr + '\n', ret)


def OutputName(argv):
  obj_pos = FindObjectFilePos(argv)
  if obj_pos:
    return argv[obj_pos]
  if '-c' not in argv:
    LogFatal('could not determine the output file: %s' %
             StringifyCommand(argv))
  src_pos = FindSourceFilePos(argv)
  if src_pos is None:
    LogFatal('could not determine the output file: %s' %
             StringifyCommand(argv))
  src_file = os.path.basename(argv[src_pos])
  return src_file.rsplit('.',1)[0] + '.o'


def SfiCompile(argv, arch, assembler, as_flags):
  "Run llc and the assembler to convert a bitcode file to ELF."
  filename = OutputName(argv)

  Run([OPT] +
      global_config_flags['OPT'] +
      [filename + '.bc', '-f', '-o', filename + '.opt.bc'])

  triple=global_pnacl_triples[arch]
  llc = [LLC] + global_config_flags['LLC'][arch] + ['-mtriple=%s' % triple]
  llc += [ filename + '.opt.bc', '-o', filename + '.s']
  Run(llc)

  Assemble(assembler, as_flags,
           [argv[0], '-c', filename + '.s', '-o', filename], False)

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


def FindSourceFilePos(argv):
  """Return the argv index of the source file.
  Return None if no source file is found.
  """
  for n, a in enumerate(argv):
    if a.endswith('.c') or a.endswith('.cc'):
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
  return False


def Assemble(asm, flags, argv, preprocess=True):
  # Some assumptions this functions has:
  # * There is one assembly file in argv.
  # * There is one output object (-o foo.o) in argv.
  # * Argv is a call to a gcc like tool.
  # This function works by patching the command line to run only the
  # preprocessor and then runs the assebler manually.
  # TODO(robertm): We also might want to pass some options through to the
  #                assembler.
  argv = argv + ['-E'] + global_config_flags['CPP_FLAGS']

  s_file = FindAssemblerFilePos(argv)
  if s_file is None:
     LogFatal('cannot find assembler source  file ' + StringifyCommand(argv))
  else:
    s_file = argv[s_file]

  obj_file_i = FindObjectFilePos(argv)
  if obj_file_i is None:
    LogFatal('cannot find object file ' + StringifyCommand(argv))
  else:
    obj_file = argv[obj_file_i]

  cpp_file = obj_file + ".cpp"
  argv[obj_file_i] = cpp_file

  if preprocess:
    Run(argv)
    Run([asm] + flags + ['-o', obj_file, cpp_file])
  else:
    Run([asm] + flags + ['-o', obj_file, s_file])


def CompileToBC(argv, llvm_binary, temp = False):
  """ Compile to .bc file."""
  argv = argv[:]
  argv[0] = llvm_binary

  if FindLinkPos(argv):
    # NOTE: this happens when using the toolchain builder scripts
    LogInfo('found .o -> exe compilation')
    Run(argv)

  obj_pos = FindObjectFilePos(argv)

  if temp:
    if obj_pos is None:
      argv += ['-o', OutputName(argv) + '.bc']
    else:
      argv[obj_pos] += '.bc'

  argv.append('--emit-llvm')
  argv.append('-fno-expand-va-arg')
  Run(argv)

def ExtractArch(argv):
  if '-arch' in argv:
    i = argv.index('-arch')
    return argv[i + 1], argv[:i] + argv[i + 2:]
  else:
    return None, argv[:]


def Incarnation_gcclike(argv):
  arch, argv = ExtractArch(argv)

  argv[0] = LLVM_GCC
  if IsDiagnosticMode(argv):
    Run(argv)
    return

  if '-nostdinc' in argv:
    argv += global_config_flags['LLVM_GCC_COMPILE']
  else:
    argv += global_config_flags['LLVM_GCC_COMPILE']
    argv += global_config_flags['LLVM_GCC_COMPILE_HEADERS']

  if '-E' in argv:
    Run(argv)
    return

  if '-emit-llvm' in argv:
    CompileToBC(argv, LLVM_GCC)
    return

  assert arch

  assembler = global_assemblers[arch]
  as_flags = global_config_flags['AS'][arch]

  if FindAssemblerFilePos(argv):
    Assemble(assembler, as_flags, argv)
    return

  CompileToBC(argv, LLVM_GCC, True)
  SfiCompile(argv, arch, assembler, as_flags)


def Incarnation_nop(argv):
  LogInfo('\nIGNORING: ' + StringifyCommand(argv))


def Incarnation_illegal(argv):
  LogFatal('illegal command ' + StringifyCommand(argv))


def MassageFinalLinkCommandPnacl(args, native_dir, flags):
  out = flags[:]

  # add init code
  if '-nostdlib' not in args:
    out.append(native_dir + '/crt1.o')
    out.append(native_dir + '/crti.o')
    out.append(native_dir + '/crtbegin.o')

  out += args

  # add fini code
  if '-nostdlib' not in args:
    # NOTE: there is a circular dependency between libgcc and libc: raise()
    out.append(native_dir + '/libcrt_platform.a')

    # c.f.: http://code.google.com/p/nativeclient/issues/detail?id=806
    # out.append(native_dir + '/crtend.o')
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

def GoldBCArgv(argv):
  # remove the script
  T_index = argv.index('-T')
  argv = argv[:T_index] + argv[T_index + 2:]

  # TODO(espindola): We should use also-emit-llvm for sanity checking that
  # we have included all that we need in the link.
  # add the -plugin and -plugin-opt options
  ret = argv + ['-plugin=%s' % GOLD_PLUGIN,
                '-plugin-opt=emit-llvm']
  return ret

def GenerateCombinedBitcodeFile(argv, arch):
  """Run gold to produce a single bitcode file
  Returns:
  name of resulting bitcode file without .bc extension.

  """
  args_bit_ld = []
  args_native_ld = []
  last_bitcode_pos = None
  output = None
  last = None
  ld_flags = global_config_flags['LD'][arch]
  root = global_pnacl_roots[arch]

  for a in argv[1:]:
    if last:
      # combine current item with prev
      a = last + '$' + a
      last = None

    if a in ['-o', '-T']:
      # combine two arg items into one arg
      last = a
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
    elif a == '-nostdlib':
      args_native_ld.append(a)
      args_bit_ld.append(a)
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

  if '-nostdlib' not in argv:
    if last_bitcode_pos != None:
      args_bit_ld = ([PNACL_BITCODE_ROOT + "/nacl_startup.o"] +
                     args_bit_ld +
                     ['-lc',
                      '-lnacl',
                      '-lstdc++',
                      '-lc',
                      '-lnosys',
                      ])

  args_bit_ld += ['-o', output + '.bc']

  # add init and fini
  args_bit_ld = MassageFinalLinkCommandPnacl(args_bit_ld, root, ld_flags)

  # add the plugin arguments
  args_bit_ld = GoldBCArgv(args_bit_ld)

  Run([ELF_LD] +  args_bit_ld)
  return output, args_native_ld


def BitcodeToNative(argv, arch):
  """The ld step for bitcode is quite elaborate:
     1) Run llc to convert to .s
     2) Run as to convert to .o
     3) Run ld
  """

  llc_flags = global_config_flags['LLC'][arch]
  ascom = global_assemblers[arch]
  as_flags = global_config_flags['AS'][arch]
  root = global_pnacl_roots[arch]
  ld_flags = global_config_flags['LD'][arch]

  output, args_native_ld = GenerateCombinedBitcodeFile(argv, arch)

  bitcode_combined = output + ".bc"
  asm_combined = output + ".bc.s"
  obj_combined = output + ".bc.o"

  if not os.path.isfile(LLC):
    LogFatal('You must create a symlink "%s" to your llc executable' % LLC)

  Run([LLC] + llc_flags + [bitcode_combined, '-o', asm_combined])

  Run([ascom] + as_flags + [asm_combined, '-o', obj_combined])

  args_native_ld = MassageFinalLinkCommandPnacl([obj_combined] + args_native_ld,
                                                root,
                                                ld_flags)
  Run([ELF_LD] +  args_native_ld)
  return output


def Incarnation_bcld_generic(argv):
  arch, argv = ExtractArch(argv)
  output = BitcodeToNative(argv, arch)

def Incarnation_bc_final(argv):
  arch, argv = ExtractArch(argv)
  # second return value is ignored
  output, args_native_ld = GenerateCombinedBitcodeFile(argv, arch)

######################################################################
# Dispatch based on name the scripts is invoked with
######################################################################

INCARNATIONS = {
   # sfigcc and sfig++ are only used for compiling and assembling, so the gcc
   # behavior is the same as the g++ behavior.
   'llvm-wrapper-sfigcc': Incarnation_gcclike,
   'llvm-wrapper-sfig++': Incarnation_gcclike,

   'llvm-wrapper-bcld' : Incarnation_bcld_generic,

   # final bc i.e. translator ready
   'llvm-wrapper-bcfinal' : Incarnation_bc_final,

   'llvm-wrapper-illegal': Incarnation_illegal,
   'llvm-wrapper-nop': Incarnation_nop,
   }


def main(argv):
  global LOG_OUT
  global LLVM_GCC
  # NOTE: we do not hook onto the "--v" flags since it may have other uses
  #       in connection with bootstrapping the gcc toolchain
  new_argv = []
  for arg in argv:
    if arg == '--pnacl-driver-verbose':
      LOG_OUT.append(sys.stderr)
    elif arg.startswith('--driver='):
      LLVM_GCC = arg[len('--driver='):]
    else:
      new_argv.append(arg)
  argv = new_argv

  if LOG_FILE_ENABLE:
      AddLogFile(BASE_NACL + '/' + LOG_FILE_NAME)
  LogStart()

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
