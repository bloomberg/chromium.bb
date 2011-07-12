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
  'ALLOW_TRANSLATE': '0',  # Allow bitcode translation before linking.
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
  'STATIC'      : '${LIBMODE_NEWLIB ? 1 : 0}', # -static (on for newlib)
  'PIC'         : '0',    # Generate PIC

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

  'OPT_LEVEL'   : '0',
  'CC_FLAGS'    : '-O${OPT_LEVEL} -fuse-llvm-va-arg -Werror-portable-llvm ' +
                  '-nostdinc -DNACL_LINUX=1 -D__native_client__=1 ' +
                  '-D__pnacl__=1 ${BIAS_%BIAS%}',


  'PREFIXES'        : '${PREFIXES_USER} ${PREFIXES_BUILTIN}',
  'PREFIXES_BUILTIN': '${STDLIB ? ' +
                      '${LIBS_SDK}/ ${LIBS_SDK_ARCH}/ ' +
                      '${LIBS_ARCH}/ ${LIBS_BC}/}',
  'PREFIXES_USER'   : '', # System prefixes specified by using the -B flag.

  'ISYSTEM'        : '${ISYSTEM_USER} ${ISYSTEM_BUILTIN}',
  'ISYSTEM_BUILTIN': '${STDINC ? ${ISYSTEM_%LIBMODE%}}',
  'ISYSTEM_USER'   : '',  # System include directories specified by
                          # using the -isystem flag.

  'ISYSTEM_newlib' :
    '${BASE_SDK}/include ' +
    '${BASE_LLVM_GCC}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
    '${BASE_LLVM_GCC}/' +
      'lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include ' +
    '${BASE_LIBSTDCPP}/include/c++/4.2.1 ' +
    '${BASE_LIBSTDCPP}/include/c++/4.2.1/arm-none-linux-gnueabi ' +
    '${BASE_INCLUDE} ' +
    '${BASE_NEWLIB}/arm-none-linux-gnueabi/include',

  'ISYSTEM_glibc' :
    '${BASE_SDK}/include ' +
    '${BASE_GLIBC}/include ' +
    '${BASE_LLVM_GCC}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
    '${BASE_LLVM_GCC}/' +
      'lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include ' +
    '${BASE_LIBSTDCPP}/include/c++/4.2.1 ' +
    '${BASE_LIBSTDCPP}/include/c++/4.2.1/arm-none-linux-gnueabi',

  # ${LIBS_ARCH} must come before ${LIBS_BC} so that the native glibc/libnacl
  # takes precedence over bitcode libc.
  'LD_FLAGS' : '-O${OPT_LEVEL} ${STATIC ? -static} ${SHARED ? -shared} ' +
               '${PIC ? -fPIC} ${@AddPrefix:-L:SEARCH_DIRS} ' +
               '${LIBMODE_GLIBC && ' +
                 '!STATIC ? ${@AddPrefix:-rpath-link=:SEARCH_DIRS}}',

  'SEARCH_DIRS'      : '${SEARCH_DIRS_USER} ${PREFIXES}',
  'SEARCH_DIRS_USER' : '', # Directories specified using -L

  # Library Strings
  'EMITMODE'         : '${STATIC ? static : ${SHARED ? shared : dynamic}}',

  # Standard Library Directories
  'LIBS_BC'          : '${BASE}/lib',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE}/lib-arm',
  'LIBS_X8632'       : '${BASE}/lib-x86-32',
  'LIBS_X8664'       : '${BASE}/lib-x86-64',

  'LIBS_SDK'         : '${BASE_SDK}/lib',

  'LIBS_SDK_ARCH'    : '${LIBS_SDK_%ARCH%}',
  'LIBS_SDK_X8632'   : '${BASE_SDK}/lib-x86-32',
  'LIBS_SDK_X8664'   : '${BASE_SDK}/lib-x86-64',
  'LIBS_SDK_ARM'     : '${BASE_SDK}/lib-arm',

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
  'LD_ARGS_nostdlib': '-barebones-link ${ld_inputs}',

  'LD_ARGS_newlib_static':
    '-static ${@FindObj:crt1.o} ${@FindObj:nacl_startup.bc} ${ld_inputs} ' +
    '--start-group -lgcc_eh -lgcc -lehsupport -lc -lnacl ' +
    '${LIBSTDCPP} ${@FindObj:libcrt_platform.a} --end-group',

  # The next three are copied verbatim from nacl-gcc
  'LD_ARGS_glibc_static':
    '-T ${@FindLinkerScript:.x.static} ' +
    '${@FindObj:crt1.o} ${@FindObj:crti.o} ${@FindObj:crtbeginT.o} ' +
    '${ld_inputs} ${LIBSTDCPP} ' +
    '--start-group -lgcc -lgcc_eh -lehsupport -lc ' +
    '--end-group ${@FindObj:crtend.o} ${@FindObj:crtn.o}',

  'LD_ARGS_glibc_shared':
    '-T ${@FindLinkerScript:.xs} ' +
    '--eh-frame-hdr -shared ${@FindObj:crti.o} ${@FindObj:crtbeginS.o} ' +
    '${ld_inputs} ${LIBSTDCPP} ' +
    '-lgcc -lgcc_eh -lc -lgcc -lgcc_eh ' +
    # When shared libgcc is ready, use this instead:
    # '-lgcc --as-needed -lgcc_s --no-as-needed ' +
    # '-lc -lgcc --as-needed -lgcc_s --no-as-needed ' +
    '${@FindObj:crtendS.o} ${@FindObj:crtn.o}',

  'LD_ARGS_glibc_dynamic':
    '-T ${@FindLinkerScript:.x} ' +
    '--eh-frame-hdr ${@FindObj:crt1.o} ${@FindObj:crti.o} ' +
    '${@FindObj:crtbegin.o} ${ld_inputs} ${LIBSTDCPP} ' +
    '-lgcc -lgcc_eh -lc -lgcc -lgcc_eh -lehsupport ' +
    # When shared libgcc is ready, use this instead:
    # '-lgcc --as-needed -lgcc_s --no-as-needed ' +
    # '-lc -lgcc --as-needed -lgcc_s --no-as-needed ' +
    '${@FindObj:crtend.o} ${@FindObj:crtn.o}',

  'LIBSTDCPP'       : '${USE_GXX ? -lstdc++ -lm }',

  'CC' : '${USE_GXX ? ${LLVM_GXX} : ${LLVM_GCC}}',

  'RUN_CC': '${CC} -emit-llvm ${mode} ${CC_FLAGS} ' +
            '${@AddPrefix:-isystem :ISYSTEM} ' +
            '${input} -o ${output}',

  'RUN_PP' : '${CC} -E ${CC_FLAGS} ${@AddPrefix:-isystem :ISYSTEM} ' +
             '"${input}" -o "${output}"'
}
env.update(EXTRA_ENV)

