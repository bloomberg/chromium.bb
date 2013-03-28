#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""NaCl SDK tool SCons."""

import __builtin__
import re
import os
import shutil
import sys
import SCons.Scanner
import SCons.Script
import subprocess
import tempfile


# Mapping from env['PLATFORM'] (scons) to a single host platform from the point
# of view of NaCl.
NACL_CANONICAL_PLATFORM_MAP = {
    'win32': 'win',
    'cygwin': 'win',
    'posix': 'linux',
    'linux': 'linux',
    'linux2': 'linux',
    'darwin': 'mac',
}

NACL_TOOL_MAP = {
    'arm': {
        '32': {
            'tooldir': 'arm-nacl',
            'as_flag': '',
            'cc_flag': '',
            'ld_flag': '',
            },
        },
    'x86': {
        '32': {
            'tooldir': 'i686-nacl',
            'other_libdir': 'lib32',
            'as_flag': '--32',
            'cc_flag': '-m32',
            'ld_flag': ' -melf_nacl',
            },
        '64': {
            'tooldir': 'x86_64-nacl',
            'other_libdir': 'lib64',
            'as_flag': '--64',
            'cc_flag': '-m64',
            'ld_flag': ' -melf64_nacl',
            },
        },
    }

def _StubOutEnvToolsForBuiltElsewhere(env):
  """Stub out all tools so that they point to 'true'.

  Some machines have their code built by another machine, they'll therefore
  run 'true' instead of running the usual build tools.

  Args:
    env: The SCons environment in question.
  """
  assert(env.Bit('built_elsewhere'))
  env.Replace(CC='true', CXX='true', LINK='true', AR='true',
              RANLIB='true', AS='true', ASPP='true', LD='true',
              STRIP='true')

def _PlatformSubdirs(env):
  if env.Bit('bitcode'):
    os = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
    name = 'pnacl_%s_x86' % os
  else:
    platform = NACL_CANONICAL_PLATFORM_MAP[env['PLATFORM']]
    arch = env['BUILD_ARCHITECTURE']
    subarch = env['TARGET_SUBARCH']
    name = platform + '_' + arch
    if not env.Bit('nacl_glibc'):
      name = name + '_newlib'
  return name


def _GetNaClSdkRoot(env, sdk_mode, psdk_mode):
  """Return the path to the sdk.

  Args:
    env: The SCons environment in question.
    sdk_mode: A string indicating which location to select the tools from.
  Returns:
    The path to the sdk.
  """

  # Allow pnacl to override its path separately.  See comments for why
  # psdk_mode is separate from sdk_mode.
  if env.Bit('bitcode') and psdk_mode.startswith('custom:'):
    return os.path.abspath(psdk_mode[len('custom:'):])

  if sdk_mode == 'local':
    if env['PLATFORM'] in ['win32', 'cygwin']:
      # Try to use cygpath under the assumption we are running thru cygwin.
      # If this is not the case, then 'local' doesn't really make any sense,
      # so then we should complain.
      try:
        path = subprocess.Popen(
            ['cygpath', '-m', '/usr/local/nacl-sdk'],
            env={'PATH': os.environ['PRESCONS_PATH']}, shell=True,
            stdout=subprocess.PIPE).communicate()[0].replace('\n', '')
      except WindowsError:
        raise NotImplementedError(
            'Not able to decide where /usr/local/nacl-sdk is on this platform,'
            'use naclsdk_mode=custom:...')
      return path
    else:
      return '/usr/local/nacl-sdk'

  elif sdk_mode == 'download':
    tcname = _PlatformSubdirs(env)
    return os.path.join(env['MAIN_DIR'], 'toolchain', tcname)
  elif sdk_mode.startswith('custom:'):
    return os.path.abspath(sdk_mode[len('custom:'):])

  elif sdk_mode == 'manual':
    return None

  else:
    raise Exception('Unknown sdk mode: %r' % sdk_mode)


