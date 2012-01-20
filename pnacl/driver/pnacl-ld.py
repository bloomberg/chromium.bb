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
from driver_log import Log, DriverOpen, TempFiles
import platform

EXTRA_ENV = {
  'ALLOW_NATIVE': '0', # Allow LD args which will change the behavior
                       # of native linking. This must be accompanied by
                       # -arch to produce a .nexe.
  'INPUTS'   : '',
  'OUTPUT'   : '',

  'SHARED'   : '0',
  'STATIC'   : '0',
  'PIC'      : '0',
  'STDLIB'   : '1',
  'RELOCATABLE': '0',
  'SONAME'   : '',

  'STRIP_MODE' : 'none',

  'STRIP_FLAGS'      : '${STRIP_FLAGS_%STRIP_MODE%}',
  'STRIP_FLAGS_all'  : '-s',
  'STRIP_FLAGS_debug': '-S',

  'NOBITCODE': '0', # True if there are no bitcode objects in the link.
                    # (native link only)
  'TRANSLATE_FLAGS': '${PIC ? -fPIC} ${!STDLIB ? -nostdlib} ' +
                     '${STATIC ? -static} ' +
                     # The intermediate bitcode file normally encodes
                     # these flags. But if there is no bitcode, then
                     # we must pass these flags manually to
                     # pnacl-translate.
                     '${NOBITCODE && SHARED ? -shared} ' +
                     '${NOBITCODE && #SONAME ? -Wl,--soname=${SONAME}} ' +
                     '${TRANSLATE_FLAGS_USER}',

  # Extra pnacl-translate flags specified by the user using -Wt
  'TRANSLATE_FLAGS_USER': '',

  'OPT_FLAGS': '-O${OPT_LEVEL} ${OPT_STRIP_%STRIP_MODE%} ' +
               '-inline-threshold=${OPT_INLINE_THRESHOLD}',
  'OPT_INLINE_THRESHOLD': '100',
  'OPT_LEVEL': '0',
  'OPT_STRIP_none': '',
  'OPT_STRIP_all': '-disable-opt --strip',
  'OPT_STRIP_debug': '-disable-opt --strip-debug',

  'GOLD_PLUGIN_ARGS': '-plugin=${GOLD_PLUGIN_SO} ' +
                      '-plugin-opt=emit-llvm',

  'LD_FLAGS'       : '-nostdlib ${@AddPrefix:-L:SEARCH_DIRS} ' +
                     '${SHARED ? -shared} ${STATIC ? -static} ' +
                     '${RELOCATABLE ? -relocatable} ' +
                     '${#SONAME ? --soname=${SONAME}}',

  # Flags for native linking.
  # Only allowed if ALLOW_NATIVE is true.
  'LD_FLAGS_NATIVE': '',

  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',

  # Standard Library Directories
  'SEARCH_DIRS_BUILTIN': '${STDLIB ? ' +
                         '  ${BASE_USR}/lib/ ' +
                         '  ${BASE_SDK}/lib/ ' +
                         '  ${BASE_LIB}/ ' +
                         '  ${SEARCH_DIRS_NATIVE} ' +
                         '}',

  # HACK-BEGIN
  # Add lib-<arch>/ to the search path.
  # These are here to let the bitcode link find the native objects
  # used in the GLibC toolchain.
  # TODO(pdox): Remove these when we have bitcode .pso stubs for GlibC.
  'SEARCH_DIRS_NATIVE': '${LIBMODE_GLIBC ? ${LIBS_ARCH}/}',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE_LIB_NATIVE}arm',
  'LIBS_X8632'       : '${BASE_LIB_NATIVE}x86-32',
  'LIBS_X8664'       : '${BASE_LIB_NATIVE}x86-64',
  # HACK-END

  'LD_GOLD_OFORMAT'        : '${LD_GOLD_OFORMAT_%ARCH%}',
  'LD_GOLD_OFORMAT_ARM'    : 'elf32-littlearm',
  'LD_GOLD_OFORMAT_X8632'  : 'elf32-nacl',
  'LD_GOLD_OFORMAT_X8664'  : 'elf64-nacl',

  'BCLD'      : '${LD_GOLD}',
  'BCLD_FLAGS': '--native-client --oformat ${LD_GOLD_OFORMAT} -Ttext=0x20000 ' +
                '${!SHARED && !RELOCATABLE ? --undef-sym-check} ' +
                '${GOLD_PLUGIN_ARGS} ${LD_FLAGS}',
  'RUN_BCLD': ('${BCLD} ${BCLD_FLAGS} ${inputs} '
               '-o "${output}"'),
}
env.update(EXTRA_ENV)

