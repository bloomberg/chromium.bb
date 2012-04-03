#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.

# Global environment and expression parsing for the PNaCl driver


# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class Environment" defined later for the implementation.

from shelltools import shell
from driver_log import Log, DriverExit

INITIAL_ENV = {
  # Set by DriverMain
  'DRIVER_PATH'     : '', # Absolute path to this driver invocation
  'DRIVER_BIN'      : '', # PNaCl driver bin/ directory

  'BASE_NACL'       : '${@FindBaseNaCl}',      # Absolute path of native_client/
  'BASE_TOOLCHAIN'  : '${@FindBaseToolchain}', # Absolute path to toolchain/
  'BASE'            : '${@FindBasePNaCl}',     # Absolute path to PNaCl
  'BUILD_OS'        : '${@GetBuildOS}',        # "linux", "darwin" or "windows"
  'BUILD_ARCH'      : '${@GetBuildArch}',      # "x86_64" or "i686" or "i386"

  # Directories
  'BASE_PKG'        : '${BASE}/pkg',
  'BASE_LLVM'       : '${BASE_PKG}/llvm',
  'BASE_BINUTILS'   : '${BASE_PKG}/binutils',
  'BASE_GCC'        : '${BASE_PKG}/gcc',

  'BASE_LIB_NATIVE' : '${BASE}/lib-',

  'BASE_NEWLIB'     : '${BASE}/newlib',
  'BASE_GLIBC'      : '${BASE}/glibc',
  'BASE_LIBMODE'    : '${BASE}/${LIBMODE}',

  'BASE_USR'        : '${BASE_LIBMODE}/usr',
  'BASE_SDK'        : '${BASE_LIBMODE}/sdk',
  'BASE_LIB'        : '${BASE_LIBMODE}/lib',


  'BASE_LLVM_BIN'   : '${BASE_LLVM}/bin',
  'TRANSLATOR_BIN'  :
    '${BASE_TOOLCHAIN}/pnacl_translator/${STANDARD_ARCH}/bin',
  'TRANSLATOR_BIN_NONSRPC':
    '${BASE_TOOLCHAIN}/pnacl_translator_nonsrpc/${STANDARD_ARCH}/bin',

  # TODO(pdox): Unify this with ARCH.
  'STANDARD_ARCH'      : '${STANDARD_ARCH_%ARCH%}',
  'STANDARD_ARCH_X8632': 'i686',
  'STANDARD_ARCH_X8664': 'x86_64',
  'STANDARD_ARCH_ARM'  : 'armv7',

  'SCONS_OUT'       : '${BASE_NACL}/scons-out',

  # Driver settings
  'ARCH'        : '',     # Target architecture
  'BIAS'        : 'NONE', # This can be 'NONE', 'ARM', 'X8632', or 'X8664'.
                          # When not set to none, this causes the front-end to
                          # act like a target-specific compiler. This bias is
                          # currently needed while compiling newlib,
                          # and some scons tests.
  'DRY_RUN'     : '0',
  'DEBUG'       : '0',    # Print out internal actions
  'RECURSE'     : '0',    # In a recursive driver call
  'SAVE_TEMPS'  : '0',    # Do not clean up temporary files
  'SANDBOXED'   : '0',    # Use sandboxed toolchain for this arch. (main switch)
  'SRPC'        : '1',    # Use SRPC sandboxed toolchain
  'FORCE_INTERMEDIATE_S': '0',
  'USE_EMULATOR'        : '0',
  'USE_BOOTSTRAP'       : '${BUILD_OS==linux ? 1 : 0}',
  'DRIVER_FLAGS'        : '', # Flags passed to the driver
  'LIBMODE'             : '', # glibc or newlib (set by ReadConfig)
  'LIBMODE_GLIBC'       : '${LIBMODE==glibc ? 1 : 0}',
  'LIBMODE_NEWLIB'      : '${LIBMODE==newlib ? 1 : 0}',

  # Logging settings
  'LOGGING'            : '0', # True if logging is enabled.
  'LOG_VERBOSE'        : '0', # Log to stdout (--pnacl-driver-verbose)
  'LOG_TO_FILE'        : '0', # Log to file (--pnacl-driver-log-to-file)
  'LOG_FILENAME'       : '${BASE}/driver.log',
  'LOG_FILE_SIZE_LIMIT': str(20 * 1024 * 1024),

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
  'DRAGONEGG_PLUGIN': '${BASE_GCC}/lib/dragonegg${SO_EXT}',

  'SCONS_STAGING'       : '${SCONS_STAGING_%ARCH%}',
  'SCONS_STAGING_X8632' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-32/staging',
  'SCONS_STAGING_X8664' : '${SCONS_OUT}/opt-${SCONS_OS}-x86-64/staging',
  'SCONS_STAGING_ARM'   : '${SCONS_OUT}/opt-${SCONS_OS}-arm/staging',

  'SEL_UNIVERSAL_PREFIX': '${USE_EMULATOR ? ${EMULATOR}}',
  'SEL_UNIVERSAL'       : '${SCONS_STAGING}/sel_universal${EXEC_EXT}',
  # NOTE: -Q skips sel_ldr qualification tests, -c -c skips validation
  'SEL_UNIVERSAL_FLAGS' : '--abort_on_error -B ${IRT_BLOB} ' +
                          '--uses_reverse_service ' +
                          '${USE_EMULATOR ? -Q -c -c --command_prefix ${EMULATOR}}',

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
  'BOOTSTRAP_LDR' : '${SCONS_STAGING}/nacl_helper_bootstrap${EXEC_EXT}',

  # Note: -a enables sel_ldr debug mode allowing us to access to local files
  #       this is only needed if we compile/run the translators in non-srpc mode
  'SB_PREFIX'     : '${USE_BOOTSTRAP ? ${BOOTSTRAP_LDR}} ${SEL_LDR} '
                    '${USE_BOOTSTRAP ? --r_debug=0xXXXXXXXXXXXXXXXXX} ' +
                    '-a -B ${IRT_BLOB} --',

  'LD_SB'         : '${SB_PREFIX} ${TRANSLATOR_BIN_NONSRPC}/ld.nexe',
  'LLC_SB'        : '${SB_PREFIX} ${RUNNABLE_LD} ' +
                    '${TRANSLATOR_BIN_NONSRPC}/llc.nexe',
  'SB_DYNAMIC'    : '0',
  'NNACL_LIBDIR'  : '${BASE_NACL}/toolchain/${SCONS_OS}_x86/' +
                    'x86_64-nacl/${ARCH  == X8632 ? lib32 : lib}',
  'RUNNABLE_LD'   : '${SB_DYNAMIC ? ${NNACL_LIBDIR}/runnable-ld.so ' +
                    '--library-path ${NNACL_LIBDIR}}',

  'LLC_SRPC'      : '${TRANSLATOR_BIN}/llc.nexe',
  'LD_SRPC'       : '${TRANSLATOR_BIN}/ld.nexe',

  # Bitcode LLVM tools
  'CLANG'         : '${BASE_LLVM_BIN}/clang${EXEC_EXT}',
  # 'clang++' doesn't work on Windows (outside of Cygwin),
  # because it is a symlink. '-ccc-cxx' enables C++ mode.
  'CLANGXX'       : '${BASE_LLVM_BIN}/clang${EXEC_EXT} -ccc-cxx',
  'LLVM_OPT'      : '${BASE_LLVM_BIN}/opt${EXEC_EXT}',
  'LLVM_DIS'      : '${BASE_LLVM_BIN}/llvm-dis${EXEC_EXT}',
  # llvm-as compiles llvm assembly (.ll) to bitcode (.bc/.po)
  'LLVM_AS'       : '${BASE_LLVM_BIN}/llvm-as${EXEC_EXT}',
  'LLVM_BCWRAP'   : '${BASE_LLVM_BIN}/bc-wrap${EXEC_EXT}',

  # Native LLVM tools
  'LLVM_LLC'      : '${BASE_LLVM_BIN}/llc${EXEC_EXT}',
  # llvm-mc is llvm's native assembler
  'LLVM_MC'       : '${BASE_LLVM_BIN}/llvm-mc${EXEC_EXT}',


  # DragonEgg (not yet supported)
  'DRAGONEGG_GCC' : '${BASE_GCC}/bin/i686-unknown-linux-gnu-gcc',
  'DRAGONEGG_GXX' : '${BASE_GCC}/bin/i686-unknown-linux-gnu-g++',

  # Binutils
  'BINUTILS_BASE'  : '${BASE_BINUTILS}/bin/arm-pc-nacl-',
  'OBJDUMP'        : '${BINUTILS_BASE}objdump${EXEC_EXT}',
  'NM'             : '${BINUTILS_BASE}nm${EXEC_EXT}',
  'AR'             : '${BINUTILS_BASE}ar${EXEC_EXT}',
  'RANLIB'         : '${BINUTILS_BASE}ranlib${EXEC_EXT}',
  'STRIP'          : '${BINUTILS_BASE}strip${EXEC_EXT}',
  'LD_BFD'         : '${BINUTILS_BASE}ld.bfd${EXEC_EXT}',
  'LD_GOLD'        : '${BINUTILS_BASE}ld.gold${EXEC_EXT}',

  # Use the default command line arguments to the sandboxed translator.
  'USE_DEFAULT_CMD_LINE': '1',
}

