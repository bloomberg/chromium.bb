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
from driver_env import env
from driver_log import Log

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
                          # currently needed while compiling newlib,
                          # and some scons tests.
                          # TODO(pdox): Can this be removed?
  'MC_DIRECT'   : '1',    # Use MC direct object emission instead of
                          # producing an intermediate .s file
  'LANGUAGE'    : 'C',    # C or CXX
  'FRONTEND'    : '',     # CLANG, or DRAGONEGG

  # Command-line options
  'GCC_MODE'    : '',     # '' (default), '-E', '-c', or '-S'
  'SHARED'      : '0',    # Produce a shared library
  'STDINC'      : '1',    # Include standard headers (-nostdinc sets to 0)
  'STDLIB'      : '1',    # Include standard libraries (-nostdlib sets to 0)
  'DEFAULTLIBS' : '1',    # Link with default libraries
  'DIAGNOSTIC'  : '0',    # Diagnostic flag detected
  'STATIC'      : '0',    # -static
  'PIC'         : '0',    # Generate PIC

  'INPUTS'      : '',    # Input files
  'OUTPUT'      : '',    # Output file
  'UNMATCHED'   : '',    # Unrecognized parameters

  'BIAS_NONE'   : '',
  'BIAS_ARM'    : '-D__arm__ -D__ARM_ARCH_7A__ -D__ARMEL__',
  'BIAS_X8632'  : '-D__i386__ -D__i386 -D__i686 -D__i686__ -D__pentium4__',
  'BIAS_X8664'  : '-D__amd64__ -D__amd64 -D__x86_64__ -D__x86_64 -D__core2__',

  'OPT_LEVEL'   : '0',
  'CC_FLAGS'    : '-O${OPT_LEVEL} ' +
                  '-nostdinc -DNACL_LINUX=1 ${BIAS_%BIAS%} ' +
                  '${CC_FLAGS_%FRONTEND%}',
  'CC_FLAGS_CLANG': '-ccc-host-triple le32-unknown-nacl',
  'CC_FLAGS_DRAGONEGG': '-D__native_client__=1 -D__pnacl__=1 ' +
                        '-flto -fplugin=${DRAGONEGG_PLUGIN}',

  'ISYSTEM'        : '${ISYSTEM_USER} ${ISYSTEM_BUILTIN}',
  'ISYSTEM_BUILTIN': '${STDINC ? ${ISYSTEM_%LIBMODE%}}',
  'ISYSTEM_USER'   : '',  # System include directories specified by
                          # using the -isystem flag.


  'ISYSTEM_CLANG':
      '${BASE_LLVM}/lib/clang/3.1/include',

  # TODO(pdox): reference dragonegg instead of llvm-gcc here.
  'ISYSTEM_DRAGONEGG':
      '${BASE_LLVM_GCC}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
      '${BASE_LLVM_GCC}/' +
        'lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include',

  'ISYSTEM_newlib' :
    '${BASE_SDK}/include ' +
    '${ISYSTEM_%FRONTEND%} ' +
    '${BASE_LIBSTDCPP}/include/c++/4.6.2 ' +
    '${BASE_LIBSTDCPP}/include/c++/4.6.2/arm-none-linux-gnueabi ' +
    '${BASE_INCLUDE} ' +
    '${BASE_NEWLIB}/arm-none-linux-gnueabi/include',

  'ISYSTEM_glibc' :
    '${BASE_SDK}/include ' +
    '${BASE_GLIBC}/include ' +
    '${ISYSTEM_%FRONTEND%} ' +
    '${BASE_GLIBC}/include/c++/4.4.3 ' +
    '${BASE_GLIBC}/include/c++/4.4.3/x86_64-nacl',

  'LD_FLAGS' : '-O${OPT_LEVEL} ${STATIC ? -static} ${SHARED ? -shared} ' +
               '${PIC ? -fPIC} ${@AddPrefix:-L:SEARCH_DIRS}',

  'SEARCH_DIRS'      : '${SEARCH_DIRS_USER} ${PREFIXES}',
  'SEARCH_DIRS_USER' : '', # Directories specified using -L
  'PREFIXES'         : '', # Prefixes specified by using the -B flag.

  # Library Strings
  'EMITMODE'         : '${!STDLIB ? nostdlib : ' +
                       '${STATIC ? ${LIBMODE}_static : ' +
                       '${SHARED ? ${LIBMODE}_shared : ${LIBMODE}_dynamic}}}',

  # This is setup so that LD_ARGS_xxx is evaluated lazily.
  'LD_ARGS' : '${LD_ARGS_%EMITMODE%}',

  # ${ld_inputs} signifies where to place the objects and libraries
  # provided on the command-line.
  'LD_ARGS_nostdlib': '-nostdlib ${ld_inputs}',

  # TODO(pdox): Pull all native objects out of here
  #             and into pnacl-translate.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2423
  'LD_ARGS_newlib_static':
    '-l:crt1.x -l:crti.bc -l:crtdummy.bc -l:crtbegin.bc ${ld_inputs} ' +
    '${DEFAULTLIBS ? --start-group ${LIBSTDCPP} -lc -lnacl --end-group ' +
    '-l:pnacl_abi.bc}',

  # The next three are copied verbatim from nacl-gcc
  'LD_ARGS_glibc_static':
    '-l:crt1.bc -l:crti.bc -l:crtdummy.bc -l:crtbegin.bc ' +
    '${ld_inputs} ${DEFAULTLIBS ? ${LIBSTDCPP} -lc ' +
    # Replace with pnacl_abi.bc
    '--start-group -lgcc_eh -lc -lgcc --end-group}',

  'LD_ARGS_glibc_shared':
    '-shared -l:crti.bc -l:crtdummy.bc -l:crtbeginS.bc ' +
    '${ld_inputs} ${DEFAULTLIBS ? ${LIBSTDCPP} -lc ' +
    # Replace with pnacl_abi.bc
    '-lgcc_s}',

  'LD_ARGS_glibc_dynamic':
    '-l:crt1.bc -l:crti.bc -l:crtdummy.bc -l:crtbegin.bc ' +
    '${ld_inputs} ${DEFAULTLIBS ? ${LIBSTDCPP} -lc ' +
    # Replace with pnacl_abi.bc
    '-lgcc_s}',

  'LIBSTDCPP'   : '${LANGUAGE==CXX ? -lstdc++ -lm }',

  'CC'              : '${CC_%FRONTEND%_%LANGUAGE%}',
  'CC_CLANG_C'      : '${CLANG}',
  'CC_CLANG_CXX'    : '${CLANGXX}',
  'CC_DRAGONEGG_C'  : '${DRAGONEGG_GCC}',
  'CC_DRAGONEGG_CXX': '${DRAGONEGG_GXX}',

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

  env.append('PREFIXES', prefix)

  # Add prefix/include to isystem if it exists
  include_dir = prefix + 'include'
  if pathtools.isdir(include_dir):
    env.append('ISYSTEM_USER', include_dir)