def AddToBCLinkFlags(*args):
  env.append('LD_FLAGS', *args)

def AddToNativeFlags(*args):
  env.append('LD_FLAGS_NATIVE', *args)

def AddToBothFlags(*args):
  env.append('LD_FLAGS', *args)
  env.append('LD_FLAGS_NATIVE', *args)

LDPatterns = [
  ( '--pnacl-allow-native', "env.set('ALLOW_NATIVE', '1')"),

  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-r',              "env.set('RELOCATABLE', '1')"),
  ( '-relocatable',    "env.set('RELOCATABLE', '1')"),

  ( ('-L', '(.+)'),
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))\n"),
  ( '-L(.+)',
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))\n"),

  # We just ignore undefined symbols in shared objects, so
  # -rpath-link should not be necessary.
  #
  # However, libsrpc.so still needs to be linked in directly (in non-IRT mode)
  # or it malfunctions. This is the only way that -rpath-link is still used.
  # There's a corresponding hack in pnacl-translate to recognize libsrpc.so
  # and link it in directly.
  # TODO(pdox): Investigate this situation.
  ( ('(-rpath)','(.*)'), ""),
  ( ('(-rpath)=(.*)'), ""),
  ( ('(-rpath-link)','(.*)'),
    "env.append('TRANSLATE_FLAGS', $0+'='+pathtools.normalize($1))"),
  ( ('(-rpath-link)=(.*)'),
    "env.append('TRANSLATE_FLAGS', $0+'='+pathtools.normalize($1))"),

  ( ('(-Ttext)','(.*)'), AddToNativeFlags),
  ( ('(-Ttext=.*)'),     AddToNativeFlags),

  # This overrides the builtin linker script.
  ( ('(-T)', '(.*)'),    AddToNativeFlags),

  # TODO(pdox): Allow setting an alternative _start symbol in bitcode
  ( ('(-e)','(.*)'),     AddToBothFlags),

  # TODO(pdox): Support GNU versioning.
  ( '(--version-script=.*)', ""),

  # Flags to pass to the native linker.
  ( '-Wn,(.*)', "env.append('LD_FLAGS_NATIVE', *($0.split(',')))"),
  # Flags to pass to translate
  ( '-Wt,(.*)', "env.append('TRANSLATE_FLAGS_USER', *($0.split(',')))"),


  ( ('(--section-start)','(.*)'), AddToNativeFlags),

  ( '-?-soname=(.*)',             "env.set('SONAME', $0)"),
  ( ('-?-soname', '(.*)'),        "env.set('SONAME', $0)"),

  ( '(-M)',                       AddToBCLinkFlags),
  ( '(-t)',                       AddToBCLinkFlags),
  ( ('(-y)','(.*)'),              AddToBCLinkFlags),
  ( ('(-defsym)','(.*)'),         AddToBCLinkFlags),

  ( '-melf_nacl',          "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),     "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',        "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),   "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',       "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),  "env.set('ARCH', 'ARM')"),

  ( ('(-?-wrap)', '(.+)'), AddToBCLinkFlags),
  ( ('(-?-wrap=.+)'),      AddToBCLinkFlags),

  # NOTE: For scons tests, the code generation fPIC flag is used with pnacl-ld.
  ( '-fPIC',               "env.set('PIC', '1')"),

  # This controls LTO optimization.
  # opt does not support -Os but internally it is identical to -O2
  # opt also does not support -O4 but -O4 is how you ask clang for LTO, so we
  # can support it as well
  ( '-Os',                 "env.set('OPT_LEVEL', '2')"),
  ( '-O([0-3])',           "env.set('OPT_LEVEL', $0)"),
  ( '-O([0-9]+)',          "env.set('OPT_LEVEL', '3')"),

  ( '-s',                  "env.append('STRIP_MODE', 'all')"),
  ( '--strip-all',         "env.append('STRIP_MODE', 'all')"),
  ( '-S',                  "env.append('STRIP_MODE', 'debug')"),
  ( '--strip-debug',       "env.append('STRIP_MODE', 'debug')"),

  # Inputs and options that need to be kept in order
  ( '(-l.*)',              "env.append('INPUTS', $0)"),
  ( ('(-l)','(.*)'),       "env.append('INPUTS', $0+$1)"),
  ( '(--no-as-needed)',    "env.append('INPUTS', $0)"),
  ( '(--as-needed)',       "env.append('INPUTS', $0)"),
  ( '(--start-group)',     "env.append('INPUTS', $0)"),
  ( '(--end-group)',       "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',          "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',          "env.append('INPUTS', $0)"),
  ( '(--(no-)?whole-archive)', "env.append('INPUTS', $0)"),
  ( '(--undefined=.*)',    "env.append('INPUTS', $0)"),
  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  ParseArgs(argv, LDPatterns)

  # If the user passed -arch, then they want native output.
  arch_flag_given = GetArch() is not None

  # Both LD_FLAGS_NATIVE and TRANSLATE_FLAGS_USER affect
  # the translation process. If they are non-empty,
  # then --pnacl-allow-native must be given.
  allow_native = env.getbool('ALLOW_NATIVE')
  native_flags = env.get('LD_FLAGS_NATIVE') + env.get('TRANSLATE_FLAGS_USER')
  if len(native_flags) > 0:
    if not allow_native:
      flagstr = ' '.join(native_flags)
      Log.Fatal('"%s" affects translation. '
                'To allow, specify --pnacl-allow-native' % flagstr)

  if env.getbool('ALLOW_NATIVE') and not arch_flag_given:
      Log.Fatal("--pnacl-allow-native given, but translation "
                "is not happening (missing -arch?)")

  if env.getbool('RELOCATABLE'):
    if env.getbool('SHARED'):
      Log.Fatal("-r and -shared may not be used together")
    env.set('STATIC', '0')

  if (env.getbool('LIBMODE_NEWLIB') and
      env.getbool('STDLIB') and
      env.getbool('SHARED')):
      Log.Fatal("Cannot generate shared objects with newlib-based toolchain")

  # Default to -static for newlib.
  if (env.getbool('LIBMODE_NEWLIB') and
      not env.getbool('SHARED') and
      not env.getbool('STATIC') and
      not env.getbool('RELOCATABLE')):
    env.set('STATIC', '1')

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = pathtools.normalize('a.out')

  if not arch_flag_given:
    # If -arch is not given, assume X86-32.
    # This is because gold requires an arch (even for bitcode linking).
    SetArch('X8632')
  assert(GetArch() is not None)

  # Expand all parameters
  # This resolves -lfoo into actual filenames,
  # and expands linker scripts into command-line arguments.
  inputs = ldtools.ExpandInputs(inputs,
                                env.get('SEARCH_DIRS'),
                                env.getbool('STATIC'))

  # Make sure the inputs have matching arch.
  CheckInputsArch(inputs)

  regular_inputs, native_objects = SplitLinkLine(inputs)

  # Filter out object files which are currently used in the bitcode link.
  # These don't actually need to be treated separately, since the
  # translator includes them automatically. Eventually, these will
  # be compiled to bitcode or replaced by bitcode stubs, and this list
  # can go away.
  if env.getbool('STDLIB'):
    native_objects = RemoveNativeStdLibs(native_objects)

  if env.getbool('SHARED'):
    bitcode_type = 'pso'
    native_type = 'so'
  elif env.getbool('RELOCATABLE'):
    bitcode_type = 'po'
    native_type = 'o'
  else:
    bitcode_type = 'pexe'
    native_type = 'nexe'

  if native_objects and not allow_native:
    argstr = ' '.join(native_objects)
    Log.Fatal("Native objects '%s' detected in the link. "
              "To allow, specify --pnacl-allow-native" % argstr)

  tng = TempNameGen([], output)

  # Do the bitcode link.
  if HasBitcodeInputs(inputs):
    chain = DriverChain(inputs, output, tng)
    chain.add(LinkBC, 'pre_opt.' + bitcode_type)
    if env.getone('OPT_LEVEL') != '0':
      chain.add(DoOPT, 'opt.' + bitcode_type)
    elif env.getone('STRIP_MODE') != 'none':
      chain.add(DoStrip, 'stripped.' + bitcode_type)
  else:
    env.set('NOBITCODE', '1')
    chain = DriverChain('', output, tng)

  # If -arch is also specified, invoke pnacl-translate afterwards.
  if arch_flag_given:
    env.set('NATIVE_OBJECTS', *native_objects)
    chain.add(DoTranslate, native_type)

  chain.run()

  if bitcode_type == 'pexe' and not arch_flag_given:
    # Mark .pexe files as executable.
    # Some versions of 'configure' expect this.
    SetExecutableMode(output)
  return 0

def RemoveNativeStdLibs(objs):
  # For newlib, all standard libraries are already bitcode.
  if env.getbool('LIBMODE_NEWLIB'):
    return objs

  # GLibC standard libraries
  defaultlibs = ['libc_nonshared.a', 'libpthread_nonshared.a',
                 'libc.a', 'libstdc++.a', 'libgcc.a', 'libgcc_eh.a',
                 'libm.a']
  return [f for f in objs if pathtools.split(f)[1] not in defaultlibs]

def SplitLinkLine(inputs):
  """ Pull native objects (.o, .a) out of the input list.
      These need special handling since they cannot be
      encoded in the bitcode file.
      (NOTE: native .so files do not need special handling,
       because they can be encoded as dependencies in the bitcode)
  """
  normal = []
  native = []
  for f in inputs:
    if ldtools.IsFlag(f):
      normal.append(f)
    elif IsNativeArchive(f) or IsNativeObject(f):
      native.append(f)
    else:
      normal.append(f)
  return (normal, native)

def HasBitcodeInputs(inputs):
  for f in inputs:
    if ldtools.IsFlag(f):
      continue
    elif IsBitcodeObject(f) or IsBitcodeArchive(f):
      return True
  return False

def CheckInputsArch(inputs):
  count = 0
  for f in inputs:
    if ldtools.IsFlag(f):
      continue
    elif IsBitcodeObject(f) or IsBitcodeDSO(f) or IsBitcodeArchive(f):
      pass
    elif IsNative(f):
      ArchMerge(f, True)
    else:
      Log.Fatal("%s: Unexpected type of file for linking (%s)",
                pathtools.touser(f), FileType(f))
    count += 1

  if count == 0:
    Log.Fatal("no input files")

def DoOPT(infile, outfile):
  opt_flags = env.get('OPT_FLAGS')
  RunDriver('pnacl-opt', opt_flags + [ infile, '-o', outfile ])

def DoStrip(infile, outfile):
  strip_flags = env.get('STRIP_FLAGS')
  RunDriver('pnacl-strip', strip_flags + [ infile, '-o', outfile ])

def DoTranslate(infile, outfile):
  args = env.get('TRANSLATE_FLAGS')
  args += ['-Wl,'+s for s in env.get('LD_FLAGS_NATIVE')]
  if infile:
    args += [infile]
  args += [s for s in env.get('NATIVE_OBJECTS')]
  args += ['-o', outfile]
  RunDriver('pnacl-translate', args)

def LinkBC(inputs, output):
  '''Input: a bunch of bc/o/lib input files
     Output: a combined & optimized bitcode file
  '''
  # Produce combined bitcode file
  RunWithEnv('${RUN_BCLD}',
             inputs=inputs,
             output=output)

if __name__ == "__main__":
  DriverMain(main)
