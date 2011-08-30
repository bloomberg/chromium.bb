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
import threading
import Queue
import artools
import ldtools
import pathtools

######################################################################
#
# Environment Settings
#
######################################################################

# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class env" defined later for the implementation.

INITIAL_ENV = {
  'BASE_NACL'       : '${@FindBaseNaCl}',   # Absolute path of native_client/
  'BASE'            : '${@FindBasePNaCl}',  # Absolute path to PNaCl
  'BASE_DRIVER'     : '${@FindBaseDriver}', # Location of PNaCl drivers
  'BUILD_OS'        : '${@GetBuildOS}',     # "linux", "darwin" or "windows"
  'BUILD_ARCH'      : '${@GetBuildArch}',   # "x86_64" or "i686" or "i386"
  'DRIVER_EXT'      : '${@GetDriverExt}',   # '.py' or ''

  # Directories
  'BASE_PKG'        : '${BASE}/pkg',
  'BASE_SDK'        : '${BASE}/sdk',
  'BASE_LLVM'       : '${BASE_PKG}/llvm',
  'BASE_LLVM_GCC'   : '${BASE_PKG}/llvm-gcc',
  'BASE_NEWLIB'     : '${BASE_PKG}/newlib',
  'BASE_GLIBC'      : '${BASE_PKG}/glibc',
  'BASE_BINUTILS'   : '${BASE_PKG}/binutils',
  'BASE_LIBSTDCPP'  : '${BASE_PKG}/libstdcpp',
  'BASE_SYSROOT'    : '${BASE}/sysroot',

  'BASE_INCLUDE'    : '${BASE_SYSROOT}/include',
  'BASE_LLVM_BIN'   : '${BASE_LLVM}/bin',
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
  'SAVE_TEMPS'  : '1',    # Do not clean up temporary files
                          # TODO(pdox): Disable for SDK version
  'SANDBOXED'   : '0',    # Use sandboxed toolchain for this arch. (main switch)
  'SRPC'        : '1',    # Use SRPC sandboxed toolchain
  'MC_DIRECT'           : '1',
  'USE_EMULATOR'        : '0',
  'DRIVER_FLAGS'        : '', # Flags passed to the driver
  'LIBMODE'             : '${@DetectLibMode}',  # glibc or newlib
  'LIBMODE_GLIBC'       : '${LIBMODE==glibc ? 1 : 0}',
  'LIBMODE_NEWLIB'      : '${LIBMODE==newlib ? 1 : 0}',

  # Logging settings
  'LOG_TO_FILE'          : '1',
  'LOG_FILENAME'         : '${BASE}/driver.log',
  'LOG_FILE_SIZE_LIMIT'  : str(20 * 1024 * 1024),
  'LOG_PRETTY_PRINT'     : '1',

   # Conventions
  'SO_EXT'          : '${SO_EXT_%BUILD_OS%}',
  'SO_EXT_darwin'   : '.dylib',
  'SO_EXT_linux'    : '.so',
  'SO_EXT_windows'  : '.dll',

  'SO_DIR'          : '${SO_DIR_%BUILD_OS%}',
  'SO_DIR_darwin'   : 'lib',
  'SO_DIR_linux'    : 'lib',
  'SO_DIR_windows'  : 'bin',  # On Windows, DLLs are placed in bin/
                              # because the dynamic loader searches %PATH%

  'SO_PREFIX'        : '${SO_PREFIX_%BUILD_OS%}',
  'SO_PREFIX_darwin' : 'lib',
  'SO_PREFIX_linux'  : 'lib',
  'SO_PREFIX_windows': 'cyg',

  'EXEC_EXT'        : '${EXEC_EXT_%BUILD_OS%}',
  'EXEC_EXT_darwin' : '',
  'EXEC_EXT_linux'  : '',
  'EXEC_EXT_windows': '.exe',

  'SCONS_OS'            : '${SCONS_OS_%BUILD_OS%}',
  'SCONS_OS_linux'      : 'linux',
  'SCONS_OS_darwin'     : 'mac',
  'SCONS_OS_windows'    : 'win',

  # Tool Pathnames
  'GOLD_PLUGIN_SO': '${BASE_LLVM}/${SO_DIR}/${SO_PREFIX}LLVMgold${SO_EXT}',

  'SCONS_STAGING'       : '${SCONS_STAGING_%ARCH%}',
  'SCONS_STAGING_X8632' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-32/staging',
  'SCONS_STAGING_X8664' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-64/staging',
  'SCONS_STAGING_ARM'   : '${SCONS_OUT}/opt-${SCONS_OS}-arm/staging',

  'SEL_UNIVERSAL_PREFIX': '${USE_EMULATOR ? ${EMULATOR}}',
  'SEL_UNIVERSAL'       : '${SCONS_STAGING}/sel_universal${EXEC_EXT}',
  'SEL_UNIVERSAL_FLAGS' : '--abort_on_error -B ${IRT_BLOB} ' +
                          '${USE_EMULATOR ? -Q --command_prefix ${EMULATOR}}',

  'IRT_STAGING'         : '${IRT_STAGING_%ARCH%}',
  'IRT_STAGING_X8632'   : '${SCONS_OUT}/nacl_irt-x86-32/staging',
  'IRT_STAGING_X8664'   : '${SCONS_OUT}/nacl_irt-x86-64/staging',
  'IRT_STAGING_ARM'     : '${SCONS_OUT}/nacl_irt-arm/staging',
  # The irt_core.nexe should suffice for the sandboxed translators, since
  # they do not use PPAPI.
  'IRT_BLOB'            : '${IRT_STAGING}/irt_core.nexe',

  'EMULATOR'            : '${EMULATOR_%ARCH%}',
  'EMULATOR_X8632'      : '',
  'EMULATOR_X8664'      : '',
  # NOTE: this is currently the only dependency on the arm trusted TC
  'EMULATOR_ARM'        :
      '${BASE_NACL}/toolchain/linux_arm-trusted/run_under_qemu_arm',

  'SEL_LDR'       : '${SCONS_STAGING}/sel_ldr${EXEC_EXT}',

  # c.f. http://code.google.com/p/nativeclient/issues/detail?id=1968
  # LLVM_AS is used to compile a .ll file to a bitcode file (".po")
  'LLVM_AS'       : '${BASE_LLVM_BIN}/llvm-as${EXEC_EXT}',
  # LLVM_MC is llvm's replacement for bintutil's as
  'LLVM_MC'       : '${BASE_LLVM_BIN}/llvm-mc${EXEC_EXT}',

  # c.f. http://code.google.com/p/nativeclient/issues/detail?id=1968
  'AS_ARM'        : '${BINUTILS_BASE}as',
  'AS_X8632'      : '${LLVM_MC}',
  'AS_X8664'      : '${LLVM_MC}',

  'LD_SB'         : '${SEL_LDR} -a -B ${IRT_BLOB} -- ${BASE_SB}/nonsrpc/bin/ld',
  'LLC_SB'        : '${SEL_LDR} -a -B ${IRT_BLOB} -- ${RUNNABLE_LD} ' +
                    '${BASE_SB}/nonsrpc/bin/llc',
  'SB_DYNAMIC'    : '0',
  'NNACL_LIBDIR'  : '${BASE_NACL}/toolchain/${SCONS_OS}_x86/' +
                    'x86_64-nacl/${ARCH  == X8632 ? lib32 : lib}',
  'RUNNABLE_LD'   : '${SB_DYNAMIC ? ${NNACL_LIBDIR}/runnable-ld.so ' +
                    '--library-path ${NNACL_LIBDIR}}',


  'LLC_SRPC'      : '${BASE_SB}/srpc/bin/llc',
  'LD_SRPC'       : '${BASE_SB}/srpc/bin/ld',

  'LLVM_GCC_PREFIX': '${BASE_LLVM_GCC}/bin/arm-none-linux-gnueabi-',
  'LLVM_GCC'      : '${LLVM_GCC_PREFIX}gcc${EXEC_EXT}',
  'LLVM_GXX'      : '${LLVM_GCC_PREFIX}g++${EXEC_EXT}',

  'LLVM_OPT'      : '${BASE_LLVM_BIN}/opt${EXEC_EXT}',
  'LLVM_LLC'      : '${BASE_LLVM_BIN}/llc${EXEC_EXT}',
  'LLVM_LD'       : '${BASE_LLVM_BIN}/llvm-ld${EXEC_EXT}',
  'LLVM_DIS'      : '${BASE_LLVM_BIN}/llvm-dis${EXEC_EXT}',
  'LLVM_LINK'     : '${BASE_LLVM_BIN}/llvm-link${EXEC_EXT}',

  'BINUTILS_BASE'  : '${BASE_BINUTILS}/bin/arm-pc-nacl-',
  'OBJDUMP'        : '${BINUTILS_BASE}objdump${EXEC_EXT}',
  'NM'             : '${BINUTILS_BASE}nm${EXEC_EXT}',
  'AR'             : '${BINUTILS_BASE}ar${EXEC_EXT}',
  'RANLIB'         : '${BINUTILS_BASE}ranlib${EXEC_EXT}',
  'STRIP'          : '${BINUTILS_BASE}strip${EXEC_EXT}',

  'LD_BFD'         : '${BINUTILS_BASE}ld.bfd${EXEC_EXT}',
  'LD_GOLD'        : '${BINUTILS_BASE}ld.gold${EXEC_EXT}',
}




