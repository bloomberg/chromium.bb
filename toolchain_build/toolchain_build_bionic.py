#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

# Done first to setup python module path.
import toolchain_env

import fnmatch
import hashing_tools
import os
import optparse
import process
import repo_tools
import stat
import shutil
import StringIO
import sys
import toolchain_build
import toolchain_main

from file_update import Mkdir, Rmdir, Symlink
from file_update import NeedsUpdate, UpdateFromTo, UpdateText

BIONIC_VERSION = '1a26187c4121548a46c55dfd13eaf1dfd2f5afa5'
ARCHES = ['arm']

BUILD_SCRIPT = os.path.abspath(__file__)
TOOLCHAIN_BUILD = os.path.dirname(BUILD_SCRIPT)
TOOLCHAIN_BUILD_SRC = os.path.join(TOOLCHAIN_BUILD, 'src')
TOOLCHAIN_BUILD_OUT = os.path.join(TOOLCHAIN_BUILD, 'out')

BIONIC_SRC = os.path.join(TOOLCHAIN_BUILD_SRC, 'bionic')

NATIVE_CLIENT = os.path.dirname(TOOLCHAIN_BUILD)
TOOLCHAIN = os.path.join(NATIVE_CLIENT, 'toolchain')

ARM_NEWLIB = os.path.join(TOOLCHAIN, 'linux_arm_newlib')
ARM_BIONIC = os.path.join(TOOLCHAIN, 'linux_arm_bionic')
X86_NEWLIB = os.path.join(TOOLCHAIN, 'linux_x86_newlib')
X86_BIONIC = os.path.join(TOOLCHAIN, 'linux_x86_bionic')


PROJECTS = [
  'bionic_%s_work',
  'gcc_%s_work',
  'test_%s_work',
]


def ReplaceText(text, maplist):
  for m in maplist:
    for key in m:
      text = text.replace(key, m[key])
  return text


def ReplaceArch(text, arch, subarch=None):
  NACL_ARCHES = {
    'arm': 'arm',
    'x86': 'x86_64'
  }
  GCC_ARCHES = {
    'arm': 'arm',
    'x86': 'i686'
  }
  CPU_ARCHES = {
    'arm': 'arm',
    'x86': 'amd64'
  }
  VERSION_MAP = {
    'arm': '4.8.2',
    'x86': '4.4.3',
  }
  REPLACE_MAP = {
    '$NACL': NACL_ARCHES[arch],
    '$GCC': GCC_ARCHES[arch],
    '$CPU': CPU_ARCHES[arch],
    '$SUB': subarch or '',
    '$ARCH': arch,
    '$VER': VERSION_MAP[arch]
  }
  return ReplaceText(text, [REPLACE_MAP])


def Clobber():
  Rmdir(os.path.join(TOOLCHAIN_BUILD, 'cache'))
  for arch in ARCHES:
    Rmdir(os.path.join(TOOLCHAIN, 'linux_%s_bionic' % arch))
    for workdir in PROJECTS:
      Rmdir(os.path.join(TOOLCHAIN_BUILD_OUT, workdir % arch))


def FetchAndBuildGCC():
  if 'arm' in ARCHES:
    tc_args = ['-y', '--no-use-remote-cache', 'gcc_libs_arm']
    toolchain_main.PackageBuilder(toolchain_build.PACKAGES, tc_args).Main()

  if 'x86' in ARCHES:
    process.Run(['make', 'sync'], cwd=os.path.join(NATIVE_CLIENT, 'tools'))


def FetchBionicSources():
  project = 'bionic'
  url = '%s/nacl-%s.git' % (toolchain_build.GIT_BASE_URL, project)
  repo_tools.SyncGitRepo(url,
                         os.path.join(TOOLCHAIN_BUILD_SRC, project),
                         BIONIC_VERSION)


