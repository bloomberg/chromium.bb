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
import fcntl
import platform
import random
import hashlib

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
  'PIC'      : '0',

  'BAREBONES_LINK' : '0',

  'STRIP_MODE' : 'none',

  'STRIP_FLAGS'      : '${STRIP_FLAGS_%STRIP_MODE%}',
  'STRIP_FLAGS_all'  : '-s',
  'STRIP_FLAGS_debug': '-S',

  'PNACL_TRANSLATE_FLAGS': '${PIC ? -fPIC}',

  'OPT_FLAGS': '-O${OPT_LEVEL} ${OPT_STRIP_%STRIP_MODE%}',
  'OPT_LEVEL': '0',
  'OPT_STRIP_none': '',
  'OPT_STRIP_all': '-disable-opt --strip',
  'OPT_STRIP_debug': '-disable-opt --strip-debug',

  # To be filled in, if generating a static nexe with a segment gap.
  'SEGMENT_GAP_FLAGS': '',

  # Sandboxed LD is always BFD.
  'LD'          : '${SANDBOXED ? ${LD_SB} ${LD_BFD_FLAGS} ' +
                             ' : ${LD_%WHICH_LD%} ${LD_%WHICH_LD%_FLAGS}}',

  'LD_BFD_FLAGS': '-m ${LD_EMUL} ${SEGMENT_GAP_FLAGS} -T ${LD_SCRIPT}',

  'LD_GOLD_FLAGS': '--native-client --oformat ${LD_GOLD_OFORMAT} '
                   '${GOLD_FIX ? ${SEGMENT_GAP_FLAGS} -T ${LD_SCRIPT} : '
                   '-Ttext=0x20000}',

  'GOLD_PLUGIN_ARGS': '-plugin=${GOLD_PLUGIN_SO} ' +
                      '-plugin-opt=emit-llvm ' +
                      '${LIBMODE_NEWLIB && !BAREBONES_LINK ? ' +
                      '-plugin-opt=-add-nacl-read-tp-dependency ' +
                      '-plugin-opt=-add-libgcc-dependencies}',

   # Symbols to wrap
  'WRAP_SYMBOLS': '',

  # Common to both GOLD and BFD.
  'LD_FLAGS'       : '-nostdlib ${LD_SEARCH_DIRS} ' +
                     '${SHARED ? -shared} ${STATIC ? -static}',

  'LD_SEARCH_DIRS' : '',
  'SEARCH_DIRS'    : '',

  'LD_GOLD_OFORMAT'        : '${LD_GOLD_OFORMAT_%ARCH%}',
  'LD_GOLD_OFORMAT_ARM'    : 'elf32-littlearm',
  'LD_GOLD_OFORMAT_X8632'  : 'elf32-nacl',
  'LD_GOLD_OFORMAT_X8664'  : 'elf64-nacl',

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
               '-o "${output}"'),
}
env.update(EXTRA_ENV)

LDPatterns = [
  ( '--pnacl-native-hack', "env.set('NATIVE_HACK', '1')"),
  ( ('--add-translate-option', '(.+)'),
                       "env.append('PNACL_TRANSLATE_FLAGS', $0)"),
  # todo(dschuff): get rid of this when we get closer to tip and fix bug 1941
  ( ('--add-opt-option=(.+)'),
                       "env.append('OPT_FLAGS', $0)"),

  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-barebones-link',       "env.set('BAREBONES_LINK', '1')"),

  ( '-shared',         "env.set('SHARED', '1')"),

  ( '-static',         "env.set('STATIC', '1')\n"
                       "env.set('SHARED', '0')"),

  ( ('-L', '(.+)'),
    "env.append('SEARCH_DIRS', pathtools.normalize($0))\n"
    "env.append('LD_SEARCH_DIRS', '-L'+pathtools.normalize($0))"),
  ( '-L(.+)',
    "env.append('SEARCH_DIRS', pathtools.normalize($0))\n"
    "env.append('LD_SEARCH_DIRS', '-L'+pathtools.normalize($0))"),

  ( ('(-rpath)','(.*)'),
    "env.append('LD_FLAGS', $0+'='+pathtools.normalize($1))"),
  ( ('(-rpath)=(.*)'),
    "env.append('LD_FLAGS', $0+'='+pathtools.normalize($1))"),

  ( ('(-rpath-link)','(.*)'),
    "env.append('LD_FLAGS', $0+'='+pathtools.normalize($1))"),
  ( ('(-rpath-link)=(.*)'),
    "env.append('LD_FLAGS', $0+'='+pathtools.normalize($1))"),

  ( ('(-Ttext)','(.*)'), "env.append('LD_FLAGS', $0, $1)"),
  ( ('(-Ttext=.*)'),     "env.append('LD_FLAGS', $0)"),
  ( ('-T', '(.*)'),    "env.set('LD_SCRIPT', pathtools.normalize($0))"),

  ( ('-e','(.*)'),     "env.append('LD_FLAGS', '-e', $0)"),
  ( ('(--section-start)','(.*)'), "env.append('LD_FLAGS', $0, $1)"),
  ( '(-?-soname=.*)',             "env.append('LD_FLAGS', $0)"),
  ( ('(-?-soname)', '(.*)'),      "env.append('LD_FLAGS', $0, $1)"),
  ( '(--eh-frame-hdr)',           "env.append('LD_FLAGS', $0)"),

  ( '(--print-gc-sections)',      "env.append('LD_FLAGS', $0)"),
  ( '(-gc-sections)',             "env.append('LD_FLAGS', $0)"),

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
  # opt does not support -Os but internally it is identical to -O2
  # opt also does not support -O4 but -O4 is how you ask llvm-gcc for LTO, so we
  # can support it as well
  ( '-Os',                 "env.set('OPT_LEVEL', '2')"),
  ( '-O4',                 "env.set('OPT_LEVEL', '3')"),
  ( '-O([0-3])',           "env.set('OPT_LEVEL', $0)"),

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
  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', pathtools.normalize($0))"),
]


