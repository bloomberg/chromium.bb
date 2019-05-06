#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to build clang binaries. It is used by package.py to
create the prebuilt binaries downloaded by update.py and used by developers.

The expectation is that update.py downloads prebuilt binaries for everyone, and
nobody should run this script as part of normal development.
"""

from __future__ import print_function

import argparse
import glob
import os
import pipes
import re
import shutil
import subprocess
import sys

from update import (CDS_URL, CHROMIUM_DIR, CLANG_REVISION, LLVM_BUILD_DIR,
                    FORCE_HEAD_REVISION_FILE, PACKAGE_VERSION, RELEASE_VERSION,
                    STAMP_FILE, CopyFile, CopyDiaDllTo, DownloadAndUnpack,
                    EnsureDirExists, GetWinSDKDir, ReadStampFile, RmTree,
                    WriteStampFile)


# Path constants. (All of these should be absolute paths.)
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
CHROME_TOOLS_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'chrometools')
THREADS_ENABLED_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'threads_enabled')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'compiler-rt')
CLANG_DIR = os.path.join(LLVM_DIR, 'tools', 'clang')
LLD_DIR = os.path.join(LLVM_DIR, 'tools', 'lld')
# TODO(thakis): Use projects/compiler-rt on Linux too once tests pass there.
if sys.platform in ['darwin', 'win32']:
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'projects', 'compiler-rt')
else:
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'compiler-rt')
LIBCXX_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxx')
LIBCXXABI_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxxabi')
LLVM_BUILD_TOOLS_DIR = os.path.abspath(
    os.path.join(LLVM_DIR, '..', 'llvm-build-tools'))
ANDROID_NDK_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'android_ndk')
FUCHSIA_SDK_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'fuchsia-sdk',
                               'sdk')

LLVM_REPO_URL = os.environ.get('LLVM_REPO_URL',
                               'https://llvm.org/svn/llvm-project')

BUG_REPORT_URL = ('https://crbug.com and run tools/clang/scripts/upload_crash.py'
                  ' (only works inside Google) which will upload a report')


def GetSvnRevision(svn_repo):
  """Returns current revision of the svn repo at svn_repo."""
  svn_info = subprocess.check_output('svn info ' + svn_repo, shell=True)
  m = re.search(r'Revision: (\d+)', svn_info)
  return m.group(1)


def RmCmakeCache(dir):
  """Delete CMake cache related files from dir."""
  for dirpath, dirs, files in os.walk(dir):
    if 'CMakeCache.txt' in files:
      os.remove(os.path.join(dirpath, 'CMakeCache.txt'))
    if 'CMakeFiles' in dirs:
      RmTree(os.path.join(dirpath, 'CMakeFiles'))


def RunCommand(command, msvc_arch=None, env=None, fail_hard=True):
  """Run command and return success (True) or failure; or if fail_hard is
     True, exit on failure.  If msvc_arch is set, runs the command in a
     shell with the msvc tools for that architecture."""

  if msvc_arch and sys.platform == 'win32':
    command = [os.path.join(GetWinSDKDir(), 'bin', 'SetEnv.cmd'),
               "/" + msvc_arch, '&&'] + command

  # https://docs.python.org/2/library/subprocess.html:
  # "On Unix with shell=True [...] if args is a sequence, the first item
  # specifies the command string, and any additional items will be treated as
  # additional arguments to the shell itself.  That is to say, Popen does the
  # equivalent of:
  #   Popen(['/bin/sh', '-c', args[0], args[1], ...])"
  #
  # We want to pass additional arguments to command[0], not to the shell,
  # so manually join everything into a single string.
  # Annoyingly, for "svn co url c:\path", pipes.quote() thinks that it should
  # quote c:\path but svn can't handle quoted paths on Windows.  Since on
  # Windows follow-on args are passed to args[0] instead of the shell, don't
  # do the single-string transformation there.
  if sys.platform != 'win32':
    command = ' '.join([pipes.quote(c) for c in command])
  print('Running', command)
  if subprocess.call(command, env=env, shell=True) == 0:
    return True
  print('Failed.')
  if fail_hard:
    sys.exit(1)
  return False


def CopyDirectoryContents(src, dst):
  """Copy the files from directory src to dst."""
  dst = os.path.realpath(dst)  # realpath() in case dst ends in /..
  EnsureDirExists(dst)
  for f in os.listdir(src):
    CopyFile(os.path.join(src, f), dst)


def Checkout(name, url, dir):
  """Checkout the SVN module at url into dir. Use name for the log message."""
  print("Checking out %s r%s into '%s'" % (name, CLANG_REVISION, dir))

  command = ['svn', 'checkout', '--force', url + '@' + CLANG_REVISION, dir]
  # The checkout command usually succeeds and produces lots of unininteresting
  # output. Hence, pass `--quiet` on the first run and run an explicit command
  # to print the revision we got afterwards on the first attempt.
  if RunCommand(command + ['--quiet'], fail_hard=False):
    RunCommand(['svnversion', dir])
    return

  if os.path.isdir(dir):
    print("Removing %s." % dir)
    RmTree(dir)

  print("Retrying.")
  RunCommand(command)


def CheckoutRepos(args):
  if args.skip_checkout:
    return

  Checkout('LLVM', LLVM_REPO_URL + '/llvm/trunk', LLVM_DIR)
  Checkout('Clang', LLVM_REPO_URL + '/cfe/trunk', CLANG_DIR)
  if True:
    Checkout('LLD', LLVM_REPO_URL + '/lld/trunk', LLD_DIR)
  elif os.path.exists(LLD_DIR):
    # In case someone sends a tryjob that temporary adds lld to the checkout,
    # make sure it's not around on future builds.
    RmTree(LLD_DIR)
  # Remove compiler-rt at old location.
  if (sys.platform == 'darwin' and
      os.path.exists(os.path.join(LLVM_DIR, 'compiler-rt'))):
    RmTree(os.path.join(LLVM_DIR, 'compiler-rt'))
  Checkout('compiler-rt', LLVM_REPO_URL + '/compiler-rt/trunk', COMPILER_RT_DIR)
  if sys.platform == 'darwin':
    # clang needs a libc++ checkout, else -stdlib=libc++ won't find includes
    # (i.e. this is needed for bootstrap builds).
    Checkout('libcxx', LLVM_REPO_URL + '/libcxx/trunk', LIBCXX_DIR)
    # We used to check out libcxxabi on OS X; we no longer need that.
    if os.path.exists(LIBCXXABI_DIR):
      RmTree(LIBCXXABI_DIR)


def DeleteChromeToolsShim():
  OLD_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'zzz-chrometools')
  shutil.rmtree(OLD_SHIM_DIR, ignore_errors=True)
  shutil.rmtree(CHROME_TOOLS_SHIM_DIR, ignore_errors=True)


def CreateChromeToolsShim():
  """Hooks the Chrome tools into the LLVM build.

  Several Chrome tools have dependencies on LLVM/Clang libraries. The LLVM build
  detects implicit tools in the tools subdirectory, so this helper install a
  shim CMakeLists.txt that forwards to the real directory for the Chrome tools.

  Note that the shim directory name intentionally has no - or _. The implicit
  tool detection logic munges them in a weird way."""
  assert not any(i in os.path.basename(CHROME_TOOLS_SHIM_DIR) for i in '-_')
  os.mkdir(CHROME_TOOLS_SHIM_DIR)
  with file(os.path.join(CHROME_TOOLS_SHIM_DIR, 'CMakeLists.txt'), 'w') as f:
    f.write('# Automatically generated by tools/clang/scripts/update.py. ' +
            'Do not edit.\n')
    f.write('# Since tools/clang is located in another directory, use the \n')
    f.write('# two arg version to specify where build artifacts go. CMake\n')
    f.write('# disallows reuse of the same binary dir for multiple source\n')
    f.write('# dirs, so the build artifacts need to go into a subdirectory.\n')
    f.write('if (CHROMIUM_TOOLS_SRC)\n')
    f.write('  add_subdirectory(${CHROMIUM_TOOLS_SRC} ' +
              '${CMAKE_CURRENT_BINARY_DIR}/a)\n')
    f.write('endif (CHROMIUM_TOOLS_SRC)\n')


def AddSvnToPathOnWin():
  """Download svn.exe and add it to PATH."""
  if sys.platform != 'win32':
    return
  svn_ver = 'svn-1.6.6-win'
  svn_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, svn_ver)
  if not os.path.exists(svn_dir):
    DownloadAndUnpack(CDS_URL + '/tools/%s.zip' % svn_ver, LLVM_BUILD_TOOLS_DIR)
  os.environ['PATH'] = svn_dir + os.pathsep + os.environ.get('PATH', '')


def AddCMakeToPath(args):
  """Download CMake and add it to PATH."""
  if args.use_system_cmake:
    return

  if sys.platform == 'win32':
    zip_name = 'cmake-3.12.1-win32-x86.zip'
    dir_name = ['cmake-3.12.1-win32-x86', 'bin']
  elif sys.platform == 'darwin':
    zip_name = 'cmake-3.12.1-Darwin-x86_64.tar.gz'
    dir_name = ['cmake-3.12.1-Darwin-x86_64', 'CMake.app', 'Contents', 'bin']
  else:
    zip_name = 'cmake-3.12.1-Linux-x86_64.tar.gz'
    dir_name = ['cmake-3.12.1-Linux-x86_64', 'bin']

  cmake_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, *dir_name)
  if not os.path.exists(cmake_dir):
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  os.environ['PATH'] = cmake_dir + os.pathsep + os.environ.get('PATH', '')


def AddGnuWinToPath():
  """Download some GNU win tools and add them to PATH."""
  if sys.platform != 'win32':
    return

  gnuwin_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'gnuwin')
  GNUWIN_VERSION = '10'
  GNUWIN_STAMP = os.path.join(gnuwin_dir, 'stamp')
  if ReadStampFile(GNUWIN_STAMP) == GNUWIN_VERSION:
    print('GNU Win tools already up to date.')
  else:
    zip_name = 'gnuwin-%s.zip' % GNUWIN_VERSION
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
    WriteStampFile(GNUWIN_VERSION, GNUWIN_STAMP)

  os.environ['PATH'] = gnuwin_dir + os.pathsep + os.environ.get('PATH', '')

  # find.exe, mv.exe and rm.exe are from MSYS (see crrev.com/389632). MSYS uses
  # Cygwin under the hood, and initializing Cygwin has a race-condition when
  # getting group and user data from the Active Directory is slow. To work
  # around this, use a horrible hack telling it not to do that.
  # See https://crbug.com/905289
  etc = os.path.join(gnuwin_dir, '..', '..', 'etc')
  EnsureDirExists(etc)
  with open(os.path.join(etc, 'nsswitch.conf'), 'w') as f:
    f.write('passwd: files\n')
    f.write('group: files\n')


def SetMacXcodePath():
  """Set DEVELOPER_DIR to the path to hermetic Xcode.app on Mac OS X."""
  if sys.platform != 'darwin':
    return

  xcode_path = os.path.join(CHROMIUM_DIR, 'build', 'mac_files', 'Xcode.app')
  if os.path.exists(xcode_path):
    os.environ['DEVELOPER_DIR'] = xcode_path


def VerifyVersionOfBuiltClangMatchesVERSION():
  """Checks that `clang --version` outputs RELEASE_VERSION. If this
  fails, update.RELEASE_VERSION is out-of-date and needs to be updated (possibly
  in an `if args.llvm_force_head_revision:` block inupdate. main() first)."""
  clang = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  if sys.platform == 'win32':
    # TODO: Parse `clang-cl /?` output for built clang's version and check that
    # to check the binary we're actually shipping? But clang-cl.exe is just
    # a copy of clang.exe, so this does check the same thing.
    clang += '.exe'
  version_out = subprocess.check_output([clang, '--version'])
  version_out = re.match(r'clang version ([0-9.]+)', version_out).group(1)
  if version_out != RELEASE_VERSION:
    print(('unexpected clang version %s (not %s), '
           'update RELEASE_VERSION in update.py')
          % (version_out, RELEASE_VERSION))
    sys.exit(1)


def gn_arg(v):
  if v == 'True':
    return True
  if v == 'False':
    return False
  raise argparse.ArgumentTypeError('Expected one of %r or %r' % (
      'True', 'False'))


def main():
  parser = argparse.ArgumentParser(description='Build Clang.')
  parser.add_argument('--bootstrap', action='store_true',
                      help='first build clang with CC, then with itself.')
  parser.add_argument('--disable-asserts', action='store_true',
                      help='build with asserts disabled')
  parser.add_argument('--gcc-toolchain', help='(no longer used)')
  parser.add_argument('--lto-lld', action='store_true',
                      help='build lld with LTO')
  parser.add_argument('--llvm-force-head-revision', action='store_true',
                      help='build the latest revision')
  parser.add_argument('--run-tests', action='store_true',
                      help='run tests after building')
  parser.add_argument('--skip-build', action='store_true',
                      help='do not build anything')
  parser.add_argument('--skip-checkout', action='store_true',
                      help='do not create or update any checkouts')
  parser.add_argument('--extra-tools', nargs='*', default=[],
                      help='select additional chrome tools to build')
  parser.add_argument('--use-system-cmake', action='store_true',
                      help='use the cmake from PATH instead of downloading '
                      'and using prebuilt cmake binaries')
  parser.add_argument('--with-android', type=gn_arg, nargs='?', const=True,
                      help='build the Android ASan runtime (linux only)',
                      default=sys.platform.startswith('linux'))
  parser.add_argument('--without-android', action='store_false',
                      help='don\'t build Android ASan runtime (linux only)',
                      dest='with_android')
  parser.add_argument('--without-fuchsia', action='store_false',
                      help='don\'t build Fuchsia clang_rt runtime (linux/mac)',
                      dest='with_fuchsia',
                      default=sys.platform in ('linux2', 'darwin'))
  args = parser.parse_args()

  if args.lto_lld and not args.bootstrap:
    print('--lto-lld requires --bootstrap')
    return 1
  if args.lto_lld and not sys.platform.startswith('linux'):
    print('--lto-lld is only effective on Linux. Ignoring the option.')
    args.lto_lld = False
  if args.with_android and not os.path.exists(ANDROID_NDK_DIR):
    print('Android NDK not found at ' + ANDROID_NDK_DIR)
    print('The Android NDK is needed to build a Clang whose -fsanitize=address')
    print('works on Android. See ')
    print('https://www.chromium.org/developers/how-tos/android-build-instructions')
    print('for how to install the NDK, or pass --without-android.')
    return 1

  if args.llvm_force_head_revision:
    # Don't build fuchsia runtime on ToT bots at all.
    args.with_fuchsia = False

  if args.with_fuchsia and not os.path.exists(FUCHSIA_SDK_DIR):
    print('Fuchsia SDK not found at ' + FUCHSIA_SDK_DIR)
    print('The Fuchsia SDK is needed to build libclang_rt for Fuchsia.')
    print('Install the Fuchsia SDK by adding fuchsia to the ')
    print('target_os section in your .gclient and running hooks, ')
    print('or pass --without-fuchsia.')
    print('https://chromium.googlesource.com/chromium/src/+/master/docs/fuchsia_build_instructions.md')
    print('for general Fuchsia build instructions.')
    return 1


  # DEVELOPER_DIR needs to be set when Xcode isn't in a standard location
  # and xcode-select wasn't run.  This is needed for running clang and ld
  # for the build done by this script, but it's also needed for running
  # macOS system svn, so this needs to happen before calling functions using
  # svn.
  SetMacXcodePath()

  AddSvnToPathOnWin()

  global CLANG_REVISION, PACKAGE_VERSION
  if args.llvm_force_head_revision:
    CLANG_REVISION = GetSvnRevision(LLVM_REPO_URL)
    PACKAGE_VERSION = CLANG_REVISION + '-0'

  # Don't buffer stdout, so that print statements are immediately flushed.
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

  print('Locally building clang %s...' % PACKAGE_VERSION)
  WriteStampFile('', STAMP_FILE)
  WriteStampFile('', FORCE_HEAD_REVISION_FILE)

  AddCMakeToPath(args)
  AddGnuWinToPath()

  DeleteChromeToolsShim()

  CheckoutRepos(args)

  if args.skip_build:
    return

  cc, cxx = None, None

  cflags = []
  cxxflags = []
  ldflags = []

  targets = 'AArch64;ARM;Mips;PowerPC;SystemZ;WebAssembly;X86'
  base_cmake_args = ['-GNinja',
                     '-DCMAKE_BUILD_TYPE=Release',
                     '-DLLVM_ENABLE_ASSERTIONS=%s' %
                         ('OFF' if args.disable_asserts else 'ON'),
                     '-DLLVM_ENABLE_PIC=OFF',
                     '-DLLVM_ENABLE_TERMINFO=OFF',
                     '-DLLVM_TARGETS_TO_BUILD=' + targets,
                     # Statically link MSVCRT to avoid DLL dependencies.
                     '-DLLVM_USE_CRT_RELEASE=MT',
                     '-DCLANG_PLUGIN_SUPPORT=OFF',
                     '-DCLANG_ENABLE_STATIC_ANALYZER=OFF',
                     '-DCLANG_ENABLE_ARCMT=OFF',
                     # TODO(crbug.com/929645): Use newer toolchain to host.
                     '-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON',
                     '-DBUG_REPORT_URL=' + BUG_REPORT_URL,
                     ]

  if sys.platform != 'win32':
    # libxml2 is required by the Win manifest merging tool used in cross-builds.
    base_cmake_args.append('-DLLVM_ENABLE_LIBXML2=FORCE_ON')

  if args.bootstrap:
    print('Building bootstrap compiler')
    EnsureDirExists(LLVM_BOOTSTRAP_DIR)
    os.chdir(LLVM_BOOTSTRAP_DIR)
    bootstrap_args = base_cmake_args + [
        '-DLLVM_TARGETS_TO_BUILD=X86;ARM;AArch64',
        '-DCMAKE_INSTALL_PREFIX=' + LLVM_BOOTSTRAP_INSTALL_DIR,
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        # Ignore args.disable_asserts for the bootstrap compiler.
        '-DLLVM_ENABLE_ASSERTIONS=ON',
        ]
    if cc is not None:  bootstrap_args.append('-DCMAKE_C_COMPILER=' + cc)
    if cxx is not None: bootstrap_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
    RmCmakeCache('.')
    RunCommand(['cmake'] + bootstrap_args + [LLVM_DIR], msvc_arch='x64')
    RunCommand(['ninja'], msvc_arch='x64')
    if args.run_tests:
      if sys.platform == 'win32':
        CopyDiaDllTo(os.path.join(LLVM_BOOTSTRAP_DIR, 'bin'))
      RunCommand(['ninja', 'check-all'], msvc_arch='x64')
    RunCommand(['ninja', 'install'], msvc_arch='x64')

    if sys.platform == 'win32':
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      # CMake has a hard time with backslashes in compiler paths:
      # https://stackoverflow.com/questions/13050827
      cc = cc.replace('\\', '/')
      cxx = cxx.replace('\\', '/')
    else:
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang++')

    print('Building final compiler')

  # LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
  # needed, on OS X it requires libc++. clang only automatically links to libc++
  # when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run
  # on OS X versions as old as 10.7.
  deployment_target = ''

  if sys.platform == 'darwin' and args.bootstrap:
    # When building on 10.9, /usr/include usually doesn't exist, and while
    # Xcode's clang automatically sets a sysroot, self-built clangs don't.
    cflags = ['-isysroot', subprocess.check_output(
        ['xcrun', '--show-sdk-path']).rstrip()]
    cxxflags = ['-stdlib=libc++'] + cflags
    ldflags += ['-stdlib=libc++']
    deployment_target = '10.7'

  # If building at head, define a macro that plugins can use for #ifdefing
  # out code that builds at head, but not at CLANG_REVISION or vice versa.
  if args.llvm_force_head_revision:
    cflags += ['-DLLVM_FORCE_HEAD_REVISION']
    cxxflags += ['-DLLVM_FORCE_HEAD_REVISION']

  # Build PDBs for archival on Windows.  Don't use RelWithDebInfo since it
  # has different optimization defaults than Release.
  # Also disable stack cookies (/GS-) for performance.
  if sys.platform == 'win32':
    cflags += ['/Zi', '/GS-']
    cxxflags += ['/Zi', '/GS-']
    ldflags += ['/DEBUG', '/OPT:REF', '/OPT:ICF']

  deployment_env = None
  if deployment_target:
    deployment_env = os.environ.copy()
    deployment_env['MACOSX_DEPLOYMENT_TARGET'] = deployment_target

  # Build lld and code coverage tools. This is done separately from the rest of
  # the build because these tools require threading support.
  tools_with_threading = [ 'dsymutil', 'lld', 'llvm-cov', 'llvm-profdata' ]
  print('Building the following tools with threading support: %s' %
        str(tools_with_threading))

  if os.path.exists(THREADS_ENABLED_BUILD_DIR):
    RmTree(THREADS_ENABLED_BUILD_DIR)
  EnsureDirExists(THREADS_ENABLED_BUILD_DIR)
  os.chdir(THREADS_ENABLED_BUILD_DIR)

  threads_enabled_cmake_args = base_cmake_args + [
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
      '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags)]
  if cc is not None:
    threads_enabled_cmake_args.append('-DCMAKE_C_COMPILER=' + cc)
  if cxx is not None:
    threads_enabled_cmake_args.append('-DCMAKE_CXX_COMPILER=' + cxx)

  if args.lto_lld:
    # Build lld with LTO. That speeds up the linker by ~10%.
    # We only use LTO for Linux now.
    #
    # The linker expects all archive members to have symbol tables, so the
    # archiver needs to be able to create symbol tables for bitcode files.
    # GNU ar and ranlib don't understand bitcode files, but llvm-ar and
    # llvm-ranlib do, so use them.
    ar = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'llvm-ar')
    ranlib = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'llvm-ranlib')
    threads_enabled_cmake_args += [
        '-DCMAKE_AR=' + ar,
        '-DCMAKE_RANLIB=' + ranlib,
        '-DLLVM_ENABLE_LTO=thin',
        '-DLLVM_USE_LINKER=lld']

  RmCmakeCache('.')
  RunCommand(['cmake'] + threads_enabled_cmake_args + [LLVM_DIR],
             msvc_arch='x64', env=deployment_env)
  RunCommand(['ninja'] + tools_with_threading, msvc_arch='x64')

  # Build clang and other tools.
  CreateChromeToolsShim()

  cmake_args = []
  # TODO(thakis): Unconditionally append this to base_cmake_args instead once
  # compiler-rt can build with clang-cl on Windows (http://llvm.org/PR23698)
  cc_args = base_cmake_args if sys.platform != 'win32' else cmake_args
  if cc is not None:  cc_args.append('-DCMAKE_C_COMPILER=' + cc)
  if cxx is not None: cc_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
  default_tools = ['plugins', 'blink_gc_plugin', 'translation_unit']
  chrome_tools = list(set(default_tools + args.extra_tools))
  cmake_args += base_cmake_args + [
      '-DLLVM_ENABLE_THREADS=OFF',
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
      '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_INSTALL_PREFIX=' + LLVM_BUILD_DIR,
      '-DCHROMIUM_TOOLS_SRC=%s' % os.path.join(CHROMIUM_DIR, 'tools', 'clang'),
      '-DCHROMIUM_TOOLS=%s' % ';'.join(chrome_tools)]
  if sys.platform == 'darwin':
    cmake_args += ['-DCOMPILER_RT_ENABLE_IOS=ON',
                   '-DSANITIZER_MIN_OSX_VERSION=10.7']

  EnsureDirExists(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)
  RmCmakeCache('.')
  RunCommand(['cmake'] + cmake_args + [LLVM_DIR],
             msvc_arch='x64', env=deployment_env)
  # FIXME: are compiler-rt, fuzzer part of the default target?
  RunCommand(['ninja'], msvc_arch='x64')

  # Copy in the threaded versions of lld and other tools.
  if sys.platform == 'win32':
    CopyFile(os.path.join(THREADS_ENABLED_BUILD_DIR, 'bin', 'lld-link.exe'),
             os.path.join(LLVM_BUILD_DIR, 'bin'))
    CopyFile(os.path.join(THREADS_ENABLED_BUILD_DIR, 'bin', 'lld.pdb'),
             os.path.join(LLVM_BUILD_DIR, 'bin'))
  else:
    for tool in tools_with_threading:
      CopyFile(os.path.join(THREADS_ENABLED_BUILD_DIR, 'bin', tool),
               os.path.join(LLVM_BUILD_DIR, 'bin'))

  if chrome_tools:
    # If any Chromium tools were built, install those now.
    RunCommand(['ninja', 'cr-install'], msvc_arch='x64')

  VerifyVersionOfBuiltClangMatchesVERSION()

  if sys.platform == 'win32':
    platform = 'windows'
  elif sys.platform == 'darwin':
    platform = 'darwin'
  else:
    assert sys.platform.startswith('linux')
    platform = 'linux'
  rt_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                RELEASE_VERSION, 'lib', platform)

  # Do an out-of-tree build of compiler-rt.
  # On Windows, this is used to get the 32-bit ASan run-time.
  # TODO(hans): Remove once the regular build above produces this.
  if sys.platform != 'darwin':
    if os.path.isdir(COMPILER_RT_BUILD_DIR):
      RmTree(COMPILER_RT_BUILD_DIR)
    os.makedirs(COMPILER_RT_BUILD_DIR)
    os.chdir(COMPILER_RT_BUILD_DIR)
    # TODO(thakis): Add this once compiler-rt can build with clang-cl (see
    # above).
    #if args.bootstrap and sys.platform == 'win32':
      # The bootstrap compiler produces 64-bit binaries by default.
      #cflags += ['-m32']
      #cxxflags += ['-m32']
    compiler_rt_args = base_cmake_args + [
        '-DLLVM_ENABLE_THREADS=OFF',
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags)]
    if sys.platform != 'win32':
      compiler_rt_args += ['-DLLVM_CONFIG_PATH=' +
                           os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-config'),
                          ]
    RmCmakeCache('.')
    RunCommand(['cmake'] + compiler_rt_args +
               [LLVM_DIR if sys.platform == 'win32' else COMPILER_RT_DIR],
               msvc_arch='x86', env=deployment_env)
    RunCommand(['ninja', 'compiler-rt'], msvc_arch='x86')

    # Copy select output to the main tree.
    # TODO(hans): Make this (and the .gypi and .isolate files) version number
    # independent.
    rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', platform)
    if sys.platform == 'win32':
      # TODO(thakis): This too is due to compiler-rt being part of the checkout
      # on Windows, see TODO above COMPILER_RT_DIR.
      rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', 'clang',
                                    RELEASE_VERSION, 'lib', platform)
    # Blacklists:
    CopyDirectoryContents(os.path.join(rt_lib_src_dir, '..', '..', 'share'),
                          os.path.join(rt_lib_dst_dir, '..', '..', 'share'))
    # Headers:
    if sys.platform != 'win32':
      CopyDirectoryContents(
          os.path.join(COMPILER_RT_BUILD_DIR, 'include/sanitizer'),
          os.path.join(LLVM_BUILD_DIR, 'lib/clang', RELEASE_VERSION,
                       'include/sanitizer'))
    # Static and dynamic libraries:
    CopyDirectoryContents(rt_lib_src_dir, rt_lib_dst_dir)

  if args.with_android:
    make_toolchain = os.path.join(
        ANDROID_NDK_DIR, 'build', 'tools', 'make_standalone_toolchain.py')
    for target_arch in ['aarch64', 'arm', 'i686']:
      # Make standalone Android toolchain for target_arch.
      toolchain_dir = os.path.join(
          LLVM_BUILD_DIR, 'android-toolchain-' + target_arch)
      api_level = '21' if target_arch == 'aarch64' else '19'
      RunCommand([
          make_toolchain,
          '--api=' + api_level,
          '--force',
          '--install-dir=%s' % toolchain_dir,
          '--stl=libc++',
          '--arch=' + {
              'aarch64': 'arm64',
              'arm': 'arm',
              'i686': 'x86',
          }[target_arch]])

      # NDK r16 "helpfully" installs libc++ as libstdc++ "so the compiler will
      # pick it up by default". Only these days, the compiler tries to find
      # libc++ instead. See https://crbug.com/902270.
      shutil.copy(os.path.join(toolchain_dir, 'sysroot/usr/lib/libstdc++.a'),
                  os.path.join(toolchain_dir, 'sysroot/usr/lib/libc++.a'))
      shutil.copy(os.path.join(toolchain_dir, 'sysroot/usr/lib/libstdc++.so'),
                  os.path.join(toolchain_dir, 'sysroot/usr/lib/libc++.so'))

      # Build compiler-rt runtimes needed for Android in a separate build tree.
      build_dir = os.path.join(LLVM_BUILD_DIR, 'android-' + target_arch)
      if not os.path.exists(build_dir):
        os.mkdir(os.path.join(build_dir))
      os.chdir(build_dir)
      target_triple = target_arch
      if target_arch == 'arm':
        target_triple = 'armv7'
      target_triple += '-linux-android' + api_level
      cflags = ['--target=%s' % target_triple,
                '--sysroot=%s/sysroot' % toolchain_dir,
                '-B%s' % toolchain_dir]
      android_args = base_cmake_args + [
        '-DLLVM_ENABLE_THREADS=OFF',
        '-DCMAKE_C_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_CXX_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang++'),
        '-DLLVM_CONFIG_PATH=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-config'),
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_ASM_FLAGS=' + ' '.join(cflags),
        '-DSANITIZER_CXX_ABI=libcxxabi',
        '-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-u__cxa_demangle',
        '-DANDROID=1']
      RmCmakeCache('.')
      RunCommand(['cmake'] + android_args + [COMPILER_RT_DIR])

      # We use ASan i686 build for fuzzing.
      libs_want = ['lib/linux/libclang_rt.asan-{0}-android.so']
      if target_arch in ['aarch64', 'arm']:
        libs_want += [
            'lib/linux/libclang_rt.ubsan_standalone-{0}-android.so',
            'lib/linux/libclang_rt.profile-{0}-android.a',
        ]
      if target_arch == 'aarch64':
        libs_want += ['lib/linux/libclang_rt.hwasan-{0}-android.so']
      libs_want = [lib.format(target_arch) for lib in libs_want]
      RunCommand(['ninja'] + libs_want)

      # And copy them into the main build tree.
      for p in libs_want:
        shutil.copy(p, rt_lib_dst_dir)

  if args.with_fuchsia:
    # Fuchsia links against libclang_rt.builtins-<arch>.a instead of libgcc.a.
    for target_arch in ['aarch64', 'x86_64']:
      fuchsia_arch_name = {'aarch64': 'arm64', 'x86_64': 'x64'}[target_arch]
      toolchain_dir = os.path.join(
          FUCHSIA_SDK_DIR, 'arch', fuchsia_arch_name, 'sysroot')
      # Build clang_rt runtime for Fuchsia in a separate build tree.
      build_dir = os.path.join(LLVM_BUILD_DIR, 'fuchsia-' + target_arch)
      if not os.path.exists(build_dir):
        os.mkdir(os.path.join(build_dir))
      os.chdir(build_dir)
      target_spec = target_arch + '-fuchsia'
      # TODO(thakis): Might have to pass -B here once sysroot contains
      # binaries (e.g. gas for arm64?)
      fuchsia_args = base_cmake_args + [
        '-DLLVM_ENABLE_THREADS=OFF',
        '-DCMAKE_C_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_CXX_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang++'),
        '-DCMAKE_LINKER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_AR=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-ar'),
        '-DLLVM_CONFIG_PATH=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-config'),
        '-DCMAKE_SYSTEM_NAME=Fuchsia',
        '-DCMAKE_C_COMPILER_TARGET=%s-fuchsia' % target_arch,
        '-DCMAKE_ASM_COMPILER_TARGET=%s-fuchsia' % target_arch,
        '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON',
        '-DCMAKE_SYSROOT=%s' % toolchain_dir,
        # TODO(thakis|scottmg): Use PER_TARGET_RUNTIME_DIR for all platforms.
        # https://crbug.com/882485.
        '-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON',

        # These are necessary because otherwise CMake tries to build an
        # executable to test to see if the compiler is working, but in doing so,
        # it links against the builtins.a that we're about to build.
        '-DCMAKE_C_COMPILER_WORKS=ON',
        '-DCMAKE_ASM_COMPILER_WORKS=ON',
        ]
      RmCmakeCache('.')
      RunCommand(['cmake'] +
                 fuchsia_args +
                 [os.path.join(COMPILER_RT_DIR, 'lib', 'builtins')])
      builtins_a = 'libclang_rt.builtins.a'
      RunCommand(['ninja', builtins_a])

      # And copy it into the main build tree.
      fuchsia_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                         RELEASE_VERSION, target_spec, 'lib')
      if not os.path.exists(fuchsia_lib_dst_dir):
        os.makedirs(fuchsia_lib_dst_dir)
      CopyFile(os.path.join(build_dir, target_spec, 'lib', builtins_a),
               fuchsia_lib_dst_dir)

  # Run tests.
  if args.run_tests or args.llvm_force_head_revision:
    os.chdir(LLVM_BUILD_DIR)
    RunCommand(['ninja', 'cr-check-all'], msvc_arch='x64')
  if args.run_tests:
    if sys.platform == 'win32':
      CopyDiaDllTo(os.path.join(LLVM_BUILD_DIR, 'bin'))
    os.chdir(LLVM_BUILD_DIR)
    test_targets = [ 'check-all' ]
    if sys.platform == 'darwin':
      # TODO(thakis): Run check-all on Darwin too, https://crbug.com/959361
      test_targets = [ 'check-llvm', 'check-clang', 'check-lld' ]
    RunCommand(['ninja'] + test_targets, msvc_arch='x64')

  if sys.platform == 'darwin':
    for dylib in glob.glob(os.path.join(rt_lib_dst_dir, '*.dylib')):
      # Fix LC_ID_DYLIB for the ASan dynamic libraries to be relative to
      # @executable_path.
      # Has to happen after running tests.
      # TODO(glider): this is transitional. We'll need to fix the dylib
      # name either in our build system, or in Clang. See also
      # http://crbug.com/344836.
      subprocess.call(['install_name_tool', '-id',
                       '@executable_path/' + os.path.basename(dylib), dylib])

  WriteStampFile(PACKAGE_VERSION, STAMP_FILE)
  WriteStampFile(PACKAGE_VERSION, FORCE_HEAD_REVISION_FILE)

  print('Clang build was successful.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
