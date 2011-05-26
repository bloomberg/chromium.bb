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

import os
import re
import subprocess
import sys
import signal
import platform
from threading import Thread, Event
from Queue import Queue, Empty

######################################################################
#
# Environment Settings
#
######################################################################

# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class env" defined later for the implementation.

INITIAL_ENV = {
  # These are filled in by env.reset()
  # TODO(pdox): It should not be necessary to auto-detect these here. Change
  #             detection method so that toolchain location is relative.
  'BASE_NACL'       : '',   # Absolute path of native_client/ dir
  'BASE_DRIVER'     : '',   # Location of PNaCl drivers
  'BUILD_OS'        : '',   # "linux" or "darwin"
  'BUILD_ARCH'      : '',   # "x86_64" or "i686" or "i386"

  # Directories
  'BASE_TOOLCHAIN'  : '${BASE_NACL}/toolchain',
  'BASE'            : '${BASE_TOOLCHAIN}/pnacl_${BUILD_OS}_${BUILD_ARCH}',
  'BASE_TRUSTED'    : '${BASE_TOOLCHAIN}/linux_arm-trusted',
  'BASE_ARM'        : '${BASE}/arm-none-linux-gnueabi',
  'BASE_ARM_INCLUDE': '${BASE_ARM}/arm-none-linux-gnueabi/include',
  'BASE_LLVM_BIN'   : '${BASE_ARM}/bin',
  'BASE_BIN'        : '${BASE}/bin',
  'BASE_SB'         : '${BASE_SB_%ARCH%}',
  'BASE_SB_X8632'   : '${BASE}/tools-sb/x8632',
  'BASE_SB_X8664'   : '${BASE}/tools-sb/x8664',
  'BASE_SB_ARM'     : '${BASE}/tools-sb/arm',
  'SCONS_OUT'       : '${BASE_NACL}/scons-out',

  # Driver settings
  'ARCH'        : '',     # Target architecture
  'DRY_RUN'     : '0',
  'DEBUG'       : '0',    # Print out internal actions
  'RECURSE'     : '0',    # In a recursive driver call
  'CLEANUP'     : '0',    # Clean up temporary files
                          # TODO(pdox): Enable for SDK version
  'SANDBOXED'   : '0',    # Use sandboxed toolchain for this arch. (main switch)
  'SRPC'        : '1',    # Use SRPC sandboxed toolchain
  'GOLD_FIX'    : '0',    # Use linker script instead of -Ttext for gold.
                          # Needed for dynamic_code_loading tests which create
                          # a gap between text and rodata.
                          # TODO(pdox): Either eliminate gold native linking or
                          #             figure out why this is broken in the
                          #             first place.
  'SKIP_OPT'            : '0',   # Skip llvm-opt after BC linking
  'MC_DIRECT'           : '1',
  'USE_EMULATOR'        : '0',
  'DRIVER_FLAGS'        : '', # Flags passed to the driver
  'USE_GLIBC'           : '0', # Use glibc instead of newlib

  # Logging settings
  'LOG_TO_FILE'          : '1',
  'LOG_FILENAME'         : '${BASE_TOOLCHAIN}/hg-log/driver.log',
  'LOG_FILE_SIZE_LIMIT'  : str(20 * 1024 * 1024),
  'LOG_PRETTY_PRINT'     : '1',

   # Conventions
  'SO_EXT'          : '${SO_EXT_%BUILD_OS%}',
  'SO_EXT_darwin'   : '.dylib',
  'SO_EXT_linux'    : '.so',

  'SCONS_OS'            : '${SCONS_OS_%BUILD_OS%}',
  'SCONS_OS_linux'      : 'linux',
  'SCONS_OS_darwin'     : 'mac',

  # Tool Pathnames
  'GOLD_PLUGIN_SO'  : '${BASE_ARM}/lib/libLLVMgold${SO_EXT}',
  'GOLD_PLUGIN_ARGS': '-plugin=${GOLD_PLUGIN_SO} ' +
                      '-plugin-opt=emit-llvm',

  'SCONS_STAGING'       : '${SCONS_STAGING_%ARCH%}',
  'SCONS_STAGING_X8632' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-32/staging',
  'SCONS_STAGING_X8664' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-64/staging',
  'SCONS_STAGING_ARM'   : '${SCONS_OUT}/opt-${SCONS_OS}-arm/staging',

  'SEL_UNIVERSAL_PREFIX': '${USE_EMULATOR ? ${EMULATOR}}',
  'SEL_UNIVERSAL'       : '${SCONS_STAGING}/sel_universal',
  'SEL_UNIVERSAL_FLAGS' : '--abort_on_error ' +
                          '${USE_EMULATOR ? -Q --command-prefix ${EMULATOR}}',

  'EMULATOR'            : '${EMULATOR_%ARCH%}',
  'EMULATOR_X8632'      : '',
  'EMULATOR_X8664'      : '',
  'EMULATOR_ARM'        : '${BASE_TRUSTED}/run_under_qemu_arm',

  'LLVM_MC'       : '${BASE_LLVM_BIN}/llvm-mc',
  'LLVM_AS'       : '${BASE_LLVM_BIN}/llvm-as',

  'SEL_LDR'       : '${SCONS_STAGING}/sel_ldr',

  'AS_SB'         : '${SEL_LDR} -a -- ${BASE_SB}/nonsrpc/bin/as',
  'AS_ARM'        : '${BINUTILS_BASE}as',
  'AS_X8632'      : '${BASE_TOOLCHAIN}/${SCONS_OS}_x86_newlib/bin/nacl-as',
  'AS_X8664'      : '${BASE_TOOLCHAIN}/${SCONS_OS}_x86_newlib/bin/nacl64-as',

  'LD_SB'         : '${SEL_LDR} -a -- ${BASE_SB}/nonsrpc/bin/ld',
  'LLC_SB'        : '${SEL_LDR} -a -- ${BASE_SB}/nonsrpc/bin/llc',

  'LLC_SRPC'      : '${BASE_SB}/srpc/bin/llc',
  'LD_SRPC'       : '${BASE_SB}/srpc/bin/ld',

  'LLVM_GCC'      : '${BASE_LLVM_BIN}/arm-none-linux-gnueabi-gcc',
  'LLVM_GXX'      : '${BASE_LLVM_BIN}/arm-none-linux-gnueabi-g++',
  'LLVM_OPT'      : '${BASE_LLVM_BIN}/opt',
  'LLVM_LLC'      : '${BASE_LLVM_BIN}/llc',
  'LLVM_LD'       : '${BASE_LLVM_BIN}/llvm-ld',
  'LLVM_DIS'      : '${BASE_LLVM_BIN}/llvm-dis',
  'LLVM_LINK'     : '${BASE_LLVM_BIN}/llvm-link',

  'BINUTILS_BASE'  : '${BASE_ARM}/bin/arm-pc-nacl-',
  'OBJDUMP'        : '${BINUTILS_BASE}objdump',
  'NM'             : '${BINUTILS_BASE}nm',
  'AR'             : '${BINUTILS_BASE}ar',
  'RANLIB'         : '${BINUTILS_BASE}ranlib',
  'STRIP'          : '${BINUTILS_BASE}strip',

  'LD_BFD'         : '${BINUTILS_BASE}ld.bfd',
  'LD_GOLD'        : '${BINUTILS_BASE}ld.gold',
}