def main(argv):
  ParseArgs(argv, LDPatterns)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = pathtools.normalize('a.out')

  # If the user passed -arch, then they want native output.
  arch_flag_given = GetArch() is not None

  # Expand all -l flags
  ExpandLibFlags(inputs)

  # Expand input files which are linker scripts
  inputs = ExpandLinkerScripts(inputs)

  if ForceSegmentGap(inputs):
    # Note: we do not use the native bookend file link_segment_gap.o,
    # so we separately specify the segment gap size 0x10000000 (256 MB) here.
    env.append('SEGMENT_GAP_FLAGS', '--defsym=__nacl_rodata_start=0x10000000')

  # NATIVE HACK
  native_left, native_right = RemoveBitcode(inputs)
  if env.getbool('NATIVE_HACK'):
    inputs = native_left + native_right
  # END NATIVE HACK

  has_native, has_bitcode = AnalyzeInputs(inputs)

  # There is an architecture bias here. To link the bitcode together,
  # we need to specify the native libraries to be used in the final linking,
  # just so the linker can resolve the symbols.
  # TODO(pdox): This is a bias that we should try to remove.
  if GetArch() is None:
    env.set('ARCH', 'X8632')

  if env.getbool('SHARED') and env.getbool('LIBMODE_NEWLIB'):
    Log.Fatal("Cannot generate shared objects with newlib-based toolchain")

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
    if not arch_flag_given:
      Log.Fatal("Native linking requires -arch")
  else:
    Log.Fatal("No input objects")

  if env.getbool('LIBMODE_GLIBC'):
    TranslateInputs(inputs)

  # Path A: Bitcode link, then opt, then translate.
  if has_bitcode:
    tng = TempNameGen([], output)
    chain = DriverChain(inputs, output, tng)
    chain.add(LinkBC, 'pre_opt.' + bitcode_type)
    if NeedsWrap():
      chain.add(WrapDIS, 'before_wrap.ll')
      chain.add(WrapLL, 'after_wrap.ll')
      chain.add(WrapAS, 'after_wrap.' + bitcode_type)
    if env.getone('OPT_LEVEL') != '0':
      chain.add(DoOPT, 'opt.' + bitcode_type)
    elif env.getone('STRIP_MODE') != 'none':
      chain.add(DoStrip, 'stripped.' + bitcode_type)
    if arch_flag_given:
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
  return arg.startswith('-') and not IsLib(arg)

def IsUndefMarker(arg):
  return arg.startswith('--undefined=')

def ExpandLibFlags(inputs):
  for i in xrange(len(inputs)):
    f = inputs[i]
    if IsFlag(f):
      continue
    if IsLib(f):
      inputs[i] = FindLib(f)

def FindLinkerScript(inputs):
  for i in xrange(len(inputs)):
    f = inputs[i]
    if IsFlag(f):
      continue
    if FileType(f) == 'ldscript':
      return (i, f)

  return (None, None)

