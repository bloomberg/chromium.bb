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
  'NATIVE_HACK' : '0',    # Only link native code, ignore bitcode libraries.
                          #
                          # This is currently only used for translating .pexe's
                          # which are linked against glibc. Since they continue
                          # to have native object dependencies, they have to go
                          # through pnacl-gcc again, followed by pnacl-ld.
                          # But since we know the bitcode dependencies have
                          # already been linked in, we remove those from the
                          # link line with this flag.

  'WHICH_LD'    : 'BFD',  # Which ld to use for native linking: GOLD or BFD

  'INPUTS'   : '',
  'OUTPUT'   : '',

  'SHARED'   : '0',
  'STATIC'   : '0',

  'LINK_IN_BITCODE_INTRINSICS' : '1',

  'STRIP_MODE' : 'none',

  'STRIP_FLAGS'      : '${STRIP_FLAGS_%STRIP_MODE%}',
  'STRIP_FLAGS_all'  : '-s',
  'STRIP_FLAGS_debug': '-S',

  'PNACL_TRANSLATE_FLAGS': '${SHARED ? -shared}',

  'OPT_FLAGS': '-std-compile-opts ${OPT_LEVEL} ${OPT_STRIP_%STRIP_MODE%}',
  'OPT_LEVEL': '-O3',
  'OPT_STRIP_none': '',
  'OPT_STRIP_all': '-disable-opt --strip',
  'OPT_STRIP_debug': '-disable-opt --strip-debug',

  # Sandboxed LD is always BFD.
  'LD'          : '${SANDBOXED ? ${LD_SB} ${LD_BFD_FLAGS} ' +
                             ' : ${LD_%WHICH_LD%} ${LD_%WHICH_LD%_FLAGS}}',

  'LD_BFD_FLAGS': '-m ${LD_EMUL} -T ${LD_SCRIPT}',

  'LD_GOLD_FLAGS': '--native-client ' +
                   '${GOLD_FIX ? -T ${LD_SCRIPT} : -Ttext=0x20000}',

   # Symbols to wrap
  'WRAP_SYMBOLS': '',

  # Common to both GOLD and BFD.
  'LD_FLAGS'       : '-nostdlib ${LD_SEARCH_DIRS} ' +
                     '${SHARED ? -shared} ${STATIC ? -static}',

  'LD_SEARCH_DIRS' : '',
  'SEARCH_DIRS'    : '',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

  'LDSCRIPTS_DIR'  : '${BASE}/ldscripts',

  'LD_SCRIPT'      : '${LD_SCRIPT_%ARCH%}',
  'LD_SCRIPT_ARM'  : '${LDSCRIPTS_DIR}/ld_script_arm_untrusted',
  'LD_SCRIPT_X8632': '${LDSCRIPTS_DIR}/ld_script_x8632_untrusted',
  'LD_SCRIPT_X8664': '${LDSCRIPTS_DIR}/ld_script_x8664_untrusted',

  'BCLD'      : '${LD_GOLD}',
  'BCLD_FLAGS': '${LD_GOLD_FLAGS} ${!SHARED ? --undef-sym-check} ' +
                '${GOLD_PLUGIN_ARGS} ${LD_FLAGS}',

  'RUN_LD' : '${LD} ${LD_FLAGS} ${inputs} -o "${output}"',

  'RUN_BCLD': ('${BCLD} ${BCLD_FLAGS} ${inputs} '
               '${LINK_IN_BITCODE_INTRINSICS ? ${BASE}/llvm-intrinsics.bc} '
               '-o "${output}"'),
}
env.update(EXTRA_ENV)