DriverPatterns = [
  ( '--pnacl-driver-verbose',             "Log.LOG_OUT.append(sys.stderr)"),
  ( '--pnacl-driver-debug',               "env.set('DEBUG', '1')"),
  ( '--pnacl-driver-recurse',             "env.set('RECURSE', '1')"),
  ( '--pnacl-driver-save-temps',          "env.set('CLEANUP', '0')"),
  ( '--pnacl-driver-set-([^=]+)=(.*)',    "env.set($0, $1)"),
  ( '--pnacl-driver-append-([^=]+)=(.*)', "env.append($0, $1)"),
  ( ('-arch', '(.+)'),                 "env.set('ARCH', FixArch($0))"),
  ( ('--add-llc-option', '(.+)'),      "env.append('LLC_FLAGS_COMMON', $0)"),
  ( '--pnacl-sb',                      "env.set('SANDBOXED', '1')"),
  ( '--pnacl-use-emulator',            "env.set('USE_EMULATOR', '1')"),
  ( '--dry-run',                       "env.set('DRY_RUN', '1')"),
  ( '--pnacl-gold-fix',                "env.set('GOLD_FIX', '1')"),
  ( '--pnacl-arm-bias',                "env.set('BIAS', 'ARM')"),
  ( '--pnacl-i686-bias',               "env.set('BIAS', 'X8632')"),
  ( '--pnacl-x86_64-bias',             "env.set('BIAS', 'X8664')"),
  ( '--pnacl-bias=(.+)',               "env.set('BIAS', FixArch($0))"),
  ( '--pnacl-skip-lto',                "env.set('SKIP_OPT', '1')"),
  ( '--pnacl-use-glibc',               "env.set('USE_GLIBC', '1')"),
 ]


######################################################################
#
# Environment
#
######################################################################