def _SetEnvForNativeSdk(env, sdk_path):
  """Initialize environment according to target architecture."""

  bin_path = os.path.join(sdk_path, 'bin')
  # NOTE: attempts to eliminate this PATH setting and use
  #       absolute path have been futile
  env.PrependENVPath('PATH', bin_path)

  tool_prefix = None
  tool_map = NACL_TOOL_MAP[env['TARGET_ARCHITECTURE']]
  subarch_spec = tool_map[env['TARGET_SUBARCH']]
  tooldir = subarch_spec['tooldir']
  if os.path.exists(os.path.join(sdk_path, tooldir)):
    # The tooldir for the build target exists.
    # The tools there do the right thing without special options.
    tool_prefix = tooldir
    libdir = os.path.join(tooldir, 'lib')
    as_mode_flag = ''
    cc_mode_flag = ''
    ld_mode_flag = ''
  else:
    # We're building for a target for which there is no matching tooldir.
    # For example, for x86-32 when only <sdk_path>/x86_64-nacl/ exists.
    # Find a tooldir for a different subarch that does exist.
    others_map = tool_map.copy()
    del others_map[env['TARGET_SUBARCH']]
    for subarch, tool_spec in others_map.iteritems():
      tooldir = tool_spec['tooldir']
      if os.path.exists(os.path.join(sdk_path, tooldir)):
        # OK, this is the other subarch to use as tooldir.
        tool_prefix = tooldir
        # We need to pass it extra options for the subarch we are building.
        as_mode_flag = subarch_spec['as_flag']
        cc_mode_flag = subarch_spec['cc_flag']
        ld_mode_flag = subarch_spec['ld_flag']
        # The lib directory may have an alternate name, i.e.
        # 'lib32' in the x86_64-nacl tooldir.
        libdir = os.path.join(tooldir, subarch_spec.get('other_libdir', 'lib'))
        break

  if tool_prefix is None:
    raise Exception("Cannot find a toolchain for %s in %s" %
                    (env['TARGET_FULLARCH'], sdk_path))

  env.Replace(# Replace header and lib paths.
              # where to put nacl extra sdk headers
              # TODO(robertm): switch to using the mechanism that
              #                passes arguments to scons
              NACL_SDK_INCLUDE='%s/%s/include' % (sdk_path, tool_prefix),
              # where to find/put nacl generic extra sdk libraries
              NACL_SDK_LIB='%s/%s' % (sdk_path, libdir),
              # Replace the normal unix tools with the NaCl ones.
              CC=os.path.join(bin_path, '%s-gcc' % tool_prefix),
              CXX=os.path.join(bin_path, '%s-g++' % tool_prefix),
              AR=os.path.join(bin_path, '%s-ar' % tool_prefix),
              AS=os.path.join(bin_path, '%s-as' % tool_prefix),
              ASPP=os.path.join(bin_path, '%s-gcc' % tool_prefix),
              GDB=os.path.join(bin_path, '%s-gdb' % tool_prefix),
              # NOTE: use g++ for linking so we can handle C AND C++.
              LINK=os.path.join(bin_path, '%s-g++' % tool_prefix),
              # Grrr... and sometimes we really need ld.
              LD=os.path.join(bin_path, '%s-ld' % tool_prefix) + ld_mode_flag,
              RANLIB=os.path.join(bin_path, '%s-ranlib' % tool_prefix),
              NM=os.path.join(bin_path, '%s-nm' % tool_prefix),
              OBJDUMP=os.path.join(bin_path, '%s-objdump' % tool_prefix),
              STRIP=os.path.join(bin_path, '%s-strip' % tool_prefix),
              ADDR2LINE=os.path.join(bin_path, '%s-addr2line' % tool_prefix),
              BASE_LINKFLAGS=[cc_mode_flag],
              BASE_CFLAGS=[cc_mode_flag],
              BASE_CXXFLAGS=[cc_mode_flag],
              BASE_ASFLAGS=[as_mode_flag],
              BASE_ASPPFLAGS=[cc_mode_flag],
              CFLAGS=['-std=gnu99'],
              CCFLAGS=['-O3',
                       '-Werror',
                       '-Wall',
                       '-Wno-variadic-macros',
                       '-Wswitch-enum',
                       '-g',
                       '-fno-stack-protector',
                       '-fdiagnostics-show-option',
                       '-pedantic',
                       '-D__linux__',
                       ],
              ASFLAGS=[],
              )

  # NaClSdk environment seems to be inherited from the host environment.
  # On Linux host, this probably makes sense. On Windows and Mac, this
  # introduces nothing except problems.
  # For now, simply override the environment settings as in
  # <scons>/engine/SCons/Platform/posix.py
  env.Replace(LIBPREFIX='lib',
              LIBSUFFIX='.a',
              SHLIBPREFIX='$LIBPREFIX',
              SHLIBSUFFIX='.so',
              LIBPREFIXES=['$LIBPREFIX'],
              LIBSUFFIXES=['$LIBSUFFIX', '$SHLIBSUFFIX'],
              )
  # Force -fPIC when compiling for shared libraries.
  env.AppendUnique(SHCCFLAGS=['-fPIC'],
                   )