def AddLDFlag(*args):
  env.append('LD_FLAGS', *args)

def AddCCFlag(*args):
  env.append('CC_FLAGS', *args)

def AddBPrefix(prefix):
  prefix = pathtools.normalize(prefix)
  if pathtools.isdir(prefix) and not prefix.endswith('/'):
    prefix += '/'

  env.append('PREFIXES_USER', prefix)

  # Add prefix/include to isystem if it exists
  include_dir = prefix + 'include'
  if pathtools.isdir(include_dir):
    env.append('ISYSTEM_USER', include_dir)

CustomPatterns = [
  ( '--driver=(.+)',             "env.set('CC', pathtools.normalize($0))\n"),
  ( '--pnacl-skip-ll',           "env.set('EMIT_LL', '0')"),
  ( '--pnacl-gxx',               "env.set('USE_GXX', '1')"),
  ( '--pnacl-allow-native',      "env.set('ALLOW_NATIVE', '1')"),
  ( '--pnacl-allow-translate',   "env.set('ALLOW_TRANSLATE', '1')"),
  ( '--pnacl-native-hack', "env.append('LD_FLAGS', '--pnacl-native-hack')\n"
                           "env.set('ALLOW_NATIVE', '1')"),
]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  ( '-nostdinc',       "env.set('STDINC', '0')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),

  # We don't care about -fPIC, but pnacl-ld and pnacl-translate do.
  ( '-fPIC',           "env.set('PIC', '1')"),

  # We must include -l, -Xlinker, and -Wl options into the INPUTS
  # in the order they appeared. This is the exactly behavior of gcc.
  # For example: gcc foo.c -Wl,--start-group -lx -ly -Wl,--end-group
  #
  ( '(-l.+)',             "env.append('INPUTS', $0)"),
  ( ('-Xlinker','(.*)'),  "env.append('INPUTS', '-Xlinker=' + $0)"),
  ( '(-Wl,.*)',           "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',         "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',        "env.append('INPUTS', $0)"),

  ( '-O([0-4s])',         "env.set('OPT_LEVEL', $0)\n"),
  ( '-O',                 "env.set('OPT_LEVEL', 1)\n"),

  ( ('-isystem', '(.*)'),
                       "env.append('ISYSTEM_USER', pathtools.normalize($0))"),
  ( ('-I', '(.+)'),    "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),
  ( '-I(.+)',          "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),

  ( '(-g)',                   AddCCFlag),
  ( '(-W.*)',                 AddCCFlag),
  ( '(-std=.*)',              AddCCFlag),
  ( '(-D.*)',                 AddCCFlag),
  ( '(-f.*)',                 AddCCFlag),
  ( '(-pedantic)',            AddCCFlag),
  ( '(-g.*)',                 AddCCFlag),
  ( '(-xassembler-with-cpp)', AddCCFlag),

  ( '-shared',                "env.set('SHARED', '1')"),
  ( '-static',                "env.set('STATIC', '1')"),

  ( ('-B','(.*)'),            AddBPrefix),
  ( ('-B(.+)'),               AddBPrefix),

  ( ('-L','(.+)'), "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),
  ( '-L(.+)',      "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),

  ( '(-Wp,.*)',       AddCCFlag),
  ( '(-MMD)',         AddCCFlag),
  ( '(-MP)',          AddCCFlag),
  ( '(-MD)',          AddCCFlag),
  ( ('(-MT)','(.*)'), AddCCFlag),
  ( ('(-MF)','(.*)'), "env.append('CC_FLAGS', $0, pathtools.normalize($1))"),

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
  ( '(-v|--v|--version|-V)',  "env.set('DIAGNOSTIC', '1')"),
  ( '(-d.*)',                 "env.set('DIAGNOSTIC', '1')"),

  # Catch all other command-line arguments
  ( '(-.*)',              "env.append('UNMATCHED', $0)"),

  # Input Files
  # Call ForceFileType for all input files at the time they are
  # parsed on the command-line. This ensures that the gcc "-x"
  # setting is correctly applied.

  ( '(.*)',  "env.append('INPUTS', pathtools.normalize($0))\n"
             "ForceFileType(pathtools.normalize($0))"),
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

  libmode_newlib = env.getbool('LIBMODE_NEWLIB')
  is_static = env.getbool('STATIC')
  is_shared = env.getbool('SHARED')

  if libmode_newlib and (is_shared or not is_static):
    Log.Fatal("Can't produce dynamic objects with newlib.")

  if env.getbool('STATIC') and env.getbool('SHARED'):
    Log.Fatal("Can't handle both -static and -shared")

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal('No input files')

  # Was -arch specified?
  arch_flag_given = GetArch() is not None

  if not arch_flag_given:
    # There's an architecture bias here: ARCH needs to be set
    # since we may have to link against native objects during
    # bitcode linking.
    env.set('ARCH', 'X8632')
    env.set('ARCH_LOCKED', '0')

  gcc_mode = env.getone('GCC_MODE')

  if env.getbool('SHARED'):
    bclink_output = 'pso'
    link_output = 'so'
  else:
    bclink_output = 'pexe'
    link_output = 'nexe'

  output_type_map = {
    ('-E', False) : 'pp',
    ('-E', True): 'pp',
    ('-c', False) : 'po',
    ('-c', True): 'o',
    ('-S', False) : 'll',
    ('-S', True): 's',
    ('',   False) : bclink_output,
    ('',   True): link_output
  }

  output_type = output_type_map[(gcc_mode, arch_flag_given)]
  needs_linking = (gcc_mode == '')

  # There are multiple input files and no linking is being done.
  # There will be multiple outputs. Handle this case separately.
  if not needs_linking:
    if output != '' and len(inputs) > 1:
      Log.Fatal('Cannot have -o with -c, -S, or -E and multiple inputs')

    for f in inputs:
      if f.startswith('-'):
        continue
      intype = FileType(f)
      if ((output_type == 'pp' and intype not in ('src','S')) or
          (output_type == 'll' and intype not in 'src') or
          (output_type == 'po' and intype not in ('src','ll')) or
          (output_type == 's' and intype not in ('src','ll','po','S')) or
          (output_type == 'o' and intype not in ('src','ll','po','S','s'))):
        Log.Fatal("%s: Unexpected type of file for '%s'",
                  pathtools.touser(f), gcc_mode)

      if output == '':
        f_output = DefaultOutputName(f, output_type)
      else:
        f_output = output

      namegen = TempNameGen([f], f_output)
      CompileOne(f, output_type, namegen, f_output)
    return 0

  # Linking case
  assert(needs_linking)
  assert(output_type in ('pso','so','pexe','nexe'))

  if output == '':
    output = pathtools.normalize('a.out')
  namegen = TempNameGen(inputs, output)

  # Compile all source files (c/c++/ll) to .po
  for i in xrange(0, len(inputs)):
    if inputs[i].startswith('-'):
      continue
    intype = FileType(inputs[i])
    if intype in ('src','ll'):
      inputs[i] = CompileOne(inputs[i], 'po', namegen)

  # Compile all .s/.S to .o
  if env.getbool('ALLOW_NATIVE'):
    for i in xrange(0, len(inputs)):
      if inputs[i].startswith('-'):
        continue
      intype = FileType(inputs[i])
      if intype in ('s','S'):
        inputs[i] = CompileOne(inputs[i], 'o', namegen)

  # We should only be left with .po and .o and libraries
  for f in inputs:
    if f.startswith('-'):
      continue
    intype = FileType(f)
    if intype in ('o','nlib','s','S'):
      if not env.getbool('ALLOW_NATIVE'):
        Log.Fatal('%s: Native object files not allowed in link. '
                  'Use --pnacl-allow-native to override.', pathtools.touser(f))
      assert(intype in ('o','nlib'))
      # ARCH must match
      if IsELF(f):
        ArchMerge(f, True)
    assert(intype in ('po','o','bclib','nlib','so','pso','ldscript'))

  # Fix the user-specified linker arguments
  ld_inputs = []
  for f in inputs:
    if f.startswith('-Xlinker='):
      ld_inputs.append(f[len('-Xlinker='):])
    elif f.startswith('-Wl,'):
      ld_inputs += f[len('-Wl,'):].split(',')
    else:
      ld_inputs.append(f)

  # Invoke the linker
  env.set('ld_inputs', *ld_inputs)

  # Note: The evaluation of LD_ARGS may have the side effect of
  # changing the ARCH if it is not already locked. (See FindObj)
  ld_args = env.get('LD_ARGS')
  ld_flags = env.get('LD_FLAGS')

  RunDriver('pnacl-ld', ld_flags + ld_args + ['-o', output])
  return 0

@env.register
def FindLinkerScript(suffix):
  name = env.getone('EMUL') + suffix
  return FindObj(name)

@env.register
def FindObj(name):
  # This is needed because ${A ? B : C} always evaluates both B and C.
  # (See the definition of LD_ARG)
  # TODO(pdox): Fix the evaluator so that short-circuiting evaluation is used.
  if not env.getbool('STDLIB'):
    return name

  prefixes = env.get('PREFIXES')

  for prefix in prefixes:
    guess = prefix + name
    if pathtools.exists(guess):
      # If the file is ELF, check ARCH.
      if IsELF(guess):
        if not ArchMerge(guess, False):
          continue
      return guess

  Log.Fatal("Cannot find %s", name)

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
              'translation. Use --pnacl-allow-translate to override.',
              pathtools.touser(input))
  args = [mode, input, '-o', output]
  if env.getbool('PIC'):
    args += ['-fPIC']
  RunDriver('pnacl-translate', args)

def SetupChain(chain, input_type, output_type):
  assert(output_type in ('pp','ll','po','s','o'))
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

  # ll -> po
  if cur_type == 'll':
    chain.add(RunLLVMAS, 'po')
    cur_type = 'po'
  if cur_type == output_type:
    return

  # src -> po
  if cur_type == 'src' and output_type == 'po':
    chain.add(RunCC, 'po', mode='-c')
    cur_type = 'po'
  if cur_type == output_type:
    return

  # po -> o
  if env.getbool('MC_DIRECT') and cur_type == 'po' and output_type == 'o':
    chain.add(RunTranslate, 'o', mode='-c')
    cur_type = 'o'
  if cur_type == output_type:
    return

  # po -> s
  if cur_type == 'po':
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