def ExpandLinkerScripts(inputs):
  while True:
    # Find a ldscript in the input list
    i, path = FindLinkerScript(inputs)

    # If none, we are done.
    if path is None:
      break

    new_inputs = ldtools.ParseLinkerScript(path)
    ExpandLibFlags(new_inputs)
    inputs = inputs[:i] + new_inputs + inputs[i+1:]

  # Handle --undefined=sym
  ret = []
  for f in inputs:
    if IsUndefMarker(f):
      env.append('LD_FLAGS', f)
    else:
      ret.append(f)

  return ret

def ForceSegmentGap(inputs):
  # Heuristic to detect when the nexe needs a segment gap (e.g., to be
  # compatible with the IRT). The heuristic is to check for -lppapi.
  # Currently, libppapi.a contains both bitcode and a native bookend
  # file "link_segment_gap.o", so is considered bitcode.
  # Thus, this must be checked before RemoveBitcode.
  if not env.getbool('STATIC'):
    # We only need this for static linking. In the dynamic linking case,
    # having a segment gap is already the default.
    return False
  for f in inputs:
    if IsFlag(f):
      continue
    if pathtools.basename(f) == 'libppapi.a':
      return True
  return False

def IsBitcodeInput(f):
  if IsFlag(f):
    return False
  return FileType(f) in ['bclib','po']

def RemoveBitcode(inputs):
  # Library order is important. We need to reinsert the
  # bitcode translation object in between the objects on
  # the left and the objects/libraries on the right.
  left = []
  right = []
  found_bc = False
  for f in inputs:
    if IsBitcodeInput(f):
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
  count = 0

  for f in inputs:
    if IsFlag(f):
      continue

    intype = FileType(f)

    if intype in ['o','so']:
      ArchMerge(f, True)
      has_native |= True
    elif intype in ['nlib']:
      # TODO(pdox): Pull ARCH info from inside native archives
      has_native |= True
    elif intype in ['pso']:
      pass
    elif intype in ['bclib','po']:
      has_bitcode |= True
    else:
      Log.Fatal("%s: Unexpected type of file for linking (%s)",
                pathtools.touser(f), intype)
    count += 1

  if count == 0:
    Log.Fatal("no input files")

  return (has_native, has_bitcode)

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
  ld_script = env.getone('LD_SCRIPT')

  script = MakeSelUniversalScriptForLD(ld_flags,
                                       ld_script,
                                       main_input,
                                       files,
                                       outfile)

  RunWithLog('${SEL_UNIVERSAL} ${SEL_UNIVERSAL_FLAGS} -- ' +
             '${LD_SRPC}', stdin=script,
             echo_stdout = False, echo_stderr = False)

def MakeSelUniversalScriptForFile(filename):
  """ Return sel_universal script text for sending a commandline argument
      representing an input file to LD.nexe. """
  script = []
  basename = pathtools.basename(filename)
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
      basename = pathtools.basename(f)
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
  args += env.get('PNACL_TRANSLATE_FLAGS')
  RunDriver('pnacl-translate', args)

def DoNativeLink(infile, outfile, native_left, native_right):
  LinkNative(native_left + [infile] + native_right, outfile)

def LinkBC(inputs, output):
  '''Input: a bunch of bc/o/lib input files
     Output: a combined & optimized bitcode file
  '''
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

      # Relabel the wrapper to the original name.
      line = line.replace('@__wrap_' + s + '(', '@' + s + '(')
      # Do the same renaming when the function appears in debug metadata.
      # Case: !123 = metadata !{i32 456, ... metadata !"__wrap_FOO", ..., \
      #              i32 (i32)* @__wrap_FOO} ; [ DW_TAG_subprogram ]
      line = line.replace('@__wrap_' + s + '}',
                          '@' + s + '}')
      line = line.replace('metadata !"__wrap_' + s + '"',
                          'metadata !"' + s + '"')
      # Case: !llvm.dbg.lv.__wrap_FOO = !{!789}
      line = line.replace('llvm.dbg.lv.__wrap_' + s + ' ',
                          'llvm.dbg.lv.' + s + ' ')

    fpout.write(line)
  fpin.close()
  fpout.close()


def WrapAS(infile, outfile):
  RunDriver('pnacl-as', [infile, '-o', outfile], suppress_arch = True)

######################################################################
# END Bitcode Link Wrap Symbols Hack
######################################################################


