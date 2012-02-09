#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.

# Global environment and expression parsing for the PNaCl driver


# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class env" defined later for the implementation.

from shelltools import shell
from driver_log import Log, DriverExit

INITIAL_ENV = {
  'BASE_NACL'       : '${@FindBaseNaCl}',   # Absolute path of native_client/
  'BASE'            : '${@FindBasePNaCl}',  # Absolute path to PNaCl
  'BASE_DRIVER'     : '${@FindBaseDriver}', # Location of PNaCl drivers
  'BUILD_OS'        : '${@GetBuildOS}',     # "linux", "darwin" or "windows"
  'BUILD_ARCH'      : '${@GetBuildArch}',   # "x86_64" or "i686" or "i386"
  'DRIVER_EXT'      : '${@GetDriverExt}',   # '.py' or ''

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
  'FORCE_INTERMEDIATE_S': '0',
  'USE_EMULATOR'        : '0',
  'USE_BOOTSTRAP'       : '${BUILD_OS==linux ? 1 : 0}',
  'DRIVER_FLAGS'        : '', # Flags passed to the driver
  'LIBMODE'             : '', # glibc or newlib (set by ReadConfig)
  'LIBMODE_GLIBC'       : '${LIBMODE==glibc ? 1 : 0}',
  'LIBMODE_NEWLIB'      : '${LIBMODE==newlib ? 1 : 0}',

  # Logging settings
  'LOG_VERBOSE'          : '0', # Activated with --pnacl-driver-verbose
  'LOG_TO_FILE'          : '1',
  'LOG_FILENAME'         : '${BASE}/driver.log',
  'LOG_FILE_SIZE_LIMIT'  : str(20 * 1024 * 1024),

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
  'GOLD_PLUGIN_SO'  : '${BASE_LLVM}/${SO_DIR}/${SO_PREFIX}LLVMgold${SO_EXT}',
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

  'LD_SB'         : '${SB_PREFIX} ${BASE_SB}/nonsrpc/bin/ld',
  'LLC_SB'        : '${SB_PREFIX} ${RUNNABLE_LD} ${BASE_SB}/nonsrpc/bin/llc',
  'SB_DYNAMIC'    : '0',
  'NNACL_LIBDIR'  : '${BASE_NACL}/toolchain/${SCONS_OS}_x86/' +
                    'x86_64-nacl/${ARCH  == X8632 ? lib32 : lib}',
  'RUNNABLE_LD'   : '${SB_DYNAMIC ? ${NNACL_LIBDIR}/runnable-ld.so ' +
                    '--library-path ${NNACL_LIBDIR}}',

  'LLC_SRPC'      : '${BASE_SB}/srpc/bin/llc',
  'LD_SRPC'       : '${BASE_SB}/srpc/bin/ld',

  # Bitcode LLVM tools
  'CLANG'         : '${BASE_LLVM_BIN}/clang${EXEC_EXT}',
  'CLANGXX'       : '${BASE_LLVM_BIN}/clang++${EXEC_EXT}',
  'LLVM_OPT'      : '${BASE_LLVM_BIN}/opt${EXEC_EXT}',
  'LLVM_DIS'      : '${BASE_LLVM_BIN}/llvm-dis${EXEC_EXT}',
  # llvm-as compiles llvm assembly (.ll) to bitcode (.bc/.po)
  'LLVM_AS'       : '${BASE_LLVM_BIN}/llvm-as${EXEC_EXT}',

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
  'USE_DEFAULT_CMD_LINE': '0',
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

  @classmethod
  def setbool(cls, varname, val):
    if val:
      cls.set(varname, '1')
    else:
      cls.set(varname, '0')

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

      (if_true_expr, j) = cls.eval_expr(s, i+1, [' : ']+terminators)
      if j == len(s):
        return ('', j) # Error one level up

      if s[j:j+3] == ' : ':
        (if_false_expr,j) = cls.eval_expr(s, j+3, terminators)
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
