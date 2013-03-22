#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
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
  'FORCE_INTERMEDIATE_LL': '0',
                          # Produce an intermediate .ll file
                          # Useful for debugging.
                          # NOTE: potentially different code paths and bugs
                          #       might be triggered by this
  'FORCE_INTERMEDIATE_S': '0',
                          # producing an intermediate .s file
                          # Useful for debugging.
                          # NOTE: potentially different code paths and bugs
                          #       might be triggered by this
  'LANGUAGE'    : '',     # C or CXX (set by SetTool)
  'INCLUDE_CXX_HEADERS': '0', # This is set by RunCC.

  # Command-line options
  'GCC_MODE'    : '',     # '' (default), '-E', '-c', or '-S'
  'STDINC'      : '1',    # Include standard headers (-nostdinc sets to 0)
  'STDLIB'      : '1',    # Include standard libraries (-nostdlib sets to 0)
  'DEFAULTLIBS' : '1',    # Link with default libraries
  'DIAGNOSTIC'  : '0',    # Diagnostic flag detected
  'SHARED'      : '0',    # Produce a shared library
  'STATIC'      : '0',    # -static (default)
  'DYNAMIC'     : '0',    # -dynamic
  'PIC'         : '0',    # Generate PIC
  # TODO(robertm): Switch the default to 1
  'NO_ASM'      : '0',    # Disallow use of inline assembler
  'NEED_DASH_E' : '0',    # Used for stdin inputs, which must have an explicit
                          # type set (using -x) unless -E is specified.
  'VERBOSE'     : '0',    # Verbose (-v)

  'PTHREAD'     : '0',   # use pthreads?
  'INPUTS'      : '',    # Input files
  'OUTPUT'      : '',    # Output file
  'UNMATCHED'   : '',    # Unrecognized parameters

  'BIAS_NONE'   : '',
  'BIAS_ARM'    : '-D__arm__ -D__ARM_ARCH_7A__ -D__ARMEL__',
  'BIAS_MIPS32' : '-D__MIPS__ -D__mips__ -D__MIPSEL__',
  'BIAS_X8632'  : '-D__i386__ -D__i386 -D__i686 -D__i686__ -D__pentium4__',
  'BIAS_X8664'  : '-D__amd64__ -D__amd64 -D__x86_64__ -D__x86_64 -D__core2__',
  'FRONTEND_TRIPLE' : 'le32-unknown-nacl',

  'OPT_LEVEL'   : '',  # Default for most tools is 0, but we need to know
                       # if it's explicitly set or not when the driver
                       # is only used for linking + translating.
  'CC_FLAGS'    : '-O${#OPT_LEVEL ? ${OPT_LEVEL} : 0} ' +
                  '-fno-common ${PTHREAD ? -pthread} ' +
                  '-nostdinc ${BIAS_%BIAS%} ' +
                  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=2345
                  # it would be better to detect asm use inside clang
                  # as some uses of asm are borderline legit, e.g.
                  # <prototype> asm("<function-name>");
                  '${NO_ASM ? -Dasm=ASM_FORBIDDEN -D__asm__=ASM_FORBIDDEN} ' +
                  '-target ${FRONTEND_TRIPLE}',


  'ISYSTEM'        : '${ISYSTEM_USER} ${STDINC ? ${ISYSTEM_BUILTIN}}',

  'ISYSTEM_USER'   : '',  # System include directories specified by
                          # using the -isystem flag.

  'ISYSTEM_BUILTIN':
    '${BASE_USR}/local/include ' +
    '${ISYSTEM_CLANG} ' +
    '${ISYSTEM_CXX} ' +
    '${BASE_USR}/include ' +
    '${BASE_SDK}/include ' +
    # This is used only for newlib bootstrapping.
    '${BASE_LIBMODE}/sysroot/include',

  'ISYSTEM_CLANG'  : '${BASE_LLVM}/lib/clang/3.3/include',

  'ISYSTEM_CXX' : '${INCLUDE_CXX_HEADERS ? ${ISYSTEM_CXX_%LIBMODE%}}',

  # TODO(pdox): This difference will go away as soon as we compile
  #             libstdc++.so ourselves.
  'ISYSTEM_CXX_newlib' :
    '${BASE_USR}/include/c++/4.6.2 ' +
    '${BASE_USR}/include/c++/4.6.2/arm-none-linux-gnueabi ' +
    '${BASE_USR}/include/c++/4.6.2/backward',


  'ISYSTEM_CXX_glibc' :
    '${BASE_USR}/include/c++/4.4.3 ' +
    '${BASE_USR}/include/c++/4.4.3/x86_64-nacl ' +
    '${BASE_USR}/include/c++/4.4.3/backward',

  # Only propagate opt level to linker if explicitly set, so that the
  # linker will know if an opt level was explicitly set or not.
  'LD_FLAGS' : '${#OPT_LEVEL ? -O${OPT_LEVEL}} ' +
               '${STATIC ? -static} ${SHARED ? -shared} ' +
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
    '-l:crt1.x ' +
    '-l:crti.bc -l:crtbegin.bc ${ld_inputs} ' +
    '--start-group ${STDLIBS} --end-group',

  'LD_ARGS_newlib_shared':
    '-l:crti.bc -l:crtbeginS.bc ${ld_inputs} ${STDLIBS}',

  'LD_ARGS_newlib_dynamic':
    '-l:crti.bc -l:crtbegin.bc ${ld_inputs} ' +
    '--start-group ${STDLIBS} --end-group',

  'LD_ARGS_glibc_shared':
    '-l:crti.bc -l:crtbeginS.bc ${ld_inputs} ${STDLIBS}',

  'LD_ARGS_glibc_dynamic':
    '-l:crt1.bc ' +
    '-l:crti.bc -l:crtbegin.bc ${ld_inputs} ${STDLIBS}',

  # Flags for translating to native .o files.
  'TRANSLATE_FLAGS' : '-O${#OPT_LEVEL ? ${OPT_LEVEL} : 0}',

  'STDLIBS'   : '${DEFAULTLIBS ? '
                '${LIBSTDCPP} ${LIBPTHREAD} ${LIBNACL} ${LIBC} ${PNACL_ABI}}',
  'LIBSTDCPP' : '${IS_CXX ? -lstdc++ -lm }',
  'LIBC'      : '-lc',
  'LIBNACL'   : '${LIBMODE_NEWLIB ? -lnacl}',
  # Enabled/disabled by -pthreads
  'LIBPTHREAD': '${PTHREAD ? -lpthread}',
  'PNACL_ABI' : '-l:pnacl_abi.bc',


  # IS_CXX is set by pnacl-clang and pnacl-clang++ programmatically
  'CC' : '${IS_CXX ? ${CLANGXX} : ${CLANG}}',
  'RUN_CC': '${CC} -emit-llvm ${mode} ${CC_FLAGS} ' +
            '${@AddPrefix:-isystem :ISYSTEM} ' +
            '-x${typespec} "${infile}" -o ${output}',
}