######################################################################
#
# Environment
#
######################################################################

def ParseError(s, leftpos, rightpos, msg):
  Log.Error("Parse Error: %s", msg);
  Log.Error('  ' + s);
  Log.Error('  ' + (' '*leftpos) + ('^'*(rightpos - leftpos + 1)))
  DriverExit(1)

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

class Environment(object):
  functions = {}

  @classmethod
  def register(cls, func):
    """ Register a function for use in the evaluator """
    cls.functions[func.__name__] = func
    return func

  def __init__(self):
    self.stack = []
    self.reset()

  def reset(self):
    self.data = dict(INITIAL_ENV)

  def update(self, extra):
    self.data.update(extra)

  def dump(self):
    for (k,v) in self.data.iteritems():
      print '%s == %s' % (k,v)

  def push(self):
    self.stack.append(self.data)
    self.data = dict(self.data) # Make a copy

  def pop(self):
    self.data = self.stack.pop()

  def has(self, varname):
    return varname in self.data

  def getraw(self, varname):
    return self.eval(self.data[varname])

  # Evaluate a variable from the environment.
  # Returns a list of terms.
  def get(self, varname):
    return shell.split(self.getraw(varname))

  # Retrieve a variable from the environment which
  # is a single term. Returns a string.
  def getone(self, varname):
    return shell.unescape(self.getraw(varname))

  def getbool(self, varname):
    return bool(int(self.getone(varname)))

  def setbool(self, varname, val):
    if val:
      self.set(varname, '1')
    else:
      self.set(varname, '0')

  # Set a variable in the environment without shell-escape
  def setraw(self, varname, val):
    self.data[varname] = val

  # Set one or more variables using named arguments
  def setmany(self, **kwargs):
    for k,v in kwargs.iteritems():
      if isinstance(v, str):
        self.set(k, v)
      else:
        self.set(k, *v)

  def clear(self, varname):
    self.data[varname] = ''

  # Set a variable to one or more terms, applying shell-escape.
  def set(self, varname, *vals):
    self.clear(varname)
    self.append(varname, *vals)

  # Append one or more terms to a variable in the
  # environment, applying shell-escape.
  def append(self, varname, *vals):
    escaped = [ shell.escape(v) for v in vals ]
    if len(self.data[varname]) > 0:
      self.data[varname] += ' '
    self.data[varname] += ' '.join(escaped)

  # Evaluate an expression s
  def eval(self, s):
    (result, i) = self.eval_expr(s, 0, [])
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
  # Do not call these functions outside of this class.
  # The env.eval method is the external interface to the evaluator.
  ######################################################################

  # Evaluate a string literal
  def eval_str(self, s, pos, terminators):
    (_,i) = FindFirst(s, pos, terminators)
    return (s[pos:i], i)

  # Evaluate %var% substitutions inside a variable name.
  # Returns (the_actual_variable_name, endpos)
  # Terminated by } character
  def eval_varname(self, s, pos, terminators):
    (_,i) = FindFirst(s, pos, ['%'] + terminators)
    leftpart = s[pos:i].strip(' ')
    if i == len(s) or s[i] in terminators:
      return (leftpart, i)

    (middlepart, j) = self.eval_bracket_expr(s, i+1, ['%'])
    if j == len(s) or s[j] != '%':
      ParseError(s, i, j, "Unterminated %")

    (rightpart, k) = self.eval_varname(s, j+1, terminators)

    fullname = leftpart + middlepart + rightpart
    fullname = fullname.strip()
    return (fullname, k)

  # Absorb whitespace
  def eval_whitespace(self, s, pos):
    i = pos
    while i < len(s) and s[i] == ' ':
      i += 1
    return (None, i)

  def eval_bool_val(self, s, pos, terminators):
    (_,i) = self.eval_whitespace(s, pos)

    if s[i] == '!':
      negated = True
      i += 1
    else:
      negated = False

    (_,i) = self.eval_whitespace(s, i)

    if s[i] == '#':
      uselen = True
      i += 1
    else:
      uselen = False

    (varname, j) = self.eval_varname(s, i, ['=']+terminators)
    if j == len(s):
      # This is an error condition one level up. Don't evaluate anything.
      return (False, j)

    if varname not in self.data:
      ParseError(s, i, j, "Undefined variable '%s'" % varname)
    vardata = self.data[varname]

    contents = self.eval(vardata)

    if s[j] == '=':
      # String equality test
      if j+1 == len(s) or s[j+1] != '=':
        ParseError(s, j, j, "Unexpected token")
      if uselen:
        ParseError(s, j, j, "Cannot combine == and #")
      (_,j) = self.eval_whitespace(s, j+2)
      (literal_str,j) = self.eval_str(s, j, [' ']+terminators)
      (_,j) = self.eval_whitespace(s, j)
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
  def eval_bool_expr(self, s, pos, terminators):
    (boolval1, i) = self.eval_bool_val(s, pos, ['&','|']+terminators)
    if i == len(s):
      # This is an error condition one level up. Don't evaluate anything.
      return (False, i)

    if s[i] in ('&','|'):
      # and/or expression
      if i+1 == len(s) or s[i+1] != s[i]:
        ParseError(s, i, i, "Unexpected token")
      is_and = (s[i] == '&')

      (boolval2, j) = self.eval_bool_expr(s, i+2, terminators)
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
  def eval_bracket_expr(self, s, pos, terminators):
    (_,pos) = self.eval_whitespace(s, pos)

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

      val = self.functions[funcname](*args)
      contents = self.eval(val)
      return (contents, j)

    (m,_) = FindFirst(s, pos, ['?']+terminators)
    if m != '?':
      # Regular variable substitution
      (varname,i) = self.eval_varname(s, pos, terminators)
      if len(s) == i:
        return ('', i)  # Error one level up
      if varname not in self.data:
        ParseError(s, pos, i, "Undefined variable '%s'" % varname)
      vardata = self.data[varname]
      contents = self.eval(vardata)
      return (contents, i)
    else:
      # Ternary Mode
      (is_cond_true,i) = self.eval_bool_expr(s, pos, ['?']+terminators)
      assert(i < len(s) and s[i] == '?')

      (if_true_expr, j) = self.eval_expr(s, i+1, [' : ']+terminators)
      if j == len(s):
        return ('', j) # Error one level up

      if s[j:j+3] == ' : ':
        (if_false_expr,j) = self.eval_expr(s, j+3, terminators)
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
  def eval_expr(self, s, pos, terminators):
    (m,i) = FindFirst(s, pos, ['${'] + terminators)
    leftpart = s[pos:i]
    if i == len(s) or m in terminators:
      return (leftpart, i)

    (middlepart, j) = self.eval_bracket_expr(s, i+2, ['}'])
    if j == len(s) or s[j] != '}':
      ParseError(s, i, j, 'Unterminated ${')

    (rightpart, k) = self.eval_expr(s, j+1, terminators)
    return (leftpart + middlepart + rightpart, k)

env = Environment()