# This is a temporary hack.
#
# Until gold can recognize that .pso files should be treated like
# dynamic dependencies, we need to translate them first.
def TranslateInputs(inputs):
  arch = GetArch()

  for i in xrange(len(inputs)):
    f = inputs[i]
    if IsFlag(f):
      continue
    if FileType(f) == 'pso':
      inputs[i] = TranslatePSO(arch, f)

def TranslatePSO(arch, path):
  """ Translates a pso and returns the filename of the native so """
  assert(FileType(path) == 'pso')
  assert(arch)
  if not pathtools.exists(path):
    Log.Fatal("Couldn't open %s", pathtools.touser(path))
  path_dir = pathtools.dirname(path)
  cache_dir = pathtools.join(path_dir, 'pnacl_cache')
  try:
    os.mkdir(cache_dir)
  except OSError:
    pass

  output_base = pathtools.join(cache_dir, pathtools.basename(path)) + '.' + arch

  # Acquire a lock.
  lock = FileLock(output_base)
  if not lock:
    # Add a random number to the file name to prevent collisions
    output_base += '.%d' % random.getrandbits(32)

  output = '%s.so' % output_base

  # Make sure the existing file matches
  if lock:
    md5file = '%s.md5' % output_base
    md5 = GetMD5Sum(path)
    if pathtools.exists(output):
      existing_md5 = ReadMD5(md5file)
      if existing_md5 == md5:
        lock.release()
        return output

  # TODO(pdox): Some driver flags, in particular --pnacl-driver-set
  # and --pnacl-driver-append, could adversely affect the translation,
  # and should probably not be passed along. However, since this is a
  # temporary solution, and we never use those flags in combination
  # with glibc, we'll just let them pass for now.
  RunDriver('pnacl-translate',
            ['-no-save-temps',
            # -arch needs to be provided, whether or not
            # there was an -arch to this invocation.
             '-arch', arch, path, '-o', output],
            suppress_arch = True) # To avoid duplicate -arch
  if lock:
    WriteMD5(md5, md5file)
    lock.release()
  else:
    TempFiles.add(output)

  return output

def ReadMD5(file):
  fp = DriverOpen(file, 'r', fail_ok = True)
  if not fp:
    return ''
  s = fp.read()
  fp.close()
  return s

def WriteMD5(s, file):
  fp = DriverOpen(file, 'w')
  fp.write(s)
  fp.close()

def GetMD5Sum(path):
  m = hashlib.md5()
  fp = DriverOpen(path, 'r')
  m.update(fp.read())
  fp.close()
  return m.hexdigest()

def FileLock(filename):
  if platform.system().lower() == "windows":
    return None
  else:
    return FileLockUnix(filename)

class FileLockUnix(object):
  def __init__(self, path):
    self.lockfile = path + '.lock'
    self.fd = None
    self.acquire()

  def acquire(self):
    fd = DriverOpen(self.lockfile, 'w+')
    fcntl.lockf(fd, fcntl.LOCK_EX)
    self.fd = fd

  def release(self):
    if self.fd is not None:
      fcntl.lockf(self.fd, fcntl.LOCK_UN)
      self.fd.close()
    self.fd = None

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

def FindLib(arg):
  """Returns the full pathname for the library input.
     For example, name might be "-lc" or "-lm".
     Returns None if the library is not found.
  """
  assert(IsLib(arg))
  name = arg[len('-l'):]

  searchdirs = env.get('SEARCH_DIRS')
  searchnames = []

  if name[0] == ':':
    # -l:filename  (search for the filename)
    name = name[1:]
    searchnames.append(name)

    # .pso may exist in lieu of .so, or vice versa.
    if '.so' in name:
      searchnames.append(name.replace('.so', '.pso'))
    if '.pso' in name:
      searchnames.append(name.replace('.pso', '.so'))
  else:
    # -lfoo
    shared_ok = not env.getbool('STATIC')
    if shared_ok:
      extensions = [ 'pso', 'so', 'a' ]
    else:
      extensions = [ 'a' ]
    for ext in extensions:
      searchnames.append('lib' + name + '.' + ext)

  for curdir in searchdirs:
    for name in searchnames:
      guess = pathtools.join(curdir, name)
      if pathtools.exists(guess):
        return guess

  Log.Fatal("Unable to find library '%s'", arg)

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker.
def LinkerFiles(args):
  ret = []
  for f in args:
    if IsFlag(f):
      continue
    else:
      if not pathtools.exists(f):
        Log.Fatal("Unable to open '%s'", pathtools.touser(f))
      ret.append(f)
  return ret

if __name__ == "__main__":
  DriverMain(main)