DriverPatterns = [
  ( '--pnacl-driver-verbose',             "Log.LOG_OUT.append(sys.stderr)"),
  ( '--pnacl-driver-debug',               "env.set('DEBUG', '1')"),
  ( '--pnacl-driver-recurse',             "env.set('RECURSE', '1')"),
  ( '--pnacl-driver-set-([^=]+)=(.*)',    "env.set($0, $1)"),
  ( '--pnacl-driver-append-([^=]+)=(.*)', "env.append($0, $1)"),
  ( ('-arch', '(.+)'),                 "SetArch($0)"),
  ( '--pnacl-sb',                      "env.set('SANDBOXED', '1')"),
  ( '--pnacl-sb-dynamic',              "env.set('SB_DYNAMIC', '1')"),
  ( '--pnacl-use-emulator',            "env.set('USE_EMULATOR', '1')"),
  ( '--dry-run',                       "env.set('DRY_RUN', '1')"),
  ( '--pnacl-arm-bias',                "env.set('BIAS', 'ARM')"),
  ( '--pnacl-i686-bias',               "env.set('BIAS', 'X8632')"),
  ( '--pnacl-x86_64-bias',             "env.set('BIAS', 'X8664')"),
  ( '--pnacl-bias=(.+)',               "env.set('BIAS', FixArch($0))"),
  ( '-save-temps',                     "env.set('SAVE_TEMPS', '1')"),
  ( '-no-save-temps',                  "env.set('SAVE_TEMPS', '0')"),
 ]