LDPatterns = [
  ( '--pnacl-native-hack', "env.set('NATIVE_HACK', '1')"),

  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '-nostdlib',       ""),

  ( '-nobitcode-intrinsics', "env.set('LINK_IN_BITCODE_INTRINSICS', '0')"),

  ( '-shared',         "env.set('SHARED', '1')"),

  ( '-static',         "env.set('STATIC', '1')\n"
                       "env.set('SHARED', '0')"),

  ( ('-L', '(.+)'),    "env.append('SEARCH_DIRS', $0)\n"
                       "env.append('LD_SEARCH_DIRS', '-L'+$0)"),
  ( '-L(.+)',          "env.append('SEARCH_DIRS', $0)\n"
                       "env.append('LD_SEARCH_DIRS', '-L'+$0)"),

  ( ('(-rpath)','(.*)'), "env.append('LD_FLAGS', $0+'='+$1)"),
  ( ('(-rpath=.*)'),     "env.append('LD_FLAGS', $0)"),

  ( ('(-Ttext)','(.*)'), "env.append('LD_FLAGS', $0, $1)"),
  ( ('(-Ttext=.*)'),     "env.append('LD_FLAGS', $0)"),
  ( ('-T', '(.*)'),    "env.set('LD_SCRIPT', $0)"),

  ( ('-e','(.*)'),     "env.append('LD_FLAGS', '-e', $0)"),
  ( ('(--section-start)','(.*)'), "env.append('LD_FLAGS', $0, $1)"),
  ( '(-?-soname=.*)',             "env.append('LD_FLAGS', $0)"),
  ( '(--eh-frame-hdr)',           "env.append('LD_FLAGS', $0)"),

  ( '-melf_nacl',          "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),     "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',        "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),   "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',       "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),  "env.set('ARCH', 'ARM')"),

  ( ('-?-wrap', '(.+)'),    "env.append('WRAP_SYMBOLS', $0)"),
  ( ('-?-wrap=(.+)'),       "env.append('WRAP_SYMBOLS', $0)"),

  # NOTE: For scons tests, the code generation fPIC flag is used with pnacl-ld.
  ( '-fPIC',               "env.set('PIC', '1')"),

  # This controls LTO optimization.
  ( '(-O.+)',              "env.set('OPT_LEVEL', $0)"),

  ( '-s',                  "env.append('STRIP_MODE', 'all')"),
  ( '--strip-all',         "env.append('STRIP_MODE', 'all')"),
  ( '-S',                  "env.append('STRIP_MODE', 'debug')"),
  ( '--strip-debug',       "env.append('STRIP_MODE', 'debug')"),

  # Inputs and options that need to be kept in order
  ( '(-l.*)',              "env.append('INPUTS', $0)"),
  ( '(--no-as-needed)',    "env.append('INPUTS', $0)"),
  ( '(--as-needed)',       "env.append('INPUTS', $0)"),
  ( '(--start-group)',     "env.append('INPUTS', $0)"),
  ( '(--end-group)',       "env.append('INPUTS', $0)"),
  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', $0)"),
]


def main(argv):
  ParseArgs(argv, LDPatterns)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = 'a.out'

  # If the user passed -arch, then they want native output.
  do_translate = (GetArch() is not None)

  # NATIVE HACK
  native_left, native_right = RemoveBitcode(inputs)
  if env.getbool('NATIVE_HACK'):
    inputs = native_left + native_right
  # END NATIVE HACK

  has_native, has_bitcode, arch = AnalyzeInputs(inputs)

  # If we auto-detected arch, set it.
  if arch:
    env.set('ARCH', arch)

  # If there is bitcode, we do a bitcode link. If -arch is also specified,
  # we invoke pnacl-translate afterwards.
  if has_bitcode:
    if env.getbool('SHARED'):
      bitcode_type = 'pso'
      native_type = 'so'
    else:
      bitcode_type = 'pexe'
      native_type = 'nexe'
  elif has_native:
    # If there is no bitcode, then we do a native link only.
    if not arch:
      Log.Fatal("Native linking requires -arch")
  else:
    Log.Fatal("No input objects")

  # Path A: Bitcode link, then opt, then translate.
  if has_bitcode:
    tng = TempNameGen([], output)
    chain = DriverChain(inputs, output, tng)
    chain.add(LinkBC, 'pre_opt.' + bitcode_type)
    if NeedsWrap():
      chain.add(WrapDIS, 'before_wrap.ll')
      chain.add(WrapLL, 'after_wrap.ll')
      chain.add(WrapAS, 'after_wrap.' + bitcode_type)
    if not env.getbool('SKIP_OPT'):
      chain.add(DoOPT, 'opt.' + bitcode_type)
    elif env.getone('STRIP_MODE') != 'none':
      chain.add(DoStrip, 'stripped.' + bitcode_type)
    if do_translate:
      # TODO(pdox): This should call pnacl-translate and nothing
      # else. However, since we still use native objects in the
      # scons and gcc builds, and special LD flags, we can't
      # translate without including these extra libraries/flags.
      chain.add(DoTranslate, 'o', mode = '-c')
      chain.add(DoNativeLink, native_type,
                native_left = native_left,
                native_right = native_right)
    chain.run()
    return 0
  else:
    # Path B: Native link only
    LinkNative(inputs, output)

  return 0

def IsLib(arg):
  return arg.startswith('-l')

def IsFlag(arg):
  return arg.startswith('--')

def RemoveBitcode(inputs):
  # Library order is important. We need to reinsert the
  # bitcode translation object in between the objects on
  # the left and the objects/libraries on the right.
  left = []
  right = []
  found_bc = False
  for f in inputs:
    if not IsFlag(f):
      if IsLib(f):
        path = FindLib(f[2:])
      else:
        path = f
      if FileType(path) in ['bclib','pso','bc','po']:
        found_bc = True
        continue
    if not found_bc:
      left.append(f)
    else:
      right.append(f)

  return (left,right)