def _SetEnvForPnacl(env, root):
  # All the PNaCl tools require Python to be in the PATH.
  # On the Windows bots, however, Python is not installed system-wide.
  # It must be pulled from ../third_party/python_26.
  python_dir = os.path.join('..', 'third_party', 'python_26')
  env.AppendENVPath('PATH', python_dir)

  arch = env['TARGET_FULLARCH']
  assert arch in ['arm', 'arm-thumb2', 'mips32', 'x86-32', 'x86-64']

  arch_flag = ' -arch %s' % arch
  if env.Bit('pnacl_generate_pexe'):
    ld_arch_flag = ''
  else:
    ld_arch_flag = arch_flag

  if env.Bit('nacl_glibc'):
    subroot = root + '/glibc'
  else:
    subroot = root + '/newlib'

  translator_root = os.path.join(os.path.dirname(root), 'pnacl_translator')

  binprefix = os.path.join(subroot, 'bin', 'pnacl-')
  binext = ''
  if env.Bit('host_windows'):
    binext = '.bat'

  if env.Bit('nacl_glibc'):
    # TODO(pdox): This bias is needed because runnable-ld is
    # expected to be in the same directory as the SDK.
    # This assumption should be removed.
    pnacl_lib = os.path.join(root, 'lib-%s' % arch)
    pnacl_extra_lib = os.path.join(subroot, 'lib')
  else:
    pnacl_lib = os.path.join(subroot, 'lib')
    pnacl_extra_lib = ''

  #TODO(robertm): remove NACL_SDK_INCLUDE ASAP
  if env.Bit('nacl_glibc'):
    pnacl_include = os.path.join(root, 'glibc', 'usr', 'include')
  else:
    pnacl_include = os.path.join(root, 'newlib', 'usr', 'include')
  pnacl_ar = binprefix + 'ar' + binext
  pnacl_as = binprefix + 'as' + binext
  pnacl_nm = binprefix + 'nm' + binext
  pnacl_ranlib = binprefix + 'ranlib' + binext
  # Use the standalone sandboxed translator in sbtc mode
  if env.Bit('use_sandboxed_translator'):
    pnacl_translate = os.path.join(translator_root, 'bin',
                                   'pnacl-translate' + binext)
  else:
    pnacl_translate = binprefix + 'translate' + binext

  pnacl_cc = binprefix + 'clang' + binext
  pnacl_cxx = binprefix + 'clang++' + binext

  pnacl_ld = binprefix + 'ld' + binext
  pnacl_nativeld = binprefix + 'nativeld' + binext
  pnacl_disass = binprefix + 'dis' + binext
  pnacl_finalize = binprefix + 'finalize' + binext
  pnacl_strip = binprefix + 'strip' + binext
  pnacl_nmf = binprefix + 'nmf' + binext
  pnacl_link_and_translate = os.path.join(subroot,
                                          'bin',
                                          'wrapper-link-and-translate') + binext

  # NOTE: XXX_flags start with space for easy concatenation
  # The flags generated here get baked into the commands (CC, CXX, LINK)
  # instead of CFLAGS etc to keep them from getting blown away by some
  # tests. Don't add flags here unless they always need to be preserved.
  pnacl_cxx_flags = ''
  pnacl_cc_flags = ' -std=gnu99'
  pnacl_ld_flags = ' ' + ' '.join(env['PNACL_BCLDFLAGS'])
  pnacl_translate_flags = ''

  if env.Bit('nacl_pic'):
    pnacl_cc_flags += ' -fPIC'
    pnacl_cxx_flags += ' -fPIC'
    # NOTE: this is a special hack for the pnacl backend which
    #       does more than linking
    pnacl_ld_flags += ' -fPIC'
    pnacl_translate_flags += ' -fPIC'

  if env.Bit('use_sandboxed_translator'):
    sb_flags = ' --pnacl-sb'
    pnacl_ld_flags += sb_flags
    pnacl_translate_flags += sb_flags

  if env.Bit('x86_64_zero_based_sandbox'):
    pnacl_translate_flags += ' -sfi-zero-based-sandbox'

  if pnacl_extra_lib:
    env.Prepend(LIBPATH=pnacl_extra_lib)

  env.Replace(# Replace header and lib paths.
              NACL_SDK_INCLUDE=pnacl_include,
              NACL_SDK_LIB=pnacl_lib,
              # Replace the normal unix tools with the PNaCl ones.
              CC=pnacl_cc + pnacl_cc_flags,
              CXX=pnacl_cxx + pnacl_cxx_flags,
              ASPP=pnacl_cc + pnacl_cc_flags,
              LIBPREFIX="lib",
              SHLIBPREFIX="lib",
              SHLIBSUFFIX=".so",
              OBJSUFFIX=".bc",
              LINK=pnacl_cxx + ld_arch_flag + pnacl_ld_flags,
              # Although we are currently forced to produce native output
              # for LINK, we are free to produce bitcode for SHLINK
              # (SharedLibrary linking) because scons doesn't do anything
              # with shared libraries except use them with the toolchain.
              SHLINK=pnacl_cxx + ld_arch_flag + pnacl_ld_flags,
              LD=pnacl_ld,
              NATIVELD=pnacl_nativeld,
              AR=pnacl_ar,
              AS=pnacl_as + ld_arch_flag,
              RANLIB=pnacl_ranlib,
              DISASS=pnacl_disass,
              OBJDUMP=pnacl_disass,
              STRIP=pnacl_strip,
              GENNMF=pnacl_nmf,
              TRANSLATE=pnacl_translate + arch_flag + pnacl_translate_flags,
              PNACLFINALIZE=pnacl_finalize,
              )

  if env.Bit('pnacl_shared_newlib'):
    def shlibemitter(target, source, env):
      """when building a .so also notify scons that we care about
      the .pso which gets generated as a side-effect and which should
      also be installed.
      This is a not very well documented scons API.
      """
      if env.Bit('pnacl_generate_pexe'):
        return (target, source)
      assert len(target) == 1
      lib = env.GetBuildPath(target[0])
      assert lib.endswith(".so")
      return (target + [lib[:-2] + 'pso'], source)

    env.Replace(LINK=pnacl_link_and_translate + arch_flag + ' -dynamic',
                SHLINK=pnacl_link_and_translate + arch_flag,
                SHLIBEMITTER=shlibemitter)


  if env.Bit('built_elsewhere'):
    def FakeInstall(dest, source, env):
      print 'Not installing', dest
    _StubOutEnvToolsForBuiltElsewhere(env)
    env.Replace(INSTALL=FakeInstall)
    if env.Bit('translate_in_build_step'):
      env.Replace(TRANSLATE='true')
    env.Replace(PNACLFINALIZE='true')