def AddLDFlag(*args):
  env.append('LD_FLAGS', *args)

def AddTranslatorFlag(*args):
  # pass translator args to ld in case we go all the way to .nexe
  env.append('LD_FLAGS', *['-Wt,' + a for a in args])
  # pass translator args to translator in case we go to .o
  env.append('TRANSLATE_FLAGS', *args)

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

stdin_count = 0
def AddInputFileStdin():
  global stdin_count

  # When stdin is an input, -x or -E must be given.
  forced_type = GetForcedFileType()
  if not forced_type:
    # Only allowed if -E is specified.
    forced_type = 'c'
    env.set('NEED_DASH_E', '1')

  stdin_name = '__stdin%d__' % stdin_count
  env.append('INPUTS', stdin_name)
  ForceFileType(stdin_name, forced_type)
  stdin_count += 1

def IsStdinInput(f):
  return f.startswith('__stdin') and f.endswith('__')

def HandleDashX(arg):
  if arg == 'none':
    SetForcedFileType(None)
  SetForcedFileType(GCCTypeToFileType(arg))

CustomPatterns = [
  ( '--driver=(.+)',             "env.set('CC', pathtools.normalize($0))\n"),
  ( '--pnacl-allow-native',      "env.set('ALLOW_NATIVE', '1')"),
  ( '--pnacl-allow-translate',   "env.set('ALLOW_TRANSLATE', '1')"),
  ( '--pnacl-frontend-triple=(.+)',   "env.set('FRONTEND_TRIPLE', $0)"),
  ( '(--pnacl-disable-pass=.+)', AddLDFlag),
]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  ( '-allow-asm',       "env.set('NO_ASM', '0')"),

  ( '-nostdinc',       "env.set('STDINC', '0')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-nodefaultlibs',  "env.set('DEFAULTLIBS', '0')"),

  # Flags to pass to native linker
  ( '(-Wn,.*)',        AddLDFlag),
  ( '-rdynamic', "env.append('LD_FLAGS', '-export-dynamic')"),

  # Flags to pass to pnacl-translate
  ( '-Wt,(.*)',               AddTranslatorFlag),
  ( ('-Xtranslator','(.*)'),  AddTranslatorFlag),

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

  ( '-O(s)',              "env.set('OPT_LEVEL', $0)\n"),
  ( '-O([0-3])',          "env.set('OPT_LEVEL', $0)\n"),
  ( '-O([0-9]+)',         "env.set('OPT_LEVEL', '3')\n"),
  ( '-O',                 "env.set('OPT_LEVEL', '1')\n"),

  ( ('-isystem', '(.*)'),
                       "env.append('ISYSTEM_USER', pathtools.normalize($0))"),
  ( '-isystem(.+)',
                       "env.append('ISYSTEM_USER', pathtools.normalize($0))"),
  ( ('-I', '(.+)'),    "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),
  ( '-I(.+)',          "env.append('CC_FLAGS', '-I'+pathtools.normalize($0))"),

  # NOTE: the -iquote =DIR syntax (substitute = with sysroot) doesn't work.
  # Clang just says: ignoring nonexistent directory "=DIR"
  ( ('-iquote', '(.+)'),
    "env.append('CC_FLAGS', '-iquote', pathtools.normalize($0))"),
  ( ('-iquote(.+)'),
    "env.append('CC_FLAGS', '-iquote', pathtools.normalize($0))"),

  ( ('-idirafter', '(.+)'),
      "env.append('CC_FLAGS', '-idirafter'+pathtools.normalize($0))"),
  ( '-idirafter(.+)',
      "env.append('CC_FLAGS', '-idirafter'+pathtools.normalize($0))"),

  ( ('(-include)','(.+)'),    AddCCFlag),
  ( ('(-include.+)'),         AddCCFlag),
  ( '(-g)',                   AddCCFlag),
  ( '(-W.*)',                 AddCCFlag),
  ( '(-std=.*)',              AddCCFlag),
  ( '(-ansi)',                AddCCFlag),
  ( ('(-D)','(.*)'),          AddCCFlag),
  ( '(-D.+)',                 AddCCFlag),
  ( ('(-U)','(.*)'),          AddCCFlag),
  ( '(-U.+)',                 AddCCFlag),
  ( '(-f.*)',                 AddCCFlag),
  ( '(-pedantic)',            AddCCFlag),
  ( '(-pedantic-errors)',     AddCCFlag),
  ( '(-g.*)',                 AddCCFlag),
  ( '(-v|--v)',               "env.append('CC_FLAGS', $0)\n"
                              "env.set('VERBOSE', '1')"),
  ( '(-pthreads?)',           "env.set('PTHREAD', '1')"),

  ( '-shared',                "env.set('SHARED', '1')"),
  ( '-static',                "env.set('STATIC', '1')"),
  ( '-dynamic',               "env.set('DYNAMIC', '1')"),

  ( ('-B','(.*)'),            AddBPrefix),
  ( ('-B(.+)'),               AddBPrefix),

  ( ('-L','(.+)'), "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),
  ( '-L(.+)',      "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))"),

  ( '(-Wp,.*)', AddCCFlag),
  ( '(-Xpreprocessor .*)', AddCCFlag),

  ( '(-MG)',          AddCCFlag),
  ( '(-MMD)',         AddCCFlag),
  ( '(-MM)',          "env.append('CC_FLAGS', $0)\n"
                      "env.set('GCC_MODE', '-E')"),
  ( '(-MP)',          AddCCFlag),
  ( ('(-MQ)','(.*)'), AddCCFlag),
  ( '(-MD)',          AddCCFlag),
  ( ('(-MT)','(.*)'), AddCCFlag),
  ( ('(-MF)','(.*)'), "env.append('CC_FLAGS', $0, pathtools.normalize($1))"),

  ( ('-x', '(.+)'),    HandleDashX),
  ( '-x(.+)',          HandleDashX),

  ( ('(-mllvm)', '(.+)'), AddCCFlag),

  # Ignore these gcc flags
  ( '(-msse)',                ""),
  ( '(-march=armv7-a)',       ""),
  ( '(-pipe)',                ""),

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

  ( '(-mfloat-abi=.+)',       AddCCFlag),

  # GCC diagnostic mode triggers
  ( '(-print-.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(--print.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(-dumpspecs)',           "env.set('DIAGNOSTIC', '1')"),
  ( '(--version)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(-V)',                   "env.set('DIAGNOSTIC', '1')\n"
                              "env.clear('CC_FLAGS')"),
  # These are preprocessor flags which should be passed to the frontend, but
  # should not prevent the usual -i flags (which DIAGNOSTIC mode does)
  ( '(-d[DIMNU])',            AddCCFlag),
  ( '(-d.*)',                 "env.set('DIAGNOSTIC', '1')"),

  # Catch all other command-line arguments
  ( '(-.+)',              "env.append('UNMATCHED', $0)"),

  # Standard input
  ( '-',     AddInputFileStdin),

  # Input Files
  # Call ForceFileType for all input files at the time they are
  # parsed on the command-line. This ensures that the gcc "-x"
  # setting is correctly applied.
  ( '(.*)',  "env.append('INPUTS', pathtools.normalize($0))\n"
             "ForceFileType(pathtools.normalize($0))"),
]

def CheckSetup():
  if not env.has('IS_CXX'):
    Log.Fatal('"pnacl-driver" cannot be used directly. '
              'Use pnacl-clang or pnacl-clang++.')

def main(argv):
  env.update(EXTRA_ENV)
  CheckSetup()
  ParseArgs(argv, CustomPatterns + GCCPatterns)

  # "configure", especially when run as part of a toolchain bootstrap
  # process, will invoke gcc with various diagnostic options and
  # parse the output. In these cases we do not alter the incoming
  # commandline. It is also important to not emit spurious messages.
  if env.getbool('DIAGNOSTIC'):
    Run(env.get('CC') + env.get('CC_FLAGS') + argv)
    return 0

  unmatched = env.get('UNMATCHED')
  if len(unmatched) > 0:
    UnrecognizedOption(*unmatched)

  libmode_newlib = env.getbool('LIBMODE_NEWLIB')
  is_shared = env.getbool('SHARED')

  if env.getbool('STATIC') and env.getbool('SHARED'):
    Log.Fatal("Can't handle both -static and -shared")

  # default to static for newlib
  if (env.getbool('LIBMODE_NEWLIB') and
      not env.getbool('DYNAMIC') and
      not env.getbool('SHARED')):
    env.set('STATIC', '1')

  # If -arch was given, we are compiling directly to native code
  compiling_to_native = GetArch() is not None

  if env.getbool('ALLOW_NATIVE') and not compiling_to_native:
    Log.Fatal("--pnacl-allow-native without -arch is not meaningful.")

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    if env.getbool('VERBOSE'):
      # -v can be invoked without any inputs. Runs the original
      # command without modifying the commandline for this case.
      Run(env.get('CC') + env.get('CC_FLAGS') + argv)
      return 0
    else:
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

  if env.getbool('NEED_DASH_E') and gcc_mode != '-E':
    Log.Fatal("-E or -x required when input is from stdin")

  # There are multiple input files and no linking is being done.
  # There will be multiple outputs. Handle this case separately.
  if not needs_linking:
    # Filter out flags
    inputs = [f for f in inputs if not IsFlag(f)]
    if output != '' and len(inputs) > 1:
      Log.Fatal('Cannot have -o with -c, -S, or -E and multiple inputs: %s',
                repr(inputs))

    for f in inputs:
      if IsFlag(f):
        continue
      intype = FileType(f)
      if not IsSourceType(intype):
        if ((output_type == 'pp' and intype != 'S') or
            (output_type == 'll') or
            (output_type == 'po' and intype != 'll') or
            (output_type == 's' and intype not in ('ll','po','S')) or
            (output_type == 'o' and intype not in ('ll','po','S','s'))):
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
    if IsFlag(inputs[i]):
      continue
    intype = FileType(inputs[i])
    if IsSourceType(intype) or intype == 'll':
      inputs[i] = CompileOne(inputs[i], 'po', namegen)

  # Compile all .s/.S to .o
  if env.getbool('ALLOW_NATIVE'):
    for i in xrange(0, len(inputs)):
      if IsFlag(inputs[i]):
        continue
      intype = FileType(inputs[i])
      if intype in ('s','S'):
        inputs[i] = CompileOne(inputs[i], 'o', namegen)

  # We should only be left with .po and .o and libraries
  for f in inputs:
    if IsFlag(f):
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

  RunDriver('ld', ld_flags + ld_args + ['-o', output])
  return 0

def IsFlag(f):
  return f.startswith('-')

def CompileOne(infile, output_type, namegen, output = None):
  if output is None:
    output = namegen.TempNameForInput(infile, output_type)

  chain = DriverChain(infile, output, namegen)
  SetupChain(chain, FileType(infile), output_type)
  chain.run()
  return output

def RunCC(infile, output, mode):
  intype = FileType(infile)
  typespec = FileTypeToGCCType(intype)
  include_cxx_headers = (env.get('LANGUAGE') == 'CXX') or (intype == 'c++')
  env.setbool('INCLUDE_CXX_HEADERS', include_cxx_headers)
  if IsStdinInput(infile):
    infile = '-'
  RunWithEnv("${RUN_CC}", infile=infile, output=output,
                          mode=mode,
                          typespec=typespec)

def RunLLVMAS(infile, output):
  if IsStdinInput(infile):
    infile = '-'
  # This is a bitcode only step - so get rid of "-arch xxx" which
  # might be inherited from the current invocation
  RunDriver('as', [infile, '-o', output], suppress_inherited_arch_args=True)

def RunNativeAS(infile, output):
  if IsStdinInput(infile):
    infile = '-'
  RunDriver('as', [infile, '-o', output])

def RunTranslate(infile, output, mode):
  if not env.getbool('ALLOW_TRANSLATE'):
    Log.Fatal('%s: Trying to convert bitcode to an object file before '
              'bitcode linking. This is supposed to wait until '
              'translation. Use --pnacl-allow-translate to override.',
              pathtools.touser(infile))
  args = env.get('TRANSLATE_FLAGS') + [mode, infile, '-o', output]
  if env.getbool('PIC'):
    args += ['-fPIC']
  RunDriver('translate', args)

def SetupChain(chain, input_type, output_type):
  assert(output_type in ('pp','ll','po','s','o'))
  cur_type = input_type

  # source file -> pp
  if IsSourceType(cur_type) and output_type == 'pp':
    chain.add(RunCC, 'cpp', mode='-E')
    cur_type = 'pp'
  if cur_type == output_type:
    return

  # source file -> ll
  if (IsSourceType(cur_type) and
     (env.getbool('FORCE_INTERMEDIATE_LL') or output_type == 'll')):
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

  # source file -> po (we also force native output to go through this phase
  if IsSourceType(cur_type) and output_type in ('po', 'o', 's'):
    chain.add(RunCC, 'po', mode='-c')
    cur_type = 'po'
  if cur_type == output_type:
    return

  # po -> o
  if (cur_type == 'po' and output_type == 'o' and
      not env.getbool('FORCE_INTERMEDIATE_S')):
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
    chain.add(RunCC, 's', mode='-E')
    cur_type = 's'
    if output_type == 'pp':
      return
  if cur_type == output_type:
    return

  # s -> o
  if cur_type == 's' and output_type == 'o':
    chain.add(RunNativeAS, 'o')
    cur_type = 'o'
  if cur_type == output_type:
    return

  Log.Fatal("Unable to compile .%s to .%s", input_type, output_type)

def get_help(argv):
  tool = env.getone('SCRIPT_NAME')

  if '--help-full' in argv:
    # To get ${CC}, etc.
    env.update(EXTRA_ENV)
    code, stdout, stderr = Run('"${CC}" -help',
                              redirect_stdout=subprocess.PIPE,
                              redirect_stderr=subprocess.STDOUT,
                              errexit=False)
    return stdout
  else:
    return """
This is a "GCC-compatible" driver using clang under the hood.

Usage: %s [options] <inputs> ...

BASIC OPTIONS:
  -o <file>             Output to <file>.
  -E                    Only run the preprocessor.
  -S                    Generate bitcode assembly.
  -c                    Generate bitcode object.
  -I <dir>              Add header search path.
  -L <dir>              Add library search path.
  -D<key>[=<val>]       Add definition for the preprocessor.
  -W<id>                Toggle warning <id>.
  -f<feature>           Enable <feature>.
  -Wl,<arg>             Pass <arg> to the linker.
  -Xlinker <arg>        Pass <arg> to the linker.
  -Wt,<arg>             Pass <arg> to the translator.
  -Xtranslator <arg>    Pass <arg> to the translator.
  -Wp,<arg>             Pass <arg> to the preprocessor.
  -Xpreprocessor,<arg>  Pass <arg> to the preprocessor.
  -x <language>         Treat subsequent input files as having type <language>.
  -static               Produce a static executable.
  -shared               Produce a shared object.
  -Bstatic              Link subsequent libraries statically.
  -Bdynamic             Link subsequent libraries dynamically.
  -fPIC                 Ignored (only used by translator backend)
                        (accepted for compatibility).
  -pipe                 Ignored (for compatibility).
  -O<n>                 Optimation level <n>: 0, 1, 2, 3, 4 or s.
  -g                    Generate complete debug information.
  -gline-tables-only    Generate debug line-information only
                        (allowing for stack traces).
  -flimit-debug-info    Generate limited debug information.
  -save-temps           Keep intermediate compilation results.
  -v                    Verbose output / show commands.
  -h | --help           Show this help.
  --help-full           Show underlying clang driver's help message
                        (warning: not all options supported).
""" % (tool)