def CreateBasicToolchain():
  # Create a toolchain directory containing only the toolchain binaries and
  # basic files line nacl_arm_macros.s.

  UpdateFromTo(ARM_NEWLIB, ARM_BIONIC,
               filters=['*arm-nacl/include*', '*arm-nacl/lib*','*.a', '*.o'])
  UpdateFromTo(ARM_NEWLIB, ARM_BIONIC, paterns=['*.s'])

  UpdateFromTo(X86_NEWLIB, X86_BIONIC,
               filters=['*x86_64-nacl/include*', '*x86_64-nacl/lib*','*.o'])
  UpdateFromTo(X86_NEWLIB, X86_BIONIC, paterns=['*.s'])

  #  Static build uses:
  #     crt1.o crti.o 4.8.2/crtbeginT.o ... 4.8.2/crtend.o crtn.o
  # -shared build uses:
  #     crti.o 4.8.2/crtbeginS.o ... 4.8.2/crtendS.o crtn.o crtn.o
  # However we only provide a crtbegin(S) and crtend(S)
  EMPTY = """/*
 * This is a dummy linker script.
 * libnacl.a, libcrt_common.a, crt0.o crt1.o crti.o and crtn.o are all
 * empty.  Instead all the work is done by crtbegin(S).o and crtend(S).o and
 * the bionic libc.  These are provided for compatability with the newlib
 * toolchain binaries.
 */"""
  EMPTY_FILES = ['crt0.o', 'crt1.o', 'crti.o', 'crtn.o',
                 'libnacl.a', 'libcrt_common.a', 'libpthread.a']

  # Bionic uses the following include paths
  BIONIC_PAIRS = [
    ('bionic/libc/include', '$NACL-nacl/include'),
    ('bionic/libc/arch-nacl/syscalls/irt.h', '$NACL-nacl/include/irt.h'),
    ('bionic/libc/arch-nacl/syscalls/irt_syscalls.h',
        '$NACL-nacl/include/irt_syscalls.h'),
    ('bionic/libc/arch-nacl/syscalls/nacl_stat.h',
        '$NACL-nacl/include/nacl_stat.h'),
    ('../../src/untrusted/irt/irt_dev.h', '$NACL-nacl/include/irt_dev.h'),
    ('bionic/libc/arch-$ARCH/include/machine',
        '$NACL-nacl/include/machine'),
    ('bionic/libc/kernel/common', '$NACL-nacl/include'),
    ('bionic/libc/kernel/arch-$ARCH/asm', '$NACL-nacl/include/asm'),
    ('bionic/libm/include', '$NACL-nacl/include'),
    ('bionic/libm/$CPU', '$NACL-nacl/include'),
    ('bionic/safe-iop/include', '$NACL-nacl/include'),
    ('bionic/libstdc++/nacl',
        '$NACL-nacl/include/c++/4.8.2/$NACL-nacl'),
    ('bionic/nacl/$ARCH', '.'),
  ]

  for arch in ARCHES:
    inspath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
    inspath = ReplaceArch(inspath, arch)

    # Create empty objects and libraries
    libpath = ReplaceArch(os.path.join(inspath, '$NACL-nacl', 'lib'), arch)
    for name in EMPTY_FILES:
      UpdateText(os.path.join(libpath, name), EMPTY)

    # Copy BIONIC files to toolchain
    for src, dst in BIONIC_PAIRS:
      srcpath = ReplaceArch(os.path.join(TOOLCHAIN_BUILD_SRC, src), arch)
      dstpath = ReplaceArch(os.path.join(inspath, dst), arch)
      UpdateFromTo(srcpath, dstpath)

    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/libc', workpath, inspath,
                      target='bootstrap')

    # Build specs file
    gcc = ReplaceArch(os.path.join(inspath, 'bin', '$NACL-nacl-gcc'), arch)
    lib = ReplaceArch(os.path.join(inspath, 'lib/gcc/$NACL-nacl/$VER'), arch)
    specs = os.path.join(lib, 'specs')
    with open(specs, 'w') as specfile:
      process.Run([gcc, '-dumpspecs'], cwd=None, shell=False,
                  outfile=specfile, verbose=False)
    text = open(specs, 'r').read()

    # Replace items in the spec file
    text = ReplaceText(text, [{
      '-lgcc': '-lgcc %{!shared: -lgcc_eh}',
      '--hash-style=gnu': '--hash-style=sysv',
    }])

    open(specs, 'w').write(text)


def ConfigureAndBuildBionic():
  for arch in ARCHES:
    inspath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
    inspath = ReplaceArch(inspath, arch)

    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/libc', workpath, inspath)
    ConfigureAndBuild(arch, 'bionic/libm', workpath, inspath)


def ConfigureAndBuildLinker():
  for arch in ARCHES:
    inspath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
    inspath = ReplaceArch(inspath, arch)

    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/linker', workpath, inspath)