def _SetEnvForSdkManually(env):
  def GetEnvOrDummy(v):
    return os.getenv('NACL_SDK_' + v, 'MISSING_SDK_' + v)

  env.Replace(# Replace header and lib paths.
              NACL_SDK_INCLUDE=GetEnvOrDummy('INCLUDE'),
              NACL_SDK_LIB=GetEnvOrDummy('LIB'),
              # Replace the normal unix tools with the NaCl ones.
              CC=GetEnvOrDummy('CC'),
              CXX=GetEnvOrDummy('CXX'),
              AR=GetEnvOrDummy('AR'),
              # NOTE: use g++ for linking so we can handle c AND c++
              LINK=GetEnvOrDummy('LINK'),
              RANLIB=GetEnvOrDummy('RANLIB'),
              )

def PNaClForceNative(env):
  assert(env.Bit('bitcode'))
  if env.Bit('pnacl_generate_pexe'):
    env.Replace(CC='NO-NATIVE-CC-INVOCATION-ALLOWED',
                CXX='NO-NATIVE-CXX-INVOCATION-ALLOWED')
    return

  env.Replace(OBJSUFFIX='.o',
              SHLIBSUFFIX='.so')
  arch_flag = ' -arch ${TARGET_FULLARCH}'
  cc_flags = ' --pnacl-allow-native --pnacl-allow-translate'
  env.Append(CC=arch_flag + cc_flags,
             CXX=arch_flag + cc_flags,
             ASPP=arch_flag + cc_flags,
             LINK=cc_flags) # Already has -arch
  env['LD'] = '${NATIVELD}' + arch_flag
  env['SHLINK'] = '${LINK}'
  if env.Bit('built_elsewhere'):
    _StubOutEnvToolsForBuiltElsewhere(env)