class env(object):
  data = {}
  stack = []

  @classmethod
  def update(cls, extra):
    INITIAL_ENV.update(extra)
    assert(cls) # This makes the pychecker "unused variable" warning go away

  @classmethod
  def reset(cls):
    cls.data = dict(INITIAL_ENV)
    cls.set('BASE_NACL', FindBaseDir())
    cls.set('BASE_DRIVER', FindBaseDriver())
    cls.set('BUILD_OS', GetBuildOS())
    cls.set('BUILD_ARCH', GetBuildArch())
    cls.set('DRIVER_EXT', GetDriverExt())

  @classmethod
  def dump(cls):
    for (k,v) in cls.data.iteritems():
      print '%s == %s' % (k,v)

  @classmethod
  def push(cls):
    cls.stack.append(cls.data)
    cls.data = dict(cls.data) # Make a copy

  @classmethod
  def pop(cls):
    cls.data = cls.stack.pop()

  @classmethod
  def has(cls, varname):
    return varname in cls.data

  @classmethod
  def getraw(cls, varname):
    return cls.eval(cls.data[varname])

  # Evaluate a variable from the environment.
  # Returns a list of terms.
  @classmethod
  def get(cls, varname):
    return shell.split(cls.getraw(varname))

  # Retrieve a variable from the environment which
  # is a single term. Returns a string.
  @classmethod
  def getone(cls, varname):
    w = cls.get(varname)
    if len(w) == 1:
      return w[0]
    if len(w) == 0:
      return ''
    assert(False)

  @classmethod
  def getbool(cls, varname):
    return bool(int(cls.getone(varname)))

  # Set a variable in the environment without shell-escape
  @classmethod
  def setraw(cls, varname, val):
    cls.data[varname] = val

  # Set one or more variables using named arguments
  @classmethod
  def setmany(cls, **kwargs):
    for k,v in kwargs.iteritems():
      if isinstance(v, str):
        cls.set(k, v)
      else:
        cls.set(k, *v)

  @classmethod
  def clear(cls, varname):
    cls.data[varname] = ''

  # Set a variable to one or more terms, applying shell-escape.
  @classmethod
  def set(cls, varname, *vals):
    cls.clear(varname)
    cls.append(varname, *vals)

  # Append one or more terms to a variable in the
  # environment, applying shell-escape.
  @classmethod
  def append(cls, varname, *vals):
    escaped = [ shell.escape(v) for v in vals ]
    if len(cls.data[varname]) > 0:
      cls.data[varname] += ' '
    cls.data[varname] += ' '.join(escaped)

  # Evaluate an expression s
  @classmethod
  def eval(cls, s):
    (result, i) = cls.eval_expr(s, 0, [])
    assert(i == len(s))
    return result

  ######################################################################
  # EXPRESSION EVALUATION CODE
  # Context Free Grammar:
  #
  # str = empty | string literal
  # expr = str | expr '$' '{' bracket_expr '}' expr
  # bracket_expr = varname | boolname ? expr | boolname ? expr : expr
  # boolname = varname | !varname | #varname | !#varname
  # varname = str | varname '%' bracket_expr '%' varname
  #
  # Do not call these functions outside of class env.
  # The env.eval method is the external interface to the evaluator.
  ######################################################################

  # Evaluate %var% substitutions inside a variable name.
  # Returns (the_actual_variable_name, endpos)
  # Terminated by } character
  @classmethod
  def eval_varname(cls, s, pos, terminators):
    #print "eval_varname(%s,%d,%s)" % (s, pos, ' '.join(terminators))
    (_,i) = FindFirst(s, pos, ['%'] + terminators)
    leftpart = s[pos:i].strip(' ')
    if i == len(s) or s[i] in terminators:
      return (leftpart, i)

    (middlepart, j) = cls.eval_bracket_expr(s, i+1, ['%'])
    if j == len(s) or s[j] != '%':
      ParseError(s, i, j, "Unterminated %")

    (rightpart, k) = cls.eval_varname(s, j+1, terminators)

    fullname = leftpart + middlepart + rightpart
    fullname = fullname.strip(' ')
    return (fullname, k)

  # Evaluate the inside of a ${} or %%.
  # Returns the (the_evaluated_string, endpos)
  @classmethod
  def eval_bracket_expr(cls, s, pos, terminators):
    #print "eval_bracket_expr(%s,%d,%s)" % (s, pos, ' '.join(terminators))
    i = pos

    if s[i] == '!':
      negated = True
      i += 1
    else:
      negated = False

    if s[i] == '#':
      uselen = True
      i += 1
    else:
      uselen = False


    (varname, j) = cls.eval_varname(s, i, ['?']+terminators)
    if j == len(s):
      # This is an error condition one level up. Don't evaluate anything.
      return ('', j)

    if varname not in cls.data:
      ParseError(s, i, j, "Undefined variable '%s'" % varname)
    vardata = cls.data[varname]

    (contents, _) = cls.eval_expr(vardata, 0, terminators)

    if s[j] != '?':
      endpos = j
    else:
      # Ternary Mode
      if uselen:
        boolval = (len(contents) != 0)
      else:
        if contents not in ('0','1'):
          ParseError(s, j, j,
            "%s evaluated to %s, which is not a boolean!" % (varname, contents))
        boolval = bool(int(contents))
      is_cond_true = negated ^ boolval
      (if_true_expr, k) = cls.eval_expr(s, j+1, [':']+terminators)
      if k == len(s):
        # This is an error condition one level up.
        return ('', k)

      if s[k] == ':':
        (if_false_expr,k) = cls.eval_expr(s, k+1, terminators)
        if k == len(s):
          # This is an error condition one level up.
          return ('', k)
      else:
        if_false_expr = ''

      if is_cond_true:
        contents = if_true_expr.strip(' ')
      else:
        contents = if_false_expr.strip(' ')
      endpos = k
    return (contents, endpos)

  # Evaluate an expression with ${} in string s, starting at pos.
  # Returns (the_evaluated_expression, endpos)
  @classmethod
  def eval_expr(cls, s, pos, terminators):
    #print "eval_expr(%s,%d,%s)" % (s, pos, ' '.join(terminators))
    (m,i) = FindFirst(s, pos, ['${'] + terminators)
    leftpart = s[pos:i]
    if i == len(s) or m in terminators:
      return (leftpart, i)

    (middlepart, j) = cls.eval_bracket_expr(s, i+2, ['}'])
    if j == len(s) or s[j] != '}':
      ParseError(s, i, j, 'Unterminated ${')

    (rightpart, k) = cls.eval_expr(s, j+1, terminators)
    return (leftpart + middlepart + rightpart, k)

  ######################################################################
  # END EXPRESSION EVALUATION CODE
  ######################################################################