# Determine the kind of input files.
# Make sure the input files are valid.
# Make sure the input files have matching architectures.
# Returns (has_native, has_bitcode, arch)
def AnalyzeInputs(inputs):
  has_native = False
  has_bitcode = False
  arch = GetArch()
  count = 0

  for f in inputs:
    if IsFlag(f):
      continue

    if IsLib(f):
      path = FindLib(f[2:])
    else:
      path = f

    intype = FileType(path)

    if intype in ['o','so','nlib']:
      has_native |= True
      info = GetELFInfo(path)
      if info:
        new_arch = FixArch(info[1])
        if arch and new_arch != arch:
          Log.Fatal("%s: File has wrong machine type %s, expecting %s",
                    path, new_arch, arch)
        arch = new_arch
      else:
        if intype == 'o':
          Log.Fatal("%s: Cannot read object file ELF header")
        elif intype == 'so':
          # This .so must be a linker script, so we can't easily see
          # what architecture it links.
          # TODO(pdox): Get this information somehow.
          pass
    elif intype in ['bc','bclib','pso','po']:
      has_bitcode |= True
    else:
      Log.Fatal("%s: Unexpected type of file for linking", f)
    count += 1

  if count == 0:
    Log.Fatal("no input files")

  return (has_native, has_bitcode, arch)

def RunLDSRPC():
  CheckPresenceSelUniversal()
  # The "main" input file is the application's combined object file.
  all_inputs = env.get('inputs')

  #############################################################
  # This is kind of arbitrary, but we need to treat one file as
  # the "main" file for sandboxed LD. When this code moves into
  # pnacl-translate, the selection of the main_input will be the
  # object emitted by llc. This make a lot more sense.
  objects = filter(lambda u: u.endswith('.o'), all_inputs)
  if len(objects) == 0:
    print "Sandboxed LD requires at least one .o file"
  main_input = objects[0]
  #############################################################

  outfile = env.getone('output')

  assert(main_input != '')
  files = LinkerFiles(all_inputs)
  ld_flags = env.get('LD_FLAGS') + env.get('LD_BFD_FLAGS')
  ld_script = env.eval('${LD_SCRIPT}')

  script = MakeSelUniversalScriptForLD(ld_flags,
                                       ld_script,
                                       main_input,
                                       files,
                                       outfile)

  RunWithLog('"${SEL_UNIVERSAL}" ${SEL_UNIVERSAL_FLAGS} -- ' +
             '"${LD_SRPC}"', stdin=script,
             echo_stdout = False, echo_stderr = False)

def MakeSelUniversalScriptForFile(filename):
  """ Return sel_universal script text for sending a commandline argument
      representing an input file to LD.nexe. """
  script = []
  basename = os.path.basename(filename)
  # A nice name for making a sel_universal variable.
  # Hopefully this does not clash...
  nicename = basename.replace('.','_').replace('-','_')
  script.append('echo "adding %s"' % basename)
  script.append('readonly_file %s %s' % (nicename, filename))
  script.append('rpc AddFile s("%s") h(%s) *' % (basename, nicename))
  script.append('rpc AddArg s("%s") *' % basename)
  return script


def MakeSelUniversalScriptForLD(ld_flags,
                                ld_script,
                                main_input,
                                files,
                                outfile):
  """ Return sel_universal script text for invoking LD.nexe with the
      given ld_flags, main_input (which is treated specially), and
      other input files. If ld_script is part of the ld_flags, it must be
      treated it as input file. The output will be written to outfile.  """
  script = []

  # Go through all the arguments and add them.
  # Based on the format of RUN_LD, the order of arguments is:
  # ld_flags, then input files (which are more sensitive to order).
  # Omit the "-o" for output so that it will use "a.out" internally.
  # We will take the fd from "a.out" and write it to the proper -o filename.
  for flag in ld_flags:
    if flag == ld_script:
      # Virtualize the file.
      script += MakeSelUniversalScriptForFile(ld_script)
    else:
      script.append('rpc AddArg s("%s") *' % flag)
    script.append('')

  # We need to virtualize these files.
  for f in files:
    if f == main_input:
      # Reload the temporary main_input object file into a new shmem region.
      basename = os.path.basename(f)
      script.append('file_size %s in_size' % f)
      script.append('shmem in_file in_addr ${in_size}')
      script.append('load_from_file %s ${in_addr} 0 ${in_size}' % f)
      script.append('rpc AddFileWithSize s("%s") h(in_file) i(${in_size}) *' %
                    basename)
      script.append('rpc AddArg s("%s") *' % basename)
    else:
      script += MakeSelUniversalScriptForFile(f)
    script.append('')

  script.append('rpc Link * h() i()')
  script.append('set_variable out_file ${result0}')
  script.append('set_variable out_size ${result1}')
  script.append('map_shmem ${out_file} out_addr')
  script.append('save_to_file %s ${out_addr} 0 ${out_size}' % outfile)
  script.append('echo "ld complete"')
  script.append('')
  return '\n'.join(script)