# Get an environment for nacl-gcc when in PNaCl mode.
def PNaClGetNNaClEnv(env):
  assert(env.Bit('bitcode'))
  assert(not env.Bit('target_mips32'))

  # This is kind of a hack. We clone the environment,
  # clear the bitcode bit, and then reload naclsdk.py
  native_env = env.Clone()
  native_env.ClearBits('bitcode')
  native_env.SetBits('native_code')
  if env.Bit('built_elsewhere'):
    _StubOutEnvToolsForBuiltElsewhere(env)
  else:
    native_env = native_env.Clone(tools=['naclsdk'])
    if native_env.Bit('pnacl_generate_pexe'):
      native_env.Replace(CC='NO-NATIVE-CC-INVOCATION-ALLOWED',
                         CXX='NO-NATIVE-CXX-INVOCATION-ALLOWED')
    else:
      # These are unfortunately clobbered by running Tool.
      native_env.Replace(EXTRA_CFLAGS=env['EXTRA_CFLAGS'],
                         EXTRA_CXXFLAGS=env['EXTRA_CXXFLAGS'],
                         CCFLAGS=env['CCFLAGS'],
                         CFLAGS=env['CFLAGS'],
                         CXXFLAGS=env['CXXFLAGS'])
  return native_env


# This adds architecture specific defines for the target architecture.
# These are normally omitted by PNaCl.
# For example: __i686__, __arm__, __mips__, __x86_64__
def AddBiasForPNaCl(env, temporarily_allow=True):
  assert(env.Bit('bitcode'))
  # re: the temporarily_allow flag -- that is for:
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=1248
  if env.Bit('pnacl_generate_pexe') and not temporarily_allow:
    env.Replace(CC='NO-NATIVE-CC-INVOCATION-ALLOWED',
                CXX='NO-NATIVE-CXX-INVOCATION-ALLOWED')
    return

  if env.Bit('target_arm'):
    env.AppendUnique(CCFLAGS=['--pnacl-arm-bias'],
                     ASPPFLAGS=['--pnacl-arm-bias'])
  elif env.Bit('target_x86_32'):
    env.AppendUnique(CCFLAGS=['--pnacl-i686-bias'],
                     ASPPFLAGS=['--pnacl-i686-bias'])
  elif env.Bit('target_x86_64'):
    env.AppendUnique(CCFLAGS=['--pnacl-x86_64-bias'],
                     ASPPFLAGS=['--pnacl-x86_64-bias'])
  elif env.Bit('target_mips32'):
    env.AppendUnique(CCFLAGS=['--pnacl-mips-bias'],
                     ASPPFLAGS=['--pnacl-mips-bias'])
  else:
    raise Exception("Unknown architecture!")


def ValidateSdk(env):
  checkables = ['${NACL_SDK_INCLUDE}/stdio.h']
  for c in checkables:
    if os.path.exists(env.subst(c)):
      continue
    # Windows build does not use cygwin and so can not see nacl subdirectory
    # if it's cygwin's symlink - check for /include instead...
    if os.path.exists(re.sub(r'(nacl64|nacl)/include/([^/]*)$',
                             r'include/\2',
                             env.subst(c))):
      continue
    # TODO(pasko): remove the legacy header presence test below.
    if os.path.exists(re.sub(r'nacl/include/([^/]*)$',
                             r'nacl64/include/\1',
                             env.subst(c))):
      continue
    message = env.subst('''
ERROR: NativeClient toolchain does not seem present!,
       Missing: %s

Configuration is:
  NACL_SDK_INCLUDE=${NACL_SDK_INCLUDE}
  NACL_SDK_LIB=${NACL_SDK_LIB}
  CC=${CC}
  CXX=${CXX}
  AR=${AR}
  AS=${AS}
  ASPP=${ASPP}
  LINK=${LINK}
  RANLIB=${RANLIB}

Run: gclient runhooks --force or build the SDK yourself.
''' % c)
    sys.stderr.write(message + "\n\n")
    sys.exit(-1)