def ConfigureAndInstallForGCC(arch, project, cfg, workpath, inspath):
  # configure does not always have +x
  filepath = os.path.abspath(os.path.join(workpath, cfg[0]))
  st_info  = os.stat(filepath)
  os.chmod(filepath, st_info.st_mode | stat.S_IEXEC)

  env = os.environ
  if arch == 'arm':
    newpath = os.path.join(ARM_BIONIC, 'bin') + ':' + env['PATH']
  else:
    newpath = os.path.join(X86_BIONIC, 'bin') + ':' + env['PATH']
  proj = '%s %s' % (project, arch)
  setpath = ['/usr/bin/env', 'PATH=' + newpath]

  # Check if config changed or script is new
  config_path = os.path.join(workpath, 'config.info')
  updated = UpdateText(config_path, ' '.join(cfg))
  updated |= NeedsUpdate(config_path, BUILD_SCRIPT)

  if updated:
    print 'Configure ' + proj
    if process.Run(setpath + cfg, cwd=workpath, env=env, outfile=sys.stdout):
      raise RuntimeError('Failed to configure %s.\n' % proj)
  else:
    print 'Reusing config for %s.' % proj

  print 'Make ' + proj
  if process.Run(setpath + ['make', '-j16', 'V=1'],
                  cwd=workpath, outfile=sys.stdout):
    raise RuntimeError('Failed to build %s.\n' % proj)
  print 'Install ' + proj
  if process.Run(setpath + ['make', '-j16', 'install', 'V=1'],
                 cwd=workpath, outfile=sys.stdout):
    raise RuntimeError('Failed to install %s.\n' % proj)
  print 'Done ' + proj