######################################################################
#
# Environment
#
######################################################################

# TODO(pdox): Move the environment & parser into a separate .py file
class env(object):
  data = {}
  stack = []
  functions = {}

  @classmethod
  def register(cls, func):
    """ Register a function for use in the evaluator """
    cls.functions[func.__name__] = func
    return func

  @classmethod
  def update(cls, extra):
    INITIAL_ENV.update(extra)
    assert(cls) # This makes the pychecker "unused variable" warning go away

  @classmethod
  def reset(cls):
    cls.data = dict(INITIAL_ENV)

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
    return shell.unescape(cls.getraw(varname))

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
  # bracket_expr = varname | boolexpr ? expr | boolexpr ? expr : expr | @call
  # boolexpr = boolval | boolval '&&' boolexpr | boolval '||' boolexpr
  # boolval = varname | !varname | #varname | !#varname | varname '==' str
  # varname = str | varname '%' bracket_expr '%' varname
  # call = func | func ':' arglist
  # func = str
  # arglist = empty | arg ':' arglist
  #
  # Do not call these functions outside of class env.
  # The env.eval method is the external interface to the evaluator.
  ######################################################################

  # Evaluate a string literal
  @classmethod
  def eval_str(cls, s, pos, terminators):
    (_,i) = FindFirst(s, pos, terminators)
    return (s[pos:i], i)

  # Evaluate %var% substitutions inside a variable name.
  # Returns (the_actual_variable_name, endpos)
  # Terminated by } character
  @classmethod
  def eval_varname(cls, s, pos, terminators):
    (_,i) = FindFirst(s, pos, ['%'] + terminators)
    leftpart = s[pos:i].strip(' ')
    if i == len(s) or s[i] in terminators:
      return (leftpart, i)

    (middlepart, j) = cls.eval_bracket_expr(s, i+1, ['%'])
    if j == len(s) or s[j] != '%':
      ParseError(s, i, j, "Unterminated %")

    (rightpart, k) = cls.eval_varname(s, j+1, terminators)

    fullname = leftpart + middlepart + rightpart
    fullname = fullname.strip()
    return (fullname, k)

  # Absorb whitespace
  @classmethod
  def eval_whitespace(cls, s, pos):
    i = pos
    while i < len(s) and s[i] == ' ':
      i += 1
    return (None, i)

  @classmethod
  def eval_bool_val(cls, s, pos, terminators):
    (_,i) = cls.eval_whitespace(s, pos)

    if s[i] == '!':
      negated = True
      i += 1
    else:
      negated = False

    (_,i) = cls.eval_whitespace(s, i)

    if s[i] == '#':
      uselen = True
      i += 1
    else:
      uselen = False

    (varname, j) = cls.eval_varname(s, i, ['=']+terminators)
    if j == len(s):
      # This is an error condition one level up. Don't evaluate anything.
      return (False, j)

    if varname not in cls.data:
      ParseError(s, i, j, "Undefined variable '%s'" % varname)
    vardata = cls.data[varname]

    contents = cls.eval(vardata)

    if s[j] == '=':
      # String equality test
      if j+1 == len(s) or s[j+1] != '=':
        ParseError(s, j, j, "Unexpected token")
      if uselen:
        ParseError(s, j, j, "Cannot combine == and #")
      (_,j) = cls.eval_whitespace(s, j+2)
      (literal_str,j) = cls.eval_str(s, j, [' ']+terminators)
      (_,j) = cls.eval_whitespace(s, j)
      if j == len(s):
        return (False, j) # Error one level up
    else:
      literal_str = None

    if uselen:
      val = (len(contents) != 0)
    elif literal_str is not None:
      val = (contents == literal_str)
    else:
      if contents not in ('0','1'):
        ParseError(s, j, j,
          "%s evaluated to %s, which is not a boolean!" % (varname, contents))
      val = bool(int(contents))
    return (negated ^ val, j)

  # Evaluate a boolexpr
  @classmethod
  def eval_bool_expr(cls, s, pos, terminators):
    (boolval1, i) = cls.eval_bool_val(s, pos, ['&','|']+terminators)
    if i == len(s):
      # This is an error condition one level up. Don't evaluate anything.
      return (False, i)

    if s[i] in ('&','|'):
      # and/or expression
      if i+1 == len(s) or s[i+1] != s[i]:
        ParseError(s, i, i, "Unexpected token")
      is_and = (s[i] == '&')

      (boolval2, j) = cls.eval_bool_expr(s, i+2, terminators)
      if j == len(s):
        # This is an error condition one level up.
        return (False, j)

      if is_and:
        return (boolval1 and boolval2, j)
      else:
        return (boolval1 or boolval2, j)

    return (boolval1, i)

  # Evaluate the inside of a ${} or %%.
  # Returns the (the_evaluated_string, endpos)
  @classmethod
  def eval_bracket_expr(cls, s, pos, terminators):
    (_,pos) = cls.eval_whitespace(s, pos)

    if s[pos] == '@':
      # Function call: ${@func}
      # or possibly  : ${@func:arg1:arg2...}
      (_,i) = FindFirst(s, pos, [':']+terminators)
      if i == len(s):
        return ('', i) # Error one level up
      funcname = s[pos+1:i]

      if s[i] != ':':
        j = i
        args = []
      else:
        (_,j) = FindFirst(s, i+1, terminators)
        if j == len(s):
          return ('', j) # Error one level up
        args = s[i+1:j].split(':')

      val = cls.functions[funcname](*args)
      contents = cls.eval(val)
      return (contents, j)

    (m,_) = FindFirst(s, pos, ['?']+terminators)
    if m != '?':
      # Regular variable substitution
      (varname,i) = cls.eval_varname(s, pos, terminators)
      if len(s) == i:
        return ('', i)  # Error one level up
      if varname not in cls.data:
        ParseError(s, pos, i, "Undefined variable '%s'" % varname)
      vardata = cls.data[varname]
      contents = cls.eval(vardata)
      return (contents, i)
    else:
      # Ternary Mode
      (is_cond_true,i) = cls.eval_bool_expr(s, pos, ['?']+terminators)
      assert(i < len(s) and s[i] == '?')

      (if_true_expr, j) = cls.eval_expr(s, i+1, [':']+terminators)
      if j == len(s):
        return ('', j) # Error one level up

      if s[j] == ':':
        (if_false_expr,j) = cls.eval_expr(s, j+1, terminators)
        if j == len(s):
          # This is an error condition one level up.
          return ('', j)
      else:
        if_false_expr = ''

      if is_cond_true:
        contents = if_true_expr.strip()
      else:
        contents = if_false_expr.strip()

      return (contents, j)

  # Evaluate an expression with ${} in string s, starting at pos.
  # Returns (the_evaluated_expression, endpos)
  @classmethod
  def eval_expr(cls, s, pos, terminators):
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
  script = shell.unescape(script)

  driver_args = env.get('DRIVER_FLAGS')

  if '--pnacl-driver-recurse' not in driver_args:
    driver_args.append('--pnacl-driver-recurse')

  # Get rid of -arch <arch> in the driver flags.
  if suppress_arch:
    while '-arch' in driver_args:
      i = driver_args.index('-arch')
      driver_args = driver_args[:i] + driver_args[i+2:]

  python_interpreter = pathtools.normalize(sys.executable)
  script = pathtools.tosys(script)
  cmd = [python_interpreter, script] + driver_args + args

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