def ParseError(s, leftpos, rightpos, msg):
  Log.Error("Parse Error: %s", msg);
  Log.Error('  ' + s);
  Log.Error('  ' + (' '*leftpos) + ('^'*(rightpos - leftpos + 1)))
  DriverExit(1)

# Run a command with extra environment settings
def RunWithEnv(cmd, **kwargs):
  RunWithLogArgs = { }
  if 'RunWithLogArgs' in kwargs:
    RunWithLogArgs = kwargs['RunWithLogArgs']
    del kwargs['RunWithLogArgs']

  env.push()
  env.setmany(**kwargs)
  RunWithLog(cmd, **RunWithLogArgs)
  env.pop()

def RunDriver(invocation, args, suppress_arch = False):
  if isinstance(args, str):
    args = shell.split(env.eval(args))

  script = env.eval('${BASE_DRIVER}/%s${DRIVER_EXT}' % invocation)
  driver_args = env.get('DRIVER_FLAGS')

  if '--pnacl-driver-recurse' not in driver_args:
    driver_args.append('--pnacl-driver-recurse')

  # Get rid of -arch <arch> in the driver flags.
  if suppress_arch:
    while '-arch' in driver_args:
      i = driver_args.index('-arch')
      driver_args = driver_args[:i] + driver_args[i+2:]

  cmd = [script] + driver_args + args

  # The invoked driver will do it's own logging, so
  # don't use RunWithLog() for the recursive driver call.
  # Use Run() so that the subprocess's stdout/stderr
  # will go directly to the real stdout/stderr.
  Log.Debug('-' * 60)
  Log.Debug('\n' + StringifyCommand(cmd))
  Run(cmd)

# Find the leftmost position in "s" which begins a substring
# in "strset", starting at "pos".
# For example:
#   FindFirst('hello world', 0, ['h','o']) = ('h', 0)
#   FindFirst('hello world', 1, ['h','o']) = ('o', 4)
#   FindFirst('hello world', 0, ['x']) = (None,11)
def FindFirst(s, pos, strset):
  m = {}
  for ss in strset:
    m[s.find(ss, pos)] = ss
  if -1 in m:
    del m[-1]
  if len(m) == 0:
    return (None, len(s))
  pos = min(m)
  return (m[pos], pos)


def GetBuildOS():
  name = platform.system().lower()
  if name not in ('linux', 'darwin'):
    Log.Fatal("Unsupported platform '%s'" % (name,))
  return name

def GetBuildArch():
  return platform.machine()

# Crawl backwards, starting from the directory containing this script,
# until we find the native_client/ directory.
def FindBaseDir():
  Depth = 0
  cur = os.path.abspath(sys.argv[0])
  while os.path.basename(cur) != 'native_client' and \
        Depth < 16:
    cur = os.path.dirname(cur)
    Depth += 1

  if os.path.basename(cur) != "native_client":
    print "Unable to find native_client directory!"
    sys.exit(1)

  return cur

def FindBaseDriver():
  return os.path.dirname(os.path.abspath(sys.argv[0]))

def GetDriverExt():
  return os.path.splitext(sys.argv[0])[1]

######################################################################
#
# Argument Parser
#
######################################################################

def ParseArgs(argv, patternlist, must_match = True):
  """ Parse argv using the patterns in patternlist
      Returns: (matched, unmatched)
  """
  matched = []
  unmatched = []
  i = 0
  while i < len(argv):
    num_matched, action, groups = MatchOne(argv, i, patternlist)
    if num_matched == 0:
      if must_match:
        Log.Fatal('Unrecognized argument: ' + argv[i])
      else:
        unmatched.append(argv[i])
      i += 1
      continue
    matched += argv[i:i+num_matched]
    if isinstance(action, str):
      # Perform $N substitution
      for g in xrange(0, len(groups)):
        action = action.replace('$%d' % g, 'groups[%d]' % g)
    try:
      if isinstance(action, str):
        # NOTE: this is essentially an eval for python expressions
        # which does rely on the current environment for unbound vars
        # Log.Info('about to exec [%s]', str(action))
        exec(action)
      else:
        action(*groups)
    except Exception, err:
      Log.Fatal('ParseArgs action [%s] failed with: %s', action, err)
    i += num_matched
  return (matched, unmatched)

def MatchOne(argv, i, patternlist):
  """Find a pattern which matches argv starting at position i"""
  for (regex, action) in patternlist:
    if isinstance(regex, str):
      regex = [regex]
    j = 0
    matches = []
    for r in regex:
      if i+j < len(argv):
        match = re.compile('^'+r+'$').match(argv[i+j])
      else:
        match = None
      matches.append(match)
      j += 1
    if None in matches:
      continue
    groups = [ list(m.groups()) for m in matches ]
    groups = reduce(lambda x,y: x+y, groups, [])
    return (len(regex), action, groups)
  return (0, '', [])

def UnrecognizedOption(*args):
  Log.Fatal("Unrecognized option: " + ' '.join(args))

######################################################################
# File Type Tools
######################################################################