def ConfigureAndBuildArmGCC(config=False):
  arch = 'arm'
  project = 'libgcc'
  tcpath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
  tcpath = ReplaceArch(tcpath, arch)

  # Prep work path
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_work')
  workpath = ReplaceArch(workpath, arch)
  Mkdir(workpath)
  Symlink('../gcc_libs_arm_work/gcc' , os.path.join(workpath, 'gcc'))

  # Prep install path
  inspath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_install')
  inspath = ReplaceArch(inspath, arch)

  dstpath = ReplaceArch(os.path.join(workpath, '$NACL-nacl/libgcc'), arch)
  cfg = [
    '../../../../src/gcc_libs/libgcc/configure',
    '--host=arm-nacl',
    '--build=i686-linux',
    '--target=arm-nacl',
    '--enable-shared',
    '--enable-shared-libgcc',
    '--with-dwarf3',
    '--with-newlib',
    '--prefix=' + inspath,
    'CFLAGS=-I../../../gcc_lib_arm_work'
  ]
  ConfigureAndInstallForGCC(arch, project, cfg, dstpath, inspath)
  UpdateFromTo(os.path.join(inspath, 'lib', 'gcc'),
               os.path.join(tcpath, 'lib', 'gcc'),
               filters=['*.o'])
  UpdateFromTo(os.path.join(inspath, 'lib', 'libgcc_s.so.1'),
               os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc_s.so.1'))
  UpdateFromTo(os.path.join(inspath, 'lib', 'libgcc_s.so'),
               os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc_s.so'))


def ConfigureAndBuildX86GCC(config=False):
  arch = 'x86'
  project = 'libgcc'

  tcpath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
  tcpath = ReplaceArch(tcpath, arch)

  # Prep work path
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_work')
  workpath = ReplaceArch(workpath, arch)
  Mkdir(workpath)
  Symlink('../gcc_libs_arm_work/gcc' , os.path.join(workpath, 'gcc'))

  # Prep install path
  inspath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_install')
  inspath = ReplaceArch(inspath, arch)

  dstpath = ReplaceArch(os.path.join(workpath, '$NACL-nacl/libgcc'), arch)
  cfg = [
    '../../../../SRC/gcc/libgcc/configure',
    '--target=x86_64-nacl',
    '--enable-shared',
    '--enable-shared-libgcc',
    '--host=x86_64-nacl',
    ',--build=i686-linux'
  ]

def ConfigureAndBuildArmCXX(config=False):
  arch = 'arm'
  project = 'libstdc++'
  tcpath = os.path.join(TOOLCHAIN, 'linux_$ARCH_bionic')
  tcpath = ReplaceArch(tcpath, arch)

  # Prep work path
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'cxx_$GCC_bionic_work')
  workpath = ReplaceArch(workpath, arch)
  Mkdir(workpath)
  Symlink('../gcc_libs_arm_work/gcc' , os.path.join(workpath, 'gcc'))

  # Prep install path
  inspath = os.path.join(TOOLCHAIN_BUILD_OUT, 'cxx_$GCC_bionic_install')
  inspath = ReplaceArch(inspath, arch)

  dstpath = ReplaceArch(os.path.join(workpath, '$NACL-nacl/libstdc++-v3'), arch)
  Mkdir(dstpath)
  cfg = [
    '../../../../src/gcc_libs/libstdc++-v3/configure',
    '--host=arm-nacl',
    '--build=i686-linux',
    '--target=arm-nacl',
    '--enable-shared',
    '--with-newlib',
    '--disable-libstdcxx-pch',
    '--enable-shared-libgcc',
    '--with-dwarf3',
    '--prefix=' + inspath,
    'CFLAGS=-I../../../gcc_lib_arm_work'
  ]
  ConfigureAndInstallForGCC(arch, project, cfg, dstpath, inspath)
  UpdateFromTo(os.path.join(inspath, 'lib'),
               os.path.join(tcpath, 'arm-nacl', 'lib'))
  UpdateFromTo(os.path.join(inspath, 'include'),
               os.path.join(tcpath, 'arm-nacl', 'include'))



def CreateProject(arch, src, dst, ins=None):
  MAKEFILE_TEMPLATE = """
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GNU Makefile based on shared rules provided by the Native Client SDK.
# See README.Makefiles for more details.

TOOLCHAIN_PATH?=$(tc_path)/linux_$ARCH_bionic
TOOLCHAIN_PREFIX:=$(TOOLCHAIN_PATH)/bin/$GCC-nacl-

CC:=$(TOOLCHAIN_PREFIX)gcc
CXX:=$(TOOLCHAIN_PREFIX)g++
AR:=$(TOOLCHAIN_PREFIX)ar

SRC_ROOT=$(src_path)
DST_ROOT=$(dst_path)
INS_ROOT=$(ins_path)

NACL_ARCH=$NACL
GCC_ARCH=$GCC

include $(build_tc_path)/tc_bionic.mk
include $(src_path)/Makefile
"""
  remap = {
    '$(src_path)': src,
    '$(dst_path)': dst,
    '$(ins_path)': ins or dst,
    '$(tc_path)': TOOLCHAIN,
    '$(build_tc_path)': TOOLCHAIN_BUILD
  }
  text = ReplaceText(MAKEFILE_TEMPLATE, [remap])
  text = ReplaceArch(text, arch)

  print 'Create dst dir: ' + dst
  Mkdir(dst)
  if ins:
    print 'Create ins dir: ' + ins
    Mkdir(ins)

  UpdateText(os.path.join(dst, 'Makefile'), text)


def ConfigureAndBuild(arch, project, workpath, inspath,
                      tcpath=None, target=None):

  # Create project for CRTx and LIBC files
  print 'Configure %s for %s.\n' % (project, arch)
  srcpath = os.path.join(TOOLCHAIN_BUILD_SRC, project)
  dstpath = ReplaceArch(os.path.join(workpath, '$NACL-nacl', project), arch)
  libpath = ReplaceArch(os.path.join(inspath, '$NACL-nacl', 'lib'), arch)
  CreateProject(arch, srcpath, dstpath, libpath)
  print 'Building %s for %s at %s.' % (project, arch, dstpath)

  args = ['make', '-j12', 'V=1']
  if target:
    args.append(target)
  if process.Run(args, cwd=dstpath, outfile=sys.stdout):
    raise RuntimeError('Failed to build %s for %s.\n' % (project, arch))
  if tcpath:
    UpdateFromTo(inspath, tcpath)
  print 'Done with %s for %s.\n' % (project, arch)


def ConfigureAndBuildGCC(clobber=False):
  ConfigureAndBuildArmGCC(clobber)


def ConfigureAndBuildCXX(clobber=False):
  ConfigureAndBuildArmCXX(clobber)


def ConfigureAndBuildTests(clobber=False):
  for arch in ARCHES:
    workpath = os.path.join(TOOLCHAIN_BUILD_OUT,
                            ReplaceArch('bionic_$GCC_test_work', arch))
    inspath = os.path.join(TOOLCHAIN_BUILD_OUT,
                           ReplaceArch('bionic_$GCC_test_install', arch))
    if clobber:
      print 'Clobbering'
      Rmdir(workpath)
      Rmdir(inspath)
    Mkdir(workpath)
    Mkdir(inspath)
    ConfigureAndBuild(arch, 'bionic/tests', workpath, inspath)


def ArchiveAndUpload(version, zipname, zippath):
  if 'BUILDBOT_BUILDERNAME' in os.environ:
    GSUTIL = '../buildbot/gsutil.sh'
  else:
    GSUTIL = 'gsutil'
  GSUTIL_ARGS = [GSUTIL, 'cp', '-a', 'public-read']
  GSUTIL_PATH = 'gs://nativeclient-archive2/toolchain'

  urldir = os.path.join(GSUTIL_PATH, version)
  zipurl = os.path.join(urldir, zipname)
  zipname = os.path.join(TOOLCHAIN_BUILD_OUT, zipname)

  try:
    os.remove(zipname)
  except:
    pass

  sys.stdout.flush()
  print >>sys.stderr, '@@@STEP_LINK@download@%s@@@' % urldir

  if process.Run(['tar', '-czf', zipname, zippath],
                 cwd=TOOLCHAIN,
                 outfile=sys.stdout):
      raise RuntimeError('Failed to zip %s from %s.\n' % (zipname, zippath))

  hashzipname = zipname + '.sha1hash'
  hashzipurl = zipurl + '.sha1hash'
  hashval = hashing_tools.HashFileContents(zipname)

  with open(hashzipname, 'w') as f:
    f.write(hashval)

  if process.Run(GSUTIL_ARGS + [zipname, zipurl],
                 cwd=TOOLCHAIN_BUILD,
                 outfile=sys.stdout):
    err = 'Failed to upload zip %s to %s.\n' % (zipname, zipurl)
    raise RuntimeError(err)

  if process.Run(GSUTIL_ARGS + [hashzipname, hashzipurl],
                 cwd=TOOLCHAIN_BUILD,
                 outfile=sys.stdout):
    err = 'Failed to upload hash %s to %s.\n' % (hashzipname, hashzipurl)
    raise RuntimeError(err)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option(
      '-v', '--verbose', dest='verbose',
      default=False, action='store_true',
      help='Produce more output.')
  parser.add_option(
      '-c', '--clobber', dest='clobber',
      default=False, action='store_true',
      help='Clobber working directories before building.')
  parser.add_option(
      '-s', '--sync', dest='sync',
      default=False, action='store_true',
      help='Sync sources first.')

  parser.add_option(
      '-b', '--buildbot', dest='buildbot',
      default=False, action='store_true',
      help='Running on the buildbot.')

  parser.add_option(
      '-e', '--empty', dest='empty',
      default=False, action='store_true',
      help='Only create empty toolchain')

  parser.add_option(
      '-u', '--upload', dest='upload',
      default=False, action='store_true',
      help='Upload build artifacts.')

  parser.add_option(
      '--skip-gcc', dest='skip_gcc',
      default=False, action='store_true',
      help='Skip building GCC components.')

  options, args = parser.parse_args(argv)
  if options.buildbot or options.upload:
    version = os.environ['BUILDBOT_REVISION']

  if options.clobber or options.empty:
    Clobber()

  if options.sync or options.buildbot:
    FetchBionicSources()

  # Copy headers, create libc.a
  CreateBasicToolchain()

  # Create libgcc.a libgcc_s
  if not options.empty and not options.skip_gcc:
    FetchAndBuildGCC()
    ConfigureAndBuildGCC(options.clobber)

  # Copy headers, create libc.so, libm.a
  ConfigureAndBuildBionic()

  # Create libc++.a, libc++.so
  if not options.empty and not options.skip_gcc:
    ConfigureAndBuildCXX(options.clobber)

  # Build the dynamic linker
  ConfigureAndBuildLinker()

  # Can not test on buildot until sel_ldr, irt, etc.. are built
  if not options.buildbot:
    ConfigureAndBuildTests(clobber=False)

  if options.buildbot or options.upload:
    zipname = 'naclsdk_linux_arm_bionic.tgz'
    ArchiveAndUpload(version, zipname, 'linux_arm_bionic')


if __name__ == '__main__':
  sys.exit(main(sys.argv))