def DoOPT(infile, outfile):
  opt_flags = env.get('OPT_FLAGS')
  RunDriver('pnacl-opt', opt_flags + [ infile, '-o', outfile ])

def DoStrip(infile, outfile):
  strip_flags = env.get('STRIP_FLAGS')
  RunDriver('pnacl-strip', strip_flags + [ infile, '-o', outfile ])

def DoTranslate(infile, outfile, mode = ''):
  args = [infile, '-o', outfile]
  if mode:
    args += [mode]
  RunDriver('pnacl-translate', args)

def DoNativeLink(infile, outfile, native_left, native_right):
  LinkNative(native_left + [infile] + native_right, outfile)

def LinkBC(inputs, output):
  '''Input: a bunch of bc/o/lib input files
     Output: a combined & optimized bitcode file
  '''
  # There is an architecture bias here. To link the bitcode together,
  # we need to specify the native libraries to be used in the final linking,
  # just so the linker can resolve the symbols. We'd like to eliminate
  # this somehow.
  if not GetArch():
    env.set('ARCH', 'X8632')

  # Produce combined bitcode file
  RunWithEnv('${RUN_BCLD}',
             inputs=inputs,
             output=output)

######################################################################
# Bitcode Link Wrap Symbols Hack
######################################################################

def NeedsWrap():
  return len(env.get('WRAP_SYMBOLS')) > 0

def WrapDIS(infile, outfile):
  RunDriver('pnacl-dis', [infile, '-o', outfile], suppress_arch = True)

def WrapLL(infile, outfile):
  assert(FileType(infile) == 'll')
  symbols = env.get('WRAP_SYMBOLS')
  Log.Info('Wrapping symbols: ' + ' '.join(symbols))

  fpin = DriverOpen(infile, 'r')
  fpout = DriverOpen(outfile, 'w')
  while True:
    line = fpin.readline()
    if not line:
      break

    for s in symbols:
      # Relabel the real function
      if line.startswith('define'):
        line = line.replace('@' + s + '(', '@__real_' + s + '(')

      # Remove declarations of __real_xyz symbol.
      # Because we are actually defining it now, leaving the declaration around
      # would cause an error. (bitcode should have either a define or declare,
      # not both).
      if line.startswith('declare') and '__real_' in line:
        line = ''

      # Relabel the wrapper to the original name
      line = line.replace('@__wrap_' + s + '(', '@' + s + '(')
    fpout.write(line)
  fpin.close()
  fpout.close()


def WrapAS(infile, outfile):
  RunDriver('pnacl-as', [infile, '-o', outfile], suppress_arch = True)

######################################################################
# END Bitcode Link Wrap Symbols Hack
######################################################################


# Final native linking step
# TODO(pdox): Move this into pnacl-translator.
def LinkNative(inputs, output):
  env.push()
  env.set('inputs', *inputs)
  env.set('output', output)

  if env.getbool('SANDBOXED') and env.getbool('SRPC'):
    RunLDSRPC()
  else:
    RunWithLog('${RUN_LD}')

  env.pop()
  return

def FindLib(name):
  """Returns the full pathname for the library named 'name'.
     For example, name might be "c" or "m".
     Returns None if the library is not found.
  """
  searchdirs = env.get('SEARCH_DIRS')
  shared_ok = not env.getbool('STATIC')
  if shared_ok:
    extensions = [ 'pso', 'so', 'a' ]
  else:
    extensions = [ 'a' ]

  for curdir in searchdirs:
    for ext in extensions:
      guess = os.path.join(curdir, 'lib' + name + '.' + ext)
      if os.path.exists(guess):
        return guess

  Log.Fatal("Unable to find library '%s'", name)

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker.
def LinkerFiles(args):
  ret = []
  for f in args:
    if IsLib(f):
      libpath = FindLib(f[2:])
      ret.append(libpath)
      continue
    elif IsFlag(f):
      continue
    else:
      if not os.path.exists(f):
        Log.Fatal("Unable to open '%s'", f)
      ret.append(f)
  return ret

if __name__ == "__main__":
  DriverMain(main)