def ScanLinkerScript(node, env, libpath):
  """SCons scanner for linker script files.
This handles trivial linker scripts like those used for libc.so and libppapi.a.
These scripts just indicate more input files to be linked in, so we want
to produce dependencies on them.

A typical such linker script looks like:

        /* Some comments.  */
        INPUT ( foo.a libbar.a libbaz.a )

or:

        /* GNU ld script
           Use the shared library, but some functions are only in
           the static library, so try that secondarily.  */
        OUTPUT_FORMAT(elf64-x86-64)
        GROUP ( /lib/libc.so.6 /usr/lib/libc_nonshared.a
                AS_NEEDED ( /lib/ld-linux-x86-64.so.2 ) )
"""
  contents = node.get_text_contents()
  if contents.startswith('!<arch>\n') or contents.startswith('\177ELF'):
    # An archive or ELF file is not a linker script.
    return []

  comment_pattern = re.compile(r'/\*.*?\*/', re.DOTALL | re.MULTILINE)
  def remove_comments(text):
    return re.sub(comment_pattern, '', text)

  tokens = remove_comments(contents).split()
  libs = []
  while tokens:
    token = tokens.pop()
    if token.startswith('OUTPUT_FORMAT('):
      pass
    elif token == 'OUTPUT_FORMAT':
      # Swallow the next three tokens: '(', 'xyz', ')'
      del tokens[0:2]
    elif token in ['(', ')', 'INPUT', 'GROUP', 'AS_NEEDED']:
      pass
    else:
      libs.append(token)

  # Find those items in the library path, ignoring ones we fail to find.
  found = [SCons.Node.FS.find_file(lib, libpath) for lib in libs]
  return [lib for lib in found if lib is not None]

# This is a modified copy of the class TempFileMunge in
# third_party/scons-2.0.1/engine/SCons/Platform/__init__.py.
# It differs in using quote_for_at_file (below) in place of
# SCons.Subst.quote_spaces.
class NaClTempFileMunge(object):
  """A callable class.  You can set an Environment variable to this,
  then call it with a string argument, then it will perform temporary
  file substitution on it.  This is used to circumvent the long command
  line limitation.

  Example usage:
  env["TEMPFILE"] = TempFileMunge
  env["LINKCOM"] = "${TEMPFILE('$LINK $TARGET $SOURCES')}"

  By default, the name of the temporary file used begins with a
  prefix of '@'.  This may be configred for other tool chains by
  setting '$TEMPFILEPREFIX'.

  env["TEMPFILEPREFIX"] = '-@'        # diab compiler
  env["TEMPFILEPREFIX"] = '-via'      # arm tool chain
  """
  def __init__(self, cmd):
    self.cmd = cmd

  def __call__(self, target, source, env, for_signature):
    if for_signature:
      # If we're being called for signature calculation, it's
      # because we're being called by the string expansion in
      # Subst.py, which has the logic to strip any $( $) that
      # may be in the command line we squirreled away.  So we
      # just return the raw command line and let the upper
      # string substitution layers do their thing.
      return self.cmd

    # Now we're actually being called because someone is actually
    # going to try to execute the command, so we have to do our
    # own expansion.
    cmd = env.subst_list(self.cmd, SCons.Subst.SUBST_CMD, target, source)[0]
    try:
      maxline = int(env.subst('$MAXLINELENGTH'))
    except ValueError:
      maxline = 2048

    length = 0
    for c in cmd:
      length += len(c)
    if length <= maxline:
      return self.cmd

    # We do a normpath because mktemp() has what appears to be
    # a bug in Windows that will use a forward slash as a path
    # delimiter.  Windows's link mistakes that for a command line
    # switch and barfs.
    #
    # We use the .lnk suffix for the benefit of the Phar Lap
    # linkloc linker, which likes to append an .lnk suffix if
    # none is given.
    (fd, tmp) = tempfile.mkstemp('.lnk', text=True)
    native_tmp = SCons.Util.get_native_path(os.path.normpath(tmp))

    if env['SHELL'] and env['SHELL'] == 'sh':
      # The sh shell will try to escape the backslashes in the
      # path, so unescape them.
      native_tmp = native_tmp.replace('\\', r'\\\\')
      # In Cygwin, we want to use rm to delete the temporary
      # file, because del does not exist in the sh shell.
      rm = env.Detect('rm') or 'del'
    else:
      # Don't use 'rm' if the shell is not sh, because rm won't
      # work with the Windows shells (cmd.exe or command.com) or
      # Windows path names.
      rm = 'del'

    prefix = env.subst('$TEMPFILEPREFIX')
    if not prefix:
      prefix = '@'

    # The @file is sometimes handled by a GNU tool itself, using
    # the libiberty/argv.c code, and sometimes handled implicitly
    # by Cygwin before the tool's own main even sees it.  These
    # two treat the contents differently, so there is no single
    # perfect way to quote.  The libiberty @file code uses a very
    # regular scheme: a \ in any context is always swallowed and
    # quotes the next character, whatever it is; '...' or "..."
    # quote whitespace in ... and the outer quotes are swallowed.
    # The Cygwin @file code uses a vaguely similar scheme, but its
    # treatment of \ is much less consistent: a \ outside a quoted
    # string is never stripped, and a \ inside a quoted string is
    # only stripped when it quoted something (Cygwin's definition
    # of "something" here is nontrivial).  In our uses the only
    # appearances of \ we expect are in Windows-style file names.
    # Fortunately, an extra doubling of \\ that doesn't get
    # stripped is harmless in the middle of a file name.
    def quote_for_at_file(s):
      s = str(s)
      if ' ' in s or '\t' in s:
        return '"' + re.sub('([ \t"])', r'\\\1', s) + '"'
      return s.replace('\\', '\\\\')

    args = list(map(quote_for_at_file, cmd[1:]))
    os.write(fd, " ".join(args) + "\n")
    os.close(fd)
    # XXX Using the SCons.Action.print_actions value directly
    # like this is bogus, but expedient.  This class should
    # really be rewritten as an Action that defines the
    # __call__() and strfunction() methods and lets the
    # normal action-execution logic handle whether or not to
    # print/execute the action.  The problem, though, is all
    # of that is decided before we execute this method as
    # part of expanding the $TEMPFILE construction variable.
    # Consequently, refactoring this will have to wait until
    # we get more flexible with allowing Actions to exist
    # independently and get strung together arbitrarily like
    # Ant tasks.  In the meantime, it's going to be more
    # user-friendly to not let obsession with architectural
    # purity get in the way of just being helpful, so we'll
    # reach into SCons.Action directly.
    if SCons.Action.print_actions:
      print("Using tempfile "+native_tmp+" for command line:\n"+
            str(cmd[0]) + " " + " ".join(args))
    return [ cmd[0], prefix + native_tmp + '\n' + rm, native_tmp ]