def memoize(f):
  """ Memoize a function with no arguments """
  saved = {}
  def newf():
    if len(saved) == 0:
      saved[None] = f()
    return saved[None]
  newf.__name__ = f.__name__
  return newf


@env.register
@memoize
def GetBuildOS():
  name = platform.system().lower()
  if name.startswith('cygwin_nt') or 'windows' in name:
    name = 'windows'
  if name not in ('linux', 'darwin', 'windows'):
    Log.Fatal("Unsupported platform '%s'", name)
  return name

@env.register
@memoize
def GetBuildArch():
  m = platform.machine()

  # Windows is special
  if m == 'x86':
    m = 'i686'

  if m not in ('i386', 'i686', 'x86_64'):
    Log.Fatal("Unsupported architecture '%s'", m)
  return m

# Crawl backwards, starting from the directory containing this script,
# until we find a directory satisfying a filter function.
def FindBaseDir(function):
  Depth = 0
  cur = DriverPath()
  while not function(cur) and Depth < 16:
    cur = pathtools.dirname(cur)
    Depth += 1
  if function(cur):
    return cur
  return None

def DriverPath():
  return pathtools.abspath(pathtools.normalize(sys.argv[0]))

@env.register
@memoize
def FindBaseNaCl():
  """ Find native_client/ directory """
  dir = FindBaseDir(lambda cur: pathtools.basename(cur) == 'native_client')
  if dir is None:
    Log.Fatal("Unable to find 'native_client' directory")
  return shell.escape(dir)

