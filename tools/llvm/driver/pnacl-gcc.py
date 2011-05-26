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

from driver_tools import *

EXTRA_ENV = {
  'ALLOW_TRANSLATE': '0',  # Allow .bc to be translated to .o (before linking).
                           # It doesn't normally make sense to do this.

  'ALLOW_NATIVE'   : '0',  # Allow native objects (.S,.s,.o) to be in the
                           # linker line for .pexe or .pso generation.
                           # It doesn't normally make sense to do this.
  'EMIT_LL'     : '1',    # Produce an intermediate .ll file
                          # Currently enabled for debugging.
  'BIAS'        : 'NONE', # This can be 'NONE', 'ARM', 'X8632', or 'X8664'.
                          # When not set to none, this causes the front-end to
                          # act like a target-specific compiler. This bias is
                          # currently needed while compiling llvm-gcc, newlib,
                          # and some scons tests.
                          # TODO(pdox): Can this be removed?
  'MC_DIRECT'   : '1',    # Use MC direct object emission instead of
                          # producing an intermediate .s file
  'USE_GXX'     : '0',    # 0 for gcc, 1 for g++.

  # Command-line options
  'GCC_MODE'    : '',     # '' (default), '-E', '-c', or '-S'
  'SHARED'      : '0',    # Produce a shared library
  'STDINC'      : '1',    # Include standard headers (-nostdinc sets to 0)
  'STDLIB'      : '1',    # Include standard libraries (-nostdlib sets to 0)
  'DIAGNOSTIC'  : '0',    # Diagnostic flag detected
  'STATIC'      : '${USE_GLIBC ? 0 : 1}', # -static (on by default for newlib)

  'INPUTS'      : '',    # Input files
  'OUTPUT'      : '',    # Output file
  'UNMATCHED'   : '',    # Unrecognized parameters

  # We need to remove the pre-existing ARM defines which appear
  # because we are using the ARM llvm-gcc frontend.
  # TODO(pdox): Prevent these from being defined by llvm-gcc in
  #             the first place by creating a custom target for PNaCl.
  'REMOVE_BIAS' : '-U__arm__ -U__APCS_32__ -U__thumb__ -U__thumb2__ ' +
                  '-U__ARMEB__ -U__THUMBEB__ -U__ARMWEL__ ' +
                  '-U__ARMEL__ -U__THUMBEL__ -U__SOFTFP__ ' +
                  '-U__VFP_FP__ -U__ARM_NEON__ -U__ARM_FP16 ' +
                  '-U__ARM_FP16__ -U__MAVERICK__ -U__XSCALE__ -U__IWMMXT__ ' +
                  '-U__ARM_EABI__',

  'BIAS_NONE'   : '${REMOVE_BIAS}',
  'BIAS_ARM'    : '',
  'BIAS_X8632'  : '${REMOVE_BIAS} ' +
                  '-D__i386__ -D__i386 -D__i686 -D__i686__ -D__pentium4__',
  'BIAS_X8664'  : '${REMOVE_BIAS} ' +
                  '-D__amd64__ -D__amd64 -D__x86_64__ -D__x86_64 -D__core2__',

  'OPT_LEVEL'   : '',
  'CC_FLAGS'    : '${OPT_LEVEL} -fuse-llvm-va-arg -Werror-portable-llvm ' +
                  '-nostdinc -DNACL_LINUX=1 -D__native_client__=1 ' +
                  '-D__pnacl__=1 ${BIAS_%BIAS%}',
  'CC_STDINC'   :
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowledge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem ${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
    '-isystem ' +
    '${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1 ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1/arm-none-linux-gnueabi ' +
    '-isystem ${BASE_ARM_INCLUDE} ' +
    # NOTE: order important
    # '-isystem',
    # BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem ${BASE}/arm-newlib/arm-none-linux-gnueabi/include',

  # ${LIBS} must come before ${LIBS_BC} so that the native glibc/libnacl
  # takes precedence over bitcode libc.
  'LD_FLAGS' : '${STATIC ? -static} ${SHARED ? -shared} ' +
               '-L${LIBS} -L${LIBS_BC}',

  # Library Strings
  'LIBMODE'          : '${USE_GLIBC ? glibc : newlib}',
  'EMITMODE'         : '${STATIC ? static : ${SHARED ? shared : dynamic}}',

  # Standard Library Directories
  'LIBS_BC'          : '${BASE}/libs-bitcode',

  'LIBS'             : '${LIBS_%ARCH%_%LIBMODE%}',
  'LIBS_ARM_newlib'  : '${BASE}/libs-arm-newlib',
  'LIBS_X8632_newlib': '${BASE}/libs-x8632-newlib',
  'LIBS_X8664_newlib': '${BASE}/libs-x8664-newlib',

  'LIBS_X8632_glibc': '${BASE}/libs-x8632-glibc',
  'LIBS_X8664_glibc': '${BASE}/libs-x8664-glibc',
  'LIBS_ARM_glibc'  : '${BASE}/libs-arm-glibc',

  'LD_ARGS' : '${STDLIB ? ${LD_ARGS_%LIBMODE%_%EMITMODE%}' +
                      ' : ${LD_ARGS_nostdlib}}',

  # This information is also in pnacl-ld.
  # This is currently needed here because we have to manually specify a linker
  # script for glibc linking.
  # TODO(pdox): Fix binutils so that the built-in linker scripts are correct.
  'EMUL'        : '${EMUL_%ARCH%}',
  'EMUL_ARM'    : 'armelf_nacl',
  'EMUL_X8632'  : 'elf_nacl',
  'EMUL_X8664'  : 'elf64_nacl',

  # ${ld_inputs} signifies where to place the objects and libraries
  # provided on the command-line.
  'LD_ARGS_nostdlib': '${ld_inputs}',

  'LD_ARGS_newlib_static':
    '-static ${LIBS}/crt1.o ${LIBS_BC}/nacl_startup.bc ${ld_inputs} ' +
    '--start-group -lc -lnacl ${LIBSTDCPP} ${LIBS}/libcrt_platform.a ' +
    '--end-group -lgcc_eh -lgcc',

  # The next three are copied verbatim from nacl-gcc
  'LD_ARGS_glibc_static':
    '-T ${LIBS}/${EMUL}.x.static ' +
    '${LIBS}/crt1.o ${LIBS}/crti.o ${LIBS}/crtbeginT.o ' +
    '${ld_inputs} ' +
    '--start-group -lgcc -lgcc_eh -lc -lnacl ${LIBSTDCPP} -lcrt_platform ' +
    '--end-group ${LIBS}/crtend.o ${LIBS}/crtn.o',

  'LD_ARGS_glibc_shared':
    '-T ${LIBS}/${EMUL}.xs ' +
    '--eh-frame-hdr -shared ${LIBS}/crti.o ${LIBS}/crtbeginS.o ' +
    '${ld_inputs} ' +
    '-lgcc --as-needed -lgcc_s --no-as-needed -lc -lnacl -lc -lcrt_platform ' +
    '-lc -lgcc --as-needed -lgcc_s --no-as-needed ' +
    '${LIBS}/crtendS.o ${LIBS}/crtn.o -rpath ${LIBS}',

  'LD_ARGS_glibc_dynamic':
    '-T ${LIBS}/${EMUL}.x ' +
    '--eh-frame-hdr ${LIBS}/crt1.o ${LIBS}/crti.o ' +
    '${LIBS}/crtbegin.o ${ld_inputs} ' +
    '-lgcc --as-needed -lgcc_s --no-as-needed ' +
    '-lc -lnacl -lc ${LIBSTDCPP} -lcrt_platform -lc -lgcc --as-needed ' +
    '-lgcc_s --no-as-needed ${LIBS}/crtend.o ${LIBS}/crtn.o -rpath ${LIBS}',

  'LIBSTDCPP'       : '${USE_GXX ? -lstdc++ }',

  'CC' : '${USE_GXX ? ${LLVM_GXX} : ${LLVM_GCC}}',

  'RUN_CC': '${CC} -emit-llvm ${mode} ${CC_FLAGS} ' +
            '${STDINC ? ${CC_STDINC}} ' +
            '${input} -o ${output}',

  'RUN_PP' : '${CC} -E ${CC_FLAGS} ${CC_STDINC} ' +
             '"${input}" -o "${output}"'
}
env.update(EXTRA_ENV)

def AddLDFlag(*args):
  env.append('LD_FLAGS', *args)

def AddCCFlag(*args):
  env.append('CC_FLAGS', *args)

CustomPatterns = [
  ( '--driver=(.+)',                   "env.set('CC', $0)\n"),
  ( '--pnacl-skip-ll',                 "env.set('EMIT_LL', '0')"),
  ( '--pnacl-gxx',                     "env.set('USE_GXX', '1')"),
  ( '--pnacl-allow-native',            "env.set('ALLOW_NATIVE', '1')"),
  ( '--pnacl-allow-translate',         "env.set('ALLOW_TRANSLATE', '1')"),
  ( '--pnacl-native-hack', "env.append('LD_FLAGS', '--pnacl-native-hack')\n"
                           "env.set('ALLOW_NATIVE', '1')"),
]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  ( '-nostdinc',       "env.set('STDINC', '0')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),

  # Ignore -fPIC. We automatically use PIC when translating shared objects.
  ( '-fPIC',           ''),

  # We must include -l, -Xlinker, and -Wl options into the INPUTS
  # in the order they appeared. This is the exactly behavior of gcc.
  # For example: gcc foo.c -Wl,--start-group -lx -ly -Wl,--end-group
  #
  ( '(-l.+)',             "env.append('INPUTS', $0)"),
  ( ('-Xlinker','(.*)'),  "env.append('INPUTS', '-Xlinker=' + $0)"),
  ( '(-Wl,.*)',           "env.append('INPUTS', $0)"),

  # This is currently only used for the front-end, but we may want to have
  # this control LTO optimization level as well.
  ( '(-O.+)',          "env.set('OPT_LEVEL', $0)"),

  ( '(-g)',                   AddCCFlag),
  ( '(-W.*)',                 AddCCFlag),
  ( '(-std=.*)',              AddCCFlag),
  ( '(-B.*)',                 AddCCFlag),
  ( '(-D.*)',                 AddCCFlag),
  ( '(-f.*)',                 AddCCFlag),
  ( ('(-I)', '(.+)'),         AddCCFlag),
  ( '(-I.+)',                 AddCCFlag),
  ( '(-pedantic)',            AddCCFlag),
  ( ('(-isystem)', '(.+)'),   AddCCFlag),
  ( '(-g.*)',                 AddCCFlag),
  ( '(-xassembler-with-cpp)', AddCCFlag),

  ( '-shared',                "env.set('SHARED', '1')"),
  ( '-static',                "env.set('STATIC', '1')"),

  ( ('(-L)','(.+)'),          AddLDFlag),
  ( '(-L.+)',                 AddLDFlag),
  ( '(-Wp,.*)',               AddCCFlag),

  ( '(-MMD)',                 AddCCFlag),
  ( '(-MP)',                  AddCCFlag),
  ( '(-MD)',                  AddCCFlag),
  ( ('(-MF)','(.*)'),         AddCCFlag),
  ( ('(-MT)','(.*)'),         AddCCFlag),

  # With -xc, GCC considers future inputs to be an input source file.
  # The following is not the correct handling of -x,
  # but it works in the limited case used by llvm/configure.
  # TODO(pdox): Make this -x really work in the general case.
  ( '-xc',                    "env.append('CC_FLAGS', '-xc')\n"
                              "SetForcedFileType('src')"),

  # Ignore these gcc flags
  ( '(-msse)',                ""),
  ( '(-march=armv7-a)',       ""),

  ( '(-s)',                   AddLDFlag),
  ( '(--strip-all)',          AddLDFlag),
  ( '(--strip-debug)',        AddLDFlag),

  # Ignore these assembler flags
  ( '(-Qy)',                  ""),
  ( ('(--traditional-format)', '.*'), ""),
  ( '(-gstabs)',              ""),
  ( '(--gstabs)',             ""),
  ( '(-gdwarf2)',             ""),
  ( '(--gdwarf2)',             ""),
  ( '(--fatal-warnings)',     ""),
  ( '(-meabi=.*)',            ""),
  ( '(-mfpu=.*)',             ""),

  # GCC diagnostic mode triggers
  ( '(-print-.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(--print.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(-dumpspecs)',           "env.set('DIAGNOSTIC', '1')"),
  ( '(-v|--version|--v|-V)',  "env.set('DIAGNOSTIC', '1')"),
  ( '(-d.*)',                 "env.set('DIAGNOSTIC', '1')"),

  # Catch all other command-line arguments
  ( '(-.*)',              "env.append('UNMATCHED', $0)"),

  # Input Files
  # Call ForceFileType for all input files at the time they are
  # parsed on the command-line. This ensures that the gcc "-x"
  # setting is correctly applied.

  ( '(.*)',                   "env.append('INPUTS', $0)\n"
                              "ForceFileType($0)"),
]


def main(argv):
  _, real_argv = ParseArgs(argv, CustomPatterns, must_match = False)
  ParseArgs(real_argv, GCCPatterns)

  # "configure", especially when run as part of a toolchain bootstrap
  # process, will invoke gcc with various diagnostic options and
  # parse the output. In these cases we do not alter the incoming
  # commandline. It is also important to not emit spurious messages.
  if env.getbool('DIAGNOSTIC'):
    RunWithLog(env.get('CC') + real_argv)
    return 0

  unmatched = env.get('UNMATCHED')
  if len(unmatched) > 0:
    UnrecognizedOption(*unmatched)

  use_glibc = env.getbool('USE_GLIBC')
  is_static = env.getbool('STATIC')
  is_shared = env.getbool('SHARED')

  if not use_glibc and (is_shared or not is_static):
    Log.Fatal("Can't produce dynamic objects with newlib. Did you want "
              "--pnacl-use-glibc ?")

  if env.getbool('STATIC') and env.getbool('SHARED'):
    Log.Fatal("Can't handle both -static and -shared")

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal('No input files')

  arch = GetArch()
  gcc_mode = env.getone('GCC_MODE')

  if env.getbool('SHARED'):
    bclink_output = 'pso'
    link_output = 'so'
  else:
    bclink_output = 'pexe'
    link_output = 'nexe'

  output_type_map = {
    ('-E', True) : 'pp',
    ('-E', False): 'pp',
    ('-c', True) : 'bc',
    ('-c', False): 'o',
    ('-S', True) : 'll',
    ('-S', False): 's',
    ('',   True) : bclink_output,
    ('',   False): link_output
  }

  output_type = output_type_map[(gcc_mode, arch is None)]
  NeedsLinking = (gcc_mode == '')

  # There are multiple input files and no linking is being done.
  # There will be multiple outputs. Handle this case separately.
  if not NeedsLinking:
    if output != '' and len(inputs) > 1:
      Log.Fatal('Cannot have -o with -c, -S, or -E and multiple inputs')

    for f in inputs:
      if f.startswith('-'):
        continue
      intype = FileType(f)
      if ((output_type == 'pp' and intype not in ('src','S')) or
          (output_type == 'll' and intype not in 'src') or
          (output_type == 'bc' and intype not in ('src','ll')) or
          (output_type == 's' and intype not in ('src','ll','bc','S')) or
          (output_type == 'o' and intype not in ('src','ll','bc','S','s'))):
        Log.Fatal("%s: Unexpected type of file for '%s'", f, gcc_mode)

      if output == '':
        f_output = DefaultOutputName(f, output_type)
      else:
        f_output = output

      namegen = TempNameGen([f], f_output)
      CompileOne(f, output_type, namegen, f_output)
    return 0

  # Linking case
  assert(NeedsLinking)
  assert(output_type in ('pso','so','pexe','nexe'))

  if output == '':
    output = 'a.out'
  namegen = TempNameGen(inputs, output)

  # Compile all source files (c/c++/ll) to .bc
  for i in xrange(0, len(inputs)):
    if inputs[i].startswith('-'):
      continue
    intype = FileType(inputs[i])
    if intype in ('src','ll'):
      inputs[i] = CompileOne(inputs[i], 'bc', namegen)

  # Compile all .s/.S to .o
  if env.getbool('ALLOW_NATIVE'):
    for i in xrange(0, len(inputs)):
      if inputs[i].startswith('-'):
        continue
      intype = FileType(inputs[i])
      if intype in ('s','S'):
        inputs[i] = CompileOne(inputs[i], 'o', namegen)

  # We should only be left with .bc and .o and libraries
  for f in inputs:
    if f.startswith('-'):
      continue
    intype = FileType(f)
    if intype in ('o','nlib','s','S') and not env.getbool('ALLOW_NATIVE'):
        Log.Fatal('%s: Native object files not allowed in link. '
                  'Use --pnacl-allow-native to override.', f)
    assert(intype in ('bc','o','bclib','nlib'))

  # Fix the user-specified linker arguments
  ld_inputs = []
  for f in inputs:
    if f.startswith('-Xlinker='):
      ld_inputs.append(f[len('-Xlinker='):])
    elif f.startswith('-Wl,'):
      ld_inputs += f[len('-Wl,'):].split(',')
    else:
      ld_inputs.append(f)

  # There's an architecture bias here, since we have to link
  # against native objects, even for bitcode linking.
  if GetArch() is None:
    env.set('ARCH', 'X8632')

  # Invoke the linker
  env.set('ld_inputs', *ld_inputs)
  ld_args = env.get('LD_ARGS')
  ld_flags = env.get('LD_FLAGS')
  RunDriver('pnacl-ld', ld_flags + ld_args + ['-o', output])
  return 0


def CompileOne(input, output_type, namegen, output = None):
  if output is None:
    output = namegen.TempNameForInput(input, output_type)

  chain = DriverChain(input, output, namegen)
  SetupChain(chain, FileType(input), output_type)
  chain.run()
  return output

def RunPP(input, output):
  RunWithEnv("${RUN_PP}", input=input, output=output)

def RunCC(input, output, mode):
  RunWithEnv("${RUN_CC}", input=input, output=output, mode=mode)

def RunLLVMAS(input, output):
  RunDriver('pnacl-as', [input, '-o', output], suppress_arch = True)

def RunNativeAS(input, output):
  RunDriver('pnacl-as', [input, '-o', output])

def RunTranslate(input, output, mode):
  if not env.getbool('ALLOW_TRANSLATE'):
    Log.Fatal('%s: Trying to convert bitcode to an object file before '
              'bitcode linking. This is supposed to wait until '
              'translation. Use --pnacl-allow-translate to override.', input)
  RunDriver('pnacl-translate', [mode, input, '-o', output])

def SetupChain(chain, input_type, output_type):
  assert(output_type in ('pp','ll','bc','s','o'))
  cur_type = input_type

  # src -> pp
  if cur_type == 'src' and output_type == 'pp':
    chain.add(RunPP, 'cpp')
    cur_type = 'pp'
  if cur_type == output_type:
    return

  # src -> ll
  if cur_type == 'src' and (env.getbool('EMIT_LL') or output_type == 'll'):
    chain.add(RunCC, 'll', mode='-S')
    cur_type = 'll'
  if cur_type == output_type:
    return

  # ll -> bc
  if cur_type == 'll':
    chain.add(RunLLVMAS, 'bc')
    cur_type = 'bc'
  if cur_type == output_type:
    return

  # src -> bc
  if cur_type == 'src' and output_type == 'bc':
    chain.add(RunCC, 'bc', mode='-c')
    cur_type = 'bc'
  if cur_type == output_type:
    return

  # bc -> o
  if env.getbool('MC_DIRECT') and cur_type == 'bc' and output_type == 'o':
    chain.add(RunTranslate, 'o', mode='-c')
    cur_type = 'o'
  if cur_type == output_type:
    return

  # bc -> s
  if cur_type == 'bc':
    chain.add(RunTranslate, 's', mode='-S')
    cur_type = 's'
  if cur_type == output_type:
    return

  # S -> s
  if cur_type == 'S':
    chain.add(RunPP, 's')
    cur_type = 's'
  if cur_type == output_type:
    return

  # s -> o
  if cur_type == 's' and output_type == 'o':
    chain.add(RunNativeAS, 'o')
    cur_type = 'o'
  if cur_type == output_type:
    return

  Log.Fatal("Unable to compile .%s to .%s", input_type, output_type)


if __name__ == "__main__":
  DriverMain(main)