def generate(env):
  """SCons entry point for this tool.

  Args:
    env: The SCons environment in question.

  NOTE: SCons requires the use of this name, which fails lint.
  """

  # make these methods to the top level scons file
  env.AddMethod(ValidateSdk)
  env.AddMethod(AddBiasForPNaCl)
  env.AddMethod(PNaClForceNative)
  env.AddMethod(PNaClGetNNaClEnv)

  # Get configuration option for getting the naclsdk.  Default is download.
  # We have a separate flag for pnacl "psdk_mode" since even with PNaCl,
  # the native nacl-gcc toolchain is used to build some parts like
  # the irt-core.
  sdk_mode = SCons.Script.ARGUMENTS.get('naclsdk_mode', 'download')
  psdk_mode = SCons.Script.ARGUMENTS.get('pnaclsdk_mode', 'default')
  if psdk_mode != 'default' and not psdk_mode.startswith('custom:'):
    raise Exception(
      'pnaclsdk_mode only supports "default" or "custom:path", not %s' %
      psdk_mode)

  # Invoke the various unix tools that the NativeClient SDK resembles.
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  if env.Bit('pnacl_generate_pexe'):
    suffix = '.nonfinal.pexe'
  else:
    suffix = '.nexe'

  env.Replace(
      COMPONENT_LINKFLAGS=[''],
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.pso', '.so', '.a'],
      _RPATH='',
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=[],
      PROGSUFFIX=suffix,
      # adding BASE_ AND EXTRA_ flags to common command lines
      # The suggested usage pattern is:
      # BASE_XXXFLAGS can only be set in this file
      # EXTRA_XXXFLAGS can only be set in a ComponentXXX call
      # NOTE: we also have EXTRA_LIBS which is handles separately in
      # site_scons/site_tools/component_builders.py
      # NOTE: the command lines were gleaned from:
      # * ../third_party/scons-2.0.1/engine/SCons/Tool/cc.py
      # * ../third_party/scons-2.0.1/engine/SCons/Tool/c++.py
      # * etc.
      CCCOM='$CC $BASE_CFLAGS $CFLAGS $EXTRA_CFLAGS ' +
            '$CCFLAGS $_CCCOMCOM -c -o $TARGET $SOURCES',
      SHCCCOM='$SHCC $BASE_CFLAGS $SHCFLAGS $EXTRA_CFLAGS ' +
              '$SHCCFLAGS $_CCCOMCOM -c -o $TARGET $SOURCES',

      CXXCOM='$CXX $BASE_CXXFLAGS $CXXFLAGS $EXTRA_CXXFLAGS ' +
             '$CCFLAGS $_CCCOMCOM -c -o $TARGET $SOURCES',
      SHCXXCOM='$SHCXX $BASE_CXXFLAGS $SHCXXFLAGS $EXTRA_CXXFLAGS ' +
               '$SHCCFLAGS $_CCCOMCOM -c -o $TARGET $SOURCES',

      LINKCOM='$LINK $BASE_LINKFLAGS $LINKFLAGS $EXTRA_LINKFLAGS ' +
              '$SOURCES $_LIBDIRFLAGS $_LIBFLAGS -o $TARGET',
      SHLINKCOM='$SHLINK $BASE_LINKFLAGS $SHLINKFLAGS $EXTRA_LINKFLAGS ' +
                '$SOURCES $_LIBDIRFLAGS $_LIBFLAGS -o $TARGET',

      ASCOM='$AS $BASE_ASFLAGS $ASFLAGS $EXTRA_ASFLAGS -o $TARGET $SOURCES',
      ASPPCOM='$ASPP $BASE_ASPPFLAGS $ASPPFLAGS $EXTRA_ASPPFLAGS ' +
              '$CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -c -o $TARGET $SOURCES',

      # Strip doesn't seem to be a first-class citizen in SCons country,
      # so we have to add these *COM, *COMSTR manually.
      # Note: it appears we cannot add this in component_setup.py
      STRIPFLAGS=['--strip-all'],
      STRIPCOM='${STRIP} ${STRIPFLAGS}',
      TRANSLATEFLAGS=['-Wl,-L${LIB_DIR}'],
      TRANSLATECOM='${TRANSLATE} ${TRANSLATEFLAGS} ${SOURCES} -o ${TARGET}',
      PNACLFINALIZEFLAGS=[],
      PNACLFINALIZECOM='${PNACLFINALIZE} ${PNACLFINALIZEFLAGS} ' +
                       '${SOURCES} -o ${TARGET}',
  )

  # Windows has a small limit on the command line size.  The linking and AR
  # commands can get quite large.  So bring in the SCons machinery to put
  # most of a command line into a temporary file and pass it with
  # @filename, which works with gcc.
  if env['PLATFORM'] in ['win32', 'cygwin']:
    env['TEMPFILE'] = NaClTempFileMunge
    for com in ['LINKCOM', 'SHLINKCOM', 'ARCOM']:
      env[com] = "${TEMPFILE('%s')}" % env[com]

  # Get root of the SDK.
  root = _GetNaClSdkRoot(env, sdk_mode, psdk_mode)

  # Determine where to get the SDK from.
  if sdk_mode == 'manual':
    _SetEnvForSdkManually(env)
  else:
    # if bitcode=1 use pnacl toolchain
    if env.Bit('bitcode'):
      _SetEnvForPnacl(env, root)

      # Get GDB from the nacl-gcc toolchain even when using PNaCl.
      # TODO(mseaborn): We really want the nacl-gdb binary to be in a
      # separate tarball from the nacl-gcc toolchain, then this step
      # will not be necessary.
      # See http://code.google.com/p/nativeclient/issues/detail?id=2773
      if env.Bit('target_x86'):
        temp_env = env.Clone()
        temp_env.ClearBits('bitcode')
        temp_root = _GetNaClSdkRoot(temp_env, sdk_mode, psdk_mode)
        _SetEnvForNativeSdk(temp_env, temp_root)
        env.Replace(GDB=temp_env['GDB'])
    else:
      _SetEnvForNativeSdk(env, root)

  env.Prepend(LIBPATH='${NACL_SDK_LIB}')

  # Install our scanner for (potential) linker scripts.
  # It applies to "source" files ending in .a or .so.
  # Dependency files it produces are to be found in ${LIBPATH}.
  # It is applied recursively to those dependencies in case
  # some of them are linker scripts too.
  ldscript_scanner = SCons.Scanner.Base(
      function=ScanLinkerScript,
      skeys=['.a', '.so', '.pso'],
      path_function=SCons.Scanner.FindPathDirs('LIBPATH'),
      recursive=True
      )
  env.Append(SCANNERS=ldscript_scanner)