@env.register
@memoize
def FindBasePNaCl():
  """ Find the base directory of the PNaCl toolchain """

  # Normally, the driver is in pnacl_*_*_*/bin/.
  # But we can also be invoked from tools/llvm/driver.
  # For now, default to using newlib when invoked from tools/llvm/driver.
  bindir = pathtools.dirname(DriverPath())
  if pathtools.basename(bindir) == 'bin':
    dir = pathtools.dirname(bindir)
    return shell.escape(dir)
  else:
    # If we've invoked from tools/llvm/driver
    return '${BASE_NACL}/toolchain/pnacl_${BUILD_OS}_${BUILD_ARCH}_${LIBMODE}'


@env.register
@memoize
def FindBaseDriver():
  """Find the directory with the driver"""
  return shell.escape(pathtools.dirname(DriverPath()))

@env.register
@memoize
def GetDriverExt():
  return pathtools.splitext(DriverPath())[1]

@env.register
@memoize
def DetectLibMode():
  """ Determine if this is glibc or newlib """
  dir = pathtools.dirname(DriverPath())

  is_newlib = pathtools.exists(pathtools.join(dir, 'newlib.cfg'))
  is_glibc = pathtools.exists(pathtools.join(dir, 'glibc.cfg'))

  assert(not (is_newlib and is_glibc))

  if is_newlib:
    return "newlib"

  if is_glibc:
    return "glibc"

  Log.Fatal("Cannot determine library mode!")

@env.register
def AddPrefix(prefix, varname):
  values = env.get(varname)
  return ' '.join([prefix + shell.escape(v) for v in values ])

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
def IsNative(filename):
  return FileType(filename) in ['o','so']

@SimpleCache
def IsBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(2)
  DriverClose(fp)
  if header == 'BC':
    return True
  return False

@SimpleCache
def IsArchive(filename):
  return artools.IsArchive(filename)

@SimpleCache
def IsBitcodeArchive(filename):
  filetype = FileType(filename)
  return filetype == 'archive-bc'

@SimpleCache
def IsNativeArchive(filename):
  return IsArchive(filename) and not IsBitcodeArchive(filename)

class ELFHeader(object):
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

  def __init__(self, e_type, e_machine, e_osabi, e_abiver):
    self.type = self.ELF_TYPES[e_type]
    self.machine = self.ELF_MACHINES[e_machine]
    self.osabi = self.ELF_OSABI[e_osabi]
    self.abiver = self.ELF_ABI_VER[e_abiver]
    self.arch = FixArch(self.machine)  # For convenience