CustomPatterns = [
  ( '--driver=(.+)',             "env.set('CC', pathtools.normalize($0))\n"),
  ( '--pnacl-skip-ll',           "env.set('EMIT_LL', '0')"),

  ( '--pnacl-clang',             "env.set('LANGUAGE', 'C')\n"
                                 "env.set('FRONTEND', 'CLANG')"),
  ( '--pnacl-clangxx',           "env.set('LANGUAGE', 'CXX')\n"
                                 "env.set('FRONTEND', 'CLANG')"),

  ( '--pnacl-dgcc',              "env.set('LANGUAGE', 'C')\n"
                                 "env.set('FRONTEND', 'DRAGONEGG')"),
  ( '--pnacl-dgxx',              "env.set('LANGUAGE', 'CXX')\n"
                                 "env.set('FRONTEND', 'DRAGONEGG')"),

  ( '--pnacl-allow-native',      "env.set('ALLOW_NATIVE', '1')"),
  ( '--pnacl-allow-translate',   "env.set('ALLOW_TRANSLATE', '1')"),
]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  ( '-nostdinc',       "env.set('STDINC', '0')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-nodefaultlibs',  "env.set('DEFAULTLIBS', '0')"),

  # Flags to pass to native linker
  ( '(-Wn,.*)',        AddLDFlag),

  # Flags to pass to pnacl-translate
  ( '(-Wt,.*)',        AddLDFlag),

  # We don't care about -fPIC, but pnacl-ld and pnacl-translate do.
  ( '-fPIC',           "env.set('PIC', '1')"),

  # We must include -l, -Xlinker, and -Wl options into the INPUTS
  # in the order they appeared. This is the exactly behavior of gcc.
  # For example: gcc foo.c -Wl,--start-group -lx -ly -Wl,--end-group
  #
  ( '(-l.+)',             "env.append('INPUTS', $0)"),
  ( ('(-l)','(.+)'),      "env.append('INPUTS', $0+$1)"),
  ( ('-Xlinker','(.*)'),  "env.append('INPUTS', '-Xlinker=' + $0)"),
  ( '(-Wl,.*)',           "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',         "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',        "env.append('INPUTS', $0)"),

  ( '-O([0-4s])',         "env.set('OPT_LEVEL', $0)\n"),
  ( '-O',                 "env.set('OPT_LEVEL', '1')\n"),

  ( ('-isystem', '(.*)'),
                       "env.append('ISYSTEM_USER', pathtools.normalize($0))"),
  ( ('-I', '(.+)'),    "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),
  ( '-I(.+)',          "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),

  ( '(-g)',                   AddCCFlag),
  ( '(-W.*)',                 AddCCFlag),
  ( '(-std=.*)',              AddCCFlag),
  ( ('(-D)','(.*)'),          AddCCFlag),
  ( '(-D.+)',                 AddCCFlag),
  ( ('(-U)','(.*)'),          AddCCFlag),
  ( '(-U.+)',                 AddCCFlag),
  ( '(-f.*)',                 AddCCFlag),
  ( '(-pedantic)',            AddCCFlag),
  ( '(-pedantic-errors)',     AddCCFlag),
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
  ( '(-v|--v|--version)',     "env.set('DIAGNOSTIC', '1')"),
  ( '(-V)',                   "env.set('DIAGNOSTIC', '1')\n"
                              "env.clear('CC_FLAGS')"),
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
    RunWithLog(env.get('CC') + env.get('CC_FLAGS') + real_argv)
    return 0

  unmatched = env.get('UNMATCHED')
  if len(unmatched) > 0:
    UnrecognizedOption(*unmatched)

  libmode_newlib = env.getbool('LIBMODE_NEWLIB')
  is_shared = env.getbool('SHARED')
  if libmode_newlib and is_shared and env.getbool('STDLIB'):
      Log.Fatal("Can't produce dynamic objects with newlib.")

  if libmode_newlib and not is_shared:
    env.set('STATIC', '1')

  if env.getbool('STATIC') and env.getbool('SHARED'):
    Log.Fatal("Can't handle both -static and -shared")

  # If -arch was given, we are compiling directly to native code
  compiling_to_native = GetArch() is not None

  if env.getbool('ALLOW_NATIVE') and not compiling_to_native:
    Log.Fatal("--pnacl-allow-native without -arch is not meaningful.")

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal('No input files')

  gcc_mode = env.getone('GCC_MODE')

  if env.getbool('SHARED'):
    bclink_output = 'pso'
    link_output = 'so'
  else:
    bclink_output = 'pexe'
    link_output = 'nexe'

  output_type_map = {
    ('-E', False) : 'pp',
    ('-E', True)  : 'pp',
    ('-c', False) : 'po',
    ('-c', True)  : 'o',
    ('-S', False) : 'll',
    ('-S', True)  : 's',
    ('',   False) : bclink_output,
    ('',   True)  : link_output
  }

  output_type = output_type_map[(gcc_mode, compiling_to_native)]
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
    if intype in ('o','s','S') or IsNativeArchive(f):
      if not env.getbool('ALLOW_NATIVE'):
        Log.Fatal('%s: Native object files not allowed in link. '
                  'Use --pnacl-allow-native to override.', pathtools.touser(f))
    assert(intype in ('po','o','so','pso','ldscript') or IsArchive(f))

  # Fix the user-specified linker arguments
  ld_inputs = []
  for f in inputs:
    if f.startswith('-Xlinker='):
      ld_inputs.append(f[len('-Xlinker='):])
    elif f.startswith('-Wl,'):
      ld_inputs += f[len('-Wl,'):].split(',')
    else:
      ld_inputs.append(f)

  if env.getbool('ALLOW_NATIVE'):
    ld_inputs.append('--pnacl-allow-native')

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