def SimpleCache(f):
  """ Cache results of a one-argument function using a dictionary """
  cache = dict()
  def wrapper(arg):
    if arg in cache:
      return cache[arg]
    else:
      result = f(arg)
      cache[arg] = result
      return result
  wrapper.__name__ = f.__name__
  wrapper.__cache = cache
  return wrapper

@SimpleCache
def IsBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(2)
  DriverClose(fp)
  if header == 'BC':
    return True
  return False

ELF_MAGIC = '\x7fELF'
ELF_TYPES = { 1: 'REL',  # .o
              2: 'EXEC', # .exe
              3: 'DYN' } # .so
ELF_MACHINES = {  3: '386',
                 40: 'ARM',
                 62: 'X86_64' }
ELF_OSABI = { 0: 'UNIX',
              3: 'LINUX',
              123: 'NACL' }
ELF_ABI_VER = { 0: 'NONE',
                7: 'NACL' }

# If the file is not ELF, returns None.
# Otherwise, returns:
#   (e_type, e_machine, e_osabi, e_abi_ver)
# Example:
#  GetELFInfo('/bin/bash') == ('EXEC', 'X86_64', 'UNIX', 'NONE')
#  GetELFInfo('bundle_size.o') == ('REL', '386', 'NACL', 'NACL')
@SimpleCache
def GetELFInfo(filename):
  # Pull e_ident, e_type, e_machine

  fp = DriverOpen(filename, 'rb')
  header = fp.read(16 + 2 + 2)
  DriverClose(fp)
  if header[0:4] != ELF_MAGIC:
    return None
  e_osabi = DecodeLE(header[7])
  e_abiver = DecodeLE(header[8])
  e_type = DecodeLE(header[16:18])
  e_machine = DecodeLE(header[18:20])

  if e_osabi not in ELF_OSABI:
    Log.Fatal('%s: ELF file has unknown OS ABI (%d)', filename, e_osabi)
  if e_abiver not in ELF_ABI_VER:
    Log.Fatal('%s: ELF file has unknown ABI version (%d)', filename, e_abiver)
  if e_type not in ELF_TYPES:
    Log.Fatal('%s: ELF file has unknown type (%d)', filename, e_type)
  if e_machine not in ELF_MACHINES:
    Log.Fatal('%s: ELF file has unknown machine type (%d)', filename, e_machine)

  return (ELF_TYPES[e_type], ELF_MACHINES[e_machine],
          ELF_OSABI[e_osabi], ELF_ABI_VER[e_abiver])

def IsELF(filename):
  return GetELFInfo(filename) is not None

# Decode Little Endian bytes into an unsigned value
def DecodeLE(bytes):
  value = 0
  for b in reversed(bytes):
    value *= 2
    value += ord(b)
  return value

# If ForcedFileType is set, FileType() will return ForcedFileType for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files to be treated
# in a certain way.
ForcedFileType = None
def SetForcedFileType(t):
  global ForcedFileType
  ForcedFileType = t

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if ForcedFileType is None:
      return
    newtype = ForcedFileType
  FileType.__cache[filename] = newtype

# File Extension -> Type string
ExtensionMap = {
  'c'   : 'src',
  'cc'  : 'src',
  'cpp' : 'src',
  'C'   : 'src',
  'll'  : 'll',
  'bc'  : 'bc',
  'po'  : 'bc',   # .po = "Portable object file"
  'pexe': 'pexe', # .pexe = "Portable executable"
  'pso' : 'pso',  # .pso = "Portable Shared Object"
  'asm' : 'S',
  'S'   : 'S',
  's'   : 's',
  'o'   : 'o',
  'os'  : 'o',
  'so'  : 'so',
  'nexe': 'nexe',
}

# The SimpleCache decorator is required for correctness, due to the
# ForceFileType mechanism.
@SimpleCache
def FileType(filename):
  # Auto-detect bitcode files, since we can't rely on extensions
  ext = filename.split('.')[-1]
  if ext in ('o', 'so') and IsBitcode(filename):
    return 'bc'

  # This is a total hack. We assume all archives contain
  # bitcode except for the ones in libs-arm*, and libs-x86*.
  # TODO(pdox): We need some way to actually detect
  # whether .a files contain bitcode or objects.
  if ext == 'a':
    if ('libs-arm' in filename or
        'libs-x86' in filename):
      return 'nlib'
    return 'bclib'

  if ext not in ExtensionMap:
    Log.Fatal('Unknown file extension: %s' % filename)

  return ExtensionMap[ext]

######################################################################
#
# File Naming System (Temp files & Output files)
#
######################################################################

def DefaultOutputName(filename, outtype):
  base = os.path.basename(filename)
  base = RemoveExtension(base)

  if outtype in ('pp','dis'): return '-'; # stdout
  if outtype in ('bc'): return base + '.o'

  assert(outtype in ExtensionMap.values())
  assert(outtype != 'src')

  return base + '.' + outtype

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = os.path.splitext(filename)
  if ext == '':
    Log.Fatal('File has no extension: ' + filename)
  return name

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = os.path.split(cur)
    if piece == '':
      break
    paths.append(piece)
  paths.reverse()
  return paths