# If the file is not ELF, returns None.
# Otherwise, returns an ELFHeader object.
@SimpleCache
def GetELFHeader(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(16 + 2 + 2)
  DriverClose(fp)
  return DecodeELFHeader(header, filename)

def DecodeELFHeader(header, filename):
  # Pull e_ident, e_type, e_machine
  if header[0:4] != ELFHeader.ELF_MAGIC:
    return None

  e_osabi = DecodeLE(header[7])
  e_abiver = DecodeLE(header[8])
  e_type = DecodeLE(header[16:18])
  e_machine = DecodeLE(header[18:20])

  if e_osabi not in ELFHeader.ELF_OSABI:
    Log.Fatal('%s: ELF file has unknown OS ABI (%d)', filename, e_osabi)
  if e_abiver not in ELFHeader.ELF_ABI_VER:
    Log.Fatal('%s: ELF file has unknown ABI version (%d)', filename, e_abiver)
  if e_type not in ELFHeader.ELF_TYPES:
    Log.Fatal('%s: ELF file has unknown type (%d)', filename, e_type)
  if e_machine not in ELFHeader.ELF_MACHINES:
    Log.Fatal('%s: ELF file has unknown machine type (%d)', filename, e_machine)

  eh = ELFHeader(e_type, e_machine, e_osabi, e_abiver)
  return eh

def IsELF(filename):
  return GetELFHeader(filename) is not None

# Decode Little Endian bytes into an unsigned value
def DecodeLE(bytes):
  value = 0
  for b in reversed(bytes):
    value *= 2
    value += ord(b)
  return value

@SimpleCache
def GetBitcodeMetadata(filename):
  assert(IsBitcode(filename))

  # TODO(pdox): Make this work with sandboxed translator
  llc = env.getone('LLVM_LLC')
  args = [ llc, '-only-dump-metadata', '-dump-metadata=-', filename ]
  _, stdout_contents, _ = Run(args, echo_stdout = False, return_stdout = True)

  metadata = { 'OutputFormat': '',
               'SOName'      : '',
               'NeedsLibrary': [] }
  for line in stdout_contents.split('\n'):
    if not line.strip():
      continue
    k, v = line.split(':')
    k = k.strip()
    v = v.strip()
    assert(k in metadata)
    if isinstance(metadata[k], list):
      metadata[k].append(v)
    else:
      metadata[k] = v

  return metadata

# If FORCED_FILE_TYPE is set, FileType() will return FORCED_FILE_TYPE for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files to be treated
# in a certain way.
FORCED_FILE_TYPE = None
def SetForcedFileType(t):
  global FORCED_FILE_TYPE
  FORCED_FILE_TYPE = t

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if FORCED_FILE_TYPE is None:
      return
    newtype = FORCED_FILE_TYPE
  FileType.__cache[filename] = newtype

# File Extension -> Type string
ExtensionMap = {
  'c'   : 'src',
  'cc'  : 'src',
  'cpp' : 'src',
  'C'   : 'src',
  'll'  : 'll',
  'bc'  : 'po',
  'po'  : 'po',   # .po = "Portable object file"
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

  # TODO(pdox): We open and read the the first few bytes of each file
  #             up to 4 times, when we only need to do it once. The
  #             OS cache prevents us from hitting the disk, but this
  #             is still slower than it needs to be.
  if IsArchive(filename):
    return artools.GetArchiveType(filename)

  if IsELF(filename):
    return GetELFType(filename)

  if IsBitcode(filename):
    return GetBitcodeType(filename)

  if ext in ('o','so','a','po','pso','pa') and ldtools.IsLinkerScript(filename):
    return 'ldscript'

  # Use the file extension if it is recognized
  if ext in ExtensionMap:
    return ExtensionMap[ext]

  Log.Fatal('%s: Unrecognized file type', filename)


@SimpleCache
def GetELFType(filename):
  """ ELF type as determined by ELF metadata """
  assert(IsELF(filename))
  elfheader = GetELFHeader(filename)
  elf_type_map = {
    'EXEC': 'nexe',
    'REL' : 'o',
    'DYN' : 'so'
  }
  return elf_type_map[elfheader.type]

@SimpleCache
def GetBitcodeType(filename):
  """ Bitcode type as determined by bitcode metadata """
  assert(IsBitcode(filename))
  metadata = GetBitcodeMetadata(filename)
  format_map = {
    'object': 'po',
    'shared': 'pso',
    'executable': 'pexe'
  }
  return format_map[metadata['OutputFormat']]

######################################################################
#
# File Naming System (Temp files & Output files)
#
######################################################################

def DefaultOutputName(filename, outtype):
  base = pathtools.basename(filename)
  base = RemoveExtension(base)

  if outtype in ('pp','dis'): return '-'; # stdout
  if outtype in ('po'): return base + '.o'

  assert(outtype in ExtensionMap.values())
  assert(outtype != 'src')

  return base + '.' + outtype

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = pathtools.splitext(filename)
  if ext == '':
    Log.Fatal('File has no extension: ' + filename)
  return name

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = pathtools.split(cur)
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
    inputs = [ pathtools.abspath(i) for i in inputs ]
    output = pathtools.abspath(output)

    self.TempBase = output + '---linked'

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
    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

  def TempNameForInput(self, input, imtype):
    fullpath = pathtools.abspath(input)
    # If input is already a temporary name, just change the extension
    if fullpath.startswith(self.TempBase):
      temp = self.TempBase + '.' + imtype
    else:
      # Source file
      temp = self.TempMap[fullpath] + '.' + imtype

    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

######################################################################
#
# Shell Utilities
#
######################################################################

class shell(object):

  @staticmethod
  def unescape(s):
    w = shell.split(s)
    if len(w) == 0:
      return ''
    if len(w) == 1:
      return w[0]
    # String was not properly escaped in the first place?
    assert(False)

  # TODO(pdox): Simplify this function by moving more of it into unescape
  @staticmethod
  def split(s):
    """Split a shell-style string up into a list of distinct arguments.
    For example: split('cmd -arg1 -arg2="a b c"')
    Returns ['cmd', '-arg1', '-arg2=a b c']
    """
    assert(isinstance(s, str))
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
    return ' '.join([ shell.escape(a) for a in args ])

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
    file_too_big = pathtools.isfile(filename) and \
                   pathtools.getsize(filename) > sizelimit
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
  def Warning(cls, m, *args):
    m = 'Warning: ' + m
    cls.ErrorPrint(m, *args)

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

def PipeRecord(sem, f, q):
  """ Read the output of a subprocess from the file object f one line at a
      time. Put each line on Queue q and release semaphore sem to wake the
      parent thread.
  """
  while True:
    line = f.readline()
    if line:
      q.put(line)
      sem.release()
    else:
      f.close()
      break
  return 0

def ProcessWait(sem, p):
  """ Wait for the subprocess.Popen object p to finish, and release
      the semaphore sem to wake the parent thread.
  """
  try:
    p.wait()
  except BaseException:
    pass
  sem.release()
  return 0

def QueueGetNext(q):
  """ Return the next line from Queue q, or None if empty.
  """
  try:
    nextline = q.get_nowait()
  except Queue.Empty:
    return None
  except KeyboardInterrupt as e:
    raise e
  else:
    return nextline

def RunWithLog(args, **kwargs):
  kwargs.setdefault('log_command', True)
  kwargs.setdefault('log_stdout', True)
  kwargs.setdefault('log_stderr', True)
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

  args = [pathtools.tosys(args[0])] + args[1:]

  if log_command:
    Log.Info('-' * 60)
    Log.Info('\n' + StringifyCommand(args, stdin))

  if env.getbool('DRY_RUN'):
    if return_stdout or return_stderr:
      # TODO(pdox): Prevent this from happening, so that
      # dry-run is more useful.
      Log.Fatal("Unhandled dry-run case.")
    return 0

  # If we only want to echo or redirect the output, we directly pass
  # a descriptor to the child (process_stdout = False), which is is much
  # faster than doing all the processing here. For any other combination
  # (e.g. to log, return, or tee), we process the output by firing off
  # a separate thread below
  record_stdout = log_stdout or return_stdout
  record_stderr = log_stderr or return_stderr
  process_stdout = record_stdout or (redirect_stdout and echo_stdout)
  process_stderr = record_stderr or (redirect_stderr and echo_stderr)

  stdin_pipe = None
  if stdin is not None:
    stdin_pipe = subprocess.PIPE

  stdout_pipe = None  # By default, inherit the parent's stdout
  if process_stdout:
    stdout_pipe = subprocess.PIPE
  elif redirect_stdout:
    stdout_pipe = redirect_stdout
  elif not echo_stdout:
    stdout_pipe = open(os.devnull)

  stderr_pipe = None # By default, inherit the parent's stderr
  if process_stderr:
    stderr_pipe = subprocess.PIPE
  elif redirect_stderr:
    stderr_pipe = redirect_stderr
  elif not echo_stderr:
    stderr_pipe = open(os.devnull)

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

  stdoutq = Queue.Queue()
  stderrq = Queue.Queue()
  IOReady = threading.Semaphore()
  threads = []

  t = threading.Thread(target=ProcessWait, args=(IOReady,p))
  threads.append(t)

  if process_stdout:
    t = threading.Thread(target=PipeRecord, args=(IOReady, p.stdout, stdoutq))
    threads.append(t)
  if process_stderr:
    t = threading.Thread(target=PipeRecord, args=(IOReady, p.stderr, stderrq))
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
  # Loop while handling I/O on stdout/stderr until the child finishes.
  # If process_stderr/stdout are both false, then we just wait for the
  # ProcessWait thread
  lastio = False
  while True:
    IOReady.acquire()
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
      if line:
        if echo_stdout:
          sys.stdout.write(line)
        if record_stdout:
          stdout_contents += line
        if redirect_stdout:
          redirect_stdout.write(line)
      stdout_qsize -= 1

    # Flush stderr queue
    while stderr_qsize > 0:
      line = QueueGetNext(stderrq)
      if line:
        if echo_stderr:
          sys.stderr.write(line)
        if record_stderr:
          stderr_contents += line
        if redirect_stderr:
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
              'armv7' : 'ARM',
              'arm-thumb2' : 'ARM' }
  if arch not in archfix:
    Log.Fatal('Unrecognized arch "%s"!', arch)
  return archfix[arch]

def IsWindowsPython():
  return 'windows' in platform.system().lower()

def SetupCygwinLibs():
  bindir = os.path.dirname(os.path.abspath(sys.argv[0]))
  os.environ['PATH'] += os.pathsep + bindir

def DriverMain(main):
  SetupSignalHandlers()
  env.reset()

  if IsWindowsPython():
    SetupCygwinLibs()

  # Parse driver arguments
  (driver_flags, main_args) = ParseArgs(sys.argv[1:],
                                        DriverPatterns,
                                        must_match = False)
  env.append('DRIVER_FLAGS', *driver_flags)

  # Start the Log
  Log.reset()
  Log.Banner(sys.argv)

  # Pull the arch from the filename
  # Examples: pnacl-i686-as  (as incarnation, i686 arch)
  tokens = pathtools.basename(DriverPath()).split('-')
  if len(tokens) > 2:
    arch = FixArch(tokens[-2])
    SetArch(arch)

  ret = main(main_args)
  DriverExit(ret)

def SetArch(arch):
  env.set('ARCH', FixArch(arch))

def GetArch(required = False):
  arch = env.getone('ARCH')
  if arch == '':
    arch = None

  if required and not arch:
    Log.Fatal('Missing -arch!')

  return arch

# Read an ELF file to determine the machine type. If ARCH is already set,
# make sure the file has the same architecture. If ARCH is not set,
# set the ARCH to the file's architecture.
#
# Returns True if the file matches ARCH.
#
# Returns False if the file doesn't match ARCH. This only happens when
# must_match is False. If must_match is True, then a fatal error is generated
# instead.
def ArchMerge(filename, must_match):
  filetype = FileType(filename)
  if filetype in ('o','so'):
    elfheader = GetELFHeader(filename)
    if not elfheader:
      Log.Fatal("%s: Cannot read ELF header", filename)
    new_arch = elfheader.arch
  elif IsNativeArchive(filename):
    new_arch = filetype[len('archive-'):]
  else:
    Log.Fatal('%s: Unexpected file type in ArchMerge', filename)

  existing_arch = GetArch()

  if not existing_arch:
    SetArch(new_arch)
    return True
  elif new_arch != existing_arch:
    if must_match:
      msg = "%s: Incompatible object file (%s != %s)"
      logfunc = Log.Fatal
    else:
      msg = "%s: Skipping incompatible object file (%s != %s)"
      logfunc = Log.Warning
    logfunc(msg, filename, new_arch, existing_arch)
    return False
  else: # existing_arch and new_arch == existing_arch
    return True


class TempFileHandler(object):
  def __init__(self):
    self.files = []

  def add(self, path):
    path = pathtools.abspath(path)
    self.files.append(path)

  def wipe(self):
    for path in self.files:
      try:
        os.remove(path)
      except OSError:
        # If we're exiting early, the temp file
        # may have never been created.
        pass
    self.files = []

TempFiles = TempFileHandler()

def DriverExit(code):
  TempFiles.wipe()
  sys.exit(code)

def CheckTranslatorPrerequisites():
  """ Assert that the scons artifacts for running the sandboxed translator
      exist: sel_universal, sel_ldr and the irt blob. """
  sel_universal = env.getone('SEL_UNIVERSAL')
  if not pathtools.exists(sel_universal):
    Log.Fatal('Could not find sel_universal [%s]', sel_universal)
  sel_ldr = env.getone('SEL_LDR')
  if not pathtools.exists(sel_ldr):
    Log.Fatal('Could not find sel_ldr [%s]', sel_ldr)
  irt_blob = env.getone('IRT_BLOB')
  if not pathtools.exists(irt_blob):
    Log.Fatal('Could not find irt_blob [%s]', irt_blob)


def DriverOpen(filename, mode, fail_ok = False):
  try:
    fp = open(pathtools.tosys(filename), mode)
  except Exception:
    if not fail_ok:
      Log.Fatal("%s: Unable to open file", pathtools.touser(filename))
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