# Generate a unique identifier for each input file.
# Start with the basename, and if that is not unique enough,
# add parent directories. Rinse, repeat.
class TempNameGen(object):
  def __init__(self, inputs, output):
    inputs = [ os.path.abspath(i) for i in inputs ]
    output = os.path.abspath(output)

    self.TempBase = output + '---linked'
    self.TempList = []

    # TODO(pdox): Figure out if there's a less confusing way
    #             to simplify the intermediate filename in this case.
    #if len(inputs) == 1:
    #  # There's only one input file, don't bother adding the source name.
    #  TempMap[inputs[0]] = output + '---'
    #  return

    # Build the initial mapping
    self.TempMap = dict()
    for f in inputs:
      if f.startswith('-'):
        continue
      path = PathSplit(f)
      self.TempMap[f] = [1, path]

    while True:
      # Find conflicts
      ConflictMap = dict()
      Conflicts = set()
      for (f, [n, path]) in self.TempMap.iteritems():
        candidate = output + '---' + '_'.join(path[-n:]) + '---'
        if candidate in ConflictMap:
          Conflicts.add(ConflictMap[candidate])
          Conflicts.add(f)
        else:
          ConflictMap[candidate] = f

      if len(Conflicts) == 0:
        break

      # Resolve conflicts
      for f in Conflicts:
        n = self.TempMap[f][0]
        if n+1 > len(self.TempMap[f][1]):
          Log.Fatal('Unable to resolve naming conflicts')
        self.TempMap[f][0] = n+1

    # Clean up the map
    NewMap = dict()
    for (f, [n, path]) in self.TempMap.iteritems():
      candidate = output + '---' + '_'.join(path[-n:]) + '---'
      NewMap[f] = candidate
    self.TempMap = NewMap
    return

  def TempNameForOutput(self, imtype):
    temp = self.TempBase + '.' + imtype
    self.TempList.append(temp)
    return temp

  def TempNameForInput(self, input, imtype):
    fullpath = os.path.abspath(input)
    # If input is already a temporary name, just change the extension
    if fullpath.startswith(self.TempBase):
      temp = self.TempBase + '.' + imtype
    else:
      # Source file
      temp = self.TempMap[fullpath] + '.' + imtype

    self.TempList.append(temp)
    return temp

  def __del__(self):
    if env.getbool('CLEANUP'):
      for f in self.TempList:
        os.remove(f)
      self.TempList = []

######################################################################
#
# Shell Utilities
#
######################################################################

class shell(object):
  @staticmethod
  def split(s):
    """Split a shell-style string up into a list of distinct arguments.
    For example: split('cmd -arg1 -arg2="a b c"')
    Returns ['cmd', '-arg1', '-arg2=a b c']
    """

    out = []
    inspace = True
    inquote = False
    buf = ''

    i = 0
    while i < len(s):
      if s[i] == '"':
        inspace = False
        inquote = not inquote
      elif s[i] == ' ' and not inquote:
        if not inspace:
          out.append(buf)
          buf = ''
        inspace = True
      elif s[i] == '\\':
        if not i+1 < len(s):
          Log.Fatal('Unterminated \\ escape sequence')
        inspace = False
        i += 1
        buf += s[i]
      else:
        inspace = False
        buf += s[i]
      i += 1
    if inquote:
      Log.Fatal('Unterminated quote')
    if not inspace:
      out.append(buf)
    return out

  @staticmethod
  def join(args):
    """Turn a list into a shell-style string For example:
       shell.join([ 'a', 'b', 'c d e' ]) = 'a b "c d e"'
    """
    if isinstance(args, str):
      return args

    out = ''
    for a in args:
      out += shell.escape(a) + ' '

    return out[0:-1]

  @staticmethod
  def escape(s):
    """Shell-escape special characters in a string
       Surround with quotes if necessary
    """
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    if ' ' in s:
      s = '"' + s + '"'
    return s

######################################################################
#
# Logging
#
######################################################################

class Log(object):
  # Lists of streams
  prefix = ''
  LOG_OUT = []
  ERROR_OUT = [sys.stderr]

  @classmethod
  def reset(cls):
    if env.getbool('LOG_TO_FILE'):
      filename = env.getone('LOG_FILENAME')
      sizelimit = int(env.getone('LOG_FILE_SIZE_LIMIT'))
      cls.AddFile(filename, sizelimit)

  @classmethod
  def AddFile(cls, filename, sizelimit):
    file_too_big = os.path.isfile(filename) and \
                   os.path.getsize(filename) > sizelimit
    mode = 'a'
    if file_too_big:
      mode = 'w'
    fp = DriverOpen(filename, mode, fail_ok = True)
    if fp:
      cls.LOG_OUT.append(fp)

  @classmethod
  def Banner(cls, argv):
    if not env.getbool('RECURSE'):
      cls.Info('-' * 60)
      cls.Info('PNaCl Driver Invoked With:\n' + StringifyCommand(argv))

  @classmethod
  def Info(cls, m, *args):
    cls.LogPrint(m, *args)

  @classmethod
  def Error(cls, m, *args):
    cls.ErrorPrint(m, *args)

  @classmethod
  def FatalWithResult(cls, ret, m, *args):
    m = 'FATAL: ' + m
    cls.LogPrint(m, *args)
    cls.ErrorPrint(m, *args)
    DriverExit(ret)

  @classmethod
  def Debug(cls, m, *args):
    if args:
      m = m % args
    if env.getbool('DEBUG'):
      print m
    assert(cls) # This makes the pychecker "unused variable" warning go away

  @classmethod
  def Fatal(cls, m, *args):
    # Note, using keyword args and arg lists while trying to keep
    # the m and *args parameters next to each other does not work
    cls.FatalWithResult(-1, m, *args)

  @classmethod
  def LogPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.LOG_OUT:
      print >> o, m

  @classmethod
  def ErrorPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.ERROR_OUT:
      print >> o, m

def EscapeEcho(s):
  """ Quick and dirty way of escaping characters that may otherwise be
      interpreted by bash / the echo command (rather than preserved). """
  return s.replace("\\", r"\\").replace("$", r"\$").replace('"', r"\"")

def StringifyCommand(cmd, stdin_contents=None):
  """ Return a string for reproducing the command "cmd", which will be
      fed stdin_contents through stdin. """
  stdin_str = ""
  if stdin_contents:
    stdin_str = "echo \"\"\"" + EscapeEcho(stdin_contents) + "\"\"\" | "
  if env.getbool('LOG_PRETTY_PRINT'):
    return stdin_str + PrettyStringify(cmd)
  else:
    return stdin_str + SimpleStringify(cmd)

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

# If the driver is waiting on a background process in RunWithLog()
# and the user Ctrl-C's or kill's the driver, it may leave
# the child process (such as llc) running. To prevent this,
# the code below sets up a signal handler which issues a kill to
# the currently running child processes.
CleanupProcesses = []
def SetupSignalHandlers():
  global CleanupProcesses
  def signal_handler(unused_signum, unused_frame):
    for p in CleanupProcesses:
      try:
        p.kill()
      except BaseException:
        pass
    os.kill(os.getpid(), signal.SIGKILL)
    return 0
  if os.name == "posix":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

def PipeRecord(event, f, q):
  while True:
    line = f.readline()
    if line:
      q.put(line)
      event.set()
    else:
      f.close()
      break
  return 0

def ProcessWait(event, p):
  try:
    p.wait()
  except BaseException:
    pass
  event.set()
  return 0

def QueueGetNext(q):
  try:
    nextline = q.get_nowait()
  except Empty:
    return None
  except KeyboardInterrupt as e:
    raise e
  else:
    return nextline

def RunWithLog(args, **kwargs):
  kwargs['log_command'] = True
  kwargs['log_stdout'] = True
  kwargs['log_stderr'] = True
  Run(args, **kwargs)

#
# RunDirect: Run a command.
# Returns: Exit code
# If return_stdout or return_stderr is true,
# returns: (exit code, stdout_contents, stderr_contents)
#
def Run(args,                    # Command and arguments
        stdin = None,            # Contents for child's stdin (string)
        echo_stdout = True,      # Echo the child's stdout to stdout
        echo_stderr = True,      # Echo the child's stderr to stderr
        log_command = False,     # Log the command being run
        log_stdout = False,      # Log the child's stdout
        log_stderr = False,      # Log the child's stderr
        errexit = True,          # Exit on failure (errcode != 0)
        return_stdout = False,   # Return the contents of stdout
        return_stderr = False,   # Return the contents of stderr
        redirect_stdout = None,  # Send stdout to a file object
        redirect_stderr = None): # Send stderr to a file object

  if isinstance(args, str):
    args = shell.split(env.eval(args))

  if log_command:
    Log.Info('-' * 60)
    Log.Info('\n' + StringifyCommand(args, stdin))

  if env.getbool('DRY_RUN'):
    if return_stdout or return_stderr:
      # TODO(pdox): Prevent this from happening, so that
      # dry-run is more useful.
      Log.Fatal("Unhandled dry-run case.")
    return 0

  record_stdout = log_stdout or return_stdout or redirect_stdout
  record_stderr = log_stderr or return_stderr or redirect_stderr

  stdin_pipe = None
  if stdin is not None:
    stdin_pipe = subprocess.PIPE

  stdout_pipe = None
  if record_stdout:
    stdout_pipe = subprocess.PIPE

  stderr_pipe = None
  if record_stderr:
    stderr_pipe = subprocess.PIPE

  try:
    p = subprocess.Popen(args, stdin=stdin_pipe,
                               stdout=stdout_pipe,
                               stderr=stderr_pipe )
  except Exception, e:
    msg =  'failed (%s) to run: %s' % (str(e), StringifyCommand(args, stdin))
    if log_command:
      Log.Fatal(msg)
    else:
      print msg
      DriverExit(1)

  CleanupProcesses.append(p)

  stdoutq = Queue()
  stderrq = Queue()
  IOReady = Event()
  threads = []

  t = Thread(target=ProcessWait, args=(IOReady,p))
  threads.append(t)

  if record_stdout:
    t = Thread(target=PipeRecord, args=(IOReady, p.stdout, stdoutq))
    threads.append(t)
  if record_stderr:
    t = Thread(target=PipeRecord, args=(IOReady, p.stderr, stderrq))
    threads.append(t)

  for t in threads:
    t.start()

  if stdin is not None:
    # This blocks while writing stdin.
    # TODO(pdox): Ideally, stdin would be fed in synchronously.
    p.stdin.write(stdin)
    p.stdin.close()

  stdout_contents = ''
  stderr_contents = ''
  # Loop while handling I/O on stdout/stderr until it finishes.
  lastio = False
  while True:
    IOReady.wait()
    if p.poll() is not None:
      # Wait for the threads to finish so that the pipes are flushed.
      for t in threads:
        t.join()
      # The threads are now closed, but there might still
      # be data on the queue.
      lastio = True

    # For fair queueing, record the size here.
    stdout_qsize = stdoutq.qsize()
    stderr_qsize = stderrq.qsize()

    # Flush stdout queue
    while stdout_qsize > 0:
      line = QueueGetNext(stdoutq)
      if line and echo_stdout:
        sys.stdout.write(line)
      if line and record_stdout:
        stdout_contents += line
      if line and redirect_stdout:
        redirect_stdout.write(line)
      stdout_qsize -= 1

    # Flush stderr queue
    while stderr_qsize > 0:
      line = QueueGetNext(stderrq)
      if line and echo_stderr:
        sys.stderr.write(line)
      if line and record_stderr:
        stderr_contents += line
      if line and redirect_stderr:
        redirect_stderr.write(line)
      stderr_qsize -= 1

    if lastio:
      break

  CleanupProcesses.pop()

  if errexit and p.returncode != 0:
    if log_command:
      Log.FatalWithResult(p.returncode,
                          'failed command: %s\n'
                          'stdout        : %s\n'
                          'stderr        : %s\n',
                          StringifyCommand(args, stdin),
                          stdout_contents, stderr_contents)
    else:
      DriverExit(p.returncode)
  else:
    if log_command:
      Log.Info('Return Code: ' + str(p.returncode))

  if return_stdout or return_stderr:
    return (p.returncode, stdout_contents, stderr_contents)
  return p.returncode

def FixArch(arch):
  arch = arch.lower()
  archfix = { 'x86-32': 'X8632',
              'x86_32': 'X8632',
              'x8632' : 'X8632',
              'i686'  : 'X8632',
              'ia32'  : 'X8632',
              '386'   : 'X8632',
              '686'   : 'X8632',

              'amd64' : 'X8664',
              'x86_64': 'X8664',
              'x86-64': 'X8664',
              'x8664' : 'X8664',

              'arm'   : 'ARM',
              'armv7' : 'ARM' }
  if arch not in archfix:
    Log.Fatal('Unrecognized arch "%s"!', arch)
  return archfix[arch]

def DriverMain(main):
  SetupSignalHandlers()
  env.reset()
  argv = sys.argv

  # Parse driver arguments
  (driver_flags, main_args) = ParseArgs(argv[1:],
                                        DriverPatterns,
                                        must_match = False)
  env.append('DRIVER_FLAGS', *driver_flags)

  # Start the Log
  Log.reset()
  Log.Banner(argv)

  # Pull the arch from the filename
  # Examples: pnacl-i686-as  (as incarnation, i686 arch)
  tokens = os.path.basename(argv[0]).split('-')
  if len(tokens) > 2:
    arch = FixArch(tokens[-2])
    env.set('ARCH', arch)

  ret = main(main_args)
  sys.exit(ret)

def GetArch(required = False):
  arch = env.getone('ARCH')
  if arch == '':
    arch = None

  if required and not arch:
    Log.Fatal('Missing -arch!')

  return arch

def DriverExit(code):
  sys.exit(code)

def CheckPresenceSelUniversal():
  'Assert that both sel_universal and sel_ldr exist'
  sel_universal = env.eval('${SEL_UNIVERSAL}')
  if not os.path.exists(sel_universal):
    Log.Fatal('Could not find sel_universal [%s]', sel_universal)
  sel_ldr = env.eval('${SEL_LDR}')
  if not os.path.exists(sel_ldr):
    Log.Fatal('Could not find sel_ldr [%s]', sel_ldr)

def DriverOpen(filename, mode, fail_ok = False):
  try:
    fp = open(filename, mode)
  except Exception:
    if not fail_ok:
      Log.Fatal("%s: Unable to open file", filename)
      DriverExit(1)
    else:
      return None
  return fp

def DriverClose(fp):
  fp.close()

class DriverChain(object):
  """ The DriverChain class takes one or more input files,
      an output file, and a sequence of steps. It executes
      those steps, using intermediate files in between,
      to generate the final outpu.
  """

  def __init__(self, input, output, namegen):
    self.input = input
    self.output = output
    self.steps = []
    self.namegen = namegen

    # "input" can be a list of files or a single file.
    # If we're compiling for a single file, then we use
    # TempNameForInput. If there are multiple files
    # (e.g. linking), then we use TempNameForOutput.
    self.use_names_for_input = isinstance(input, str)

  def add(self, callback, output_type, **extra):
    step = (callback, output_type, extra)
    self.steps.append(step)

  def run(self):
    step_input = self.input
    for (i, (callback, output_type, extra)) in enumerate(self.steps):
      if i == len(self.steps)-1:
        # Last step
        step_output = self.output
      else:
        # Intermediate step
        if self.use_names_for_input:
          step_output = self.namegen.TempNameForInput(self.input, output_type)
        else:
          step_output = self.namegen.TempNameForOutput(output_type)
      callback(step_input, step_output, **extra)
      step_input = step_output
