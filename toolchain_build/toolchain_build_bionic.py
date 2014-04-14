#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import argparse
import fnmatch
import os
import process
import stat
import shutil
import StringIO
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.gsd_storage
import pynacl.hashing_tools
import pynacl.platform
import pynacl.repo_tools

BUILD_SCRIPT = os.path.abspath(__file__)
TOOLCHAIN_BUILD = os.path.dirname(BUILD_SCRIPT)
NATIVE_CLIENT = os.path.dirname(TOOLCHAIN_BUILD)
PKG_VERSION = os.path.join(NATIVE_CLIENT, 'build', 'package_version')
sys.path.append(PKG_VERSION)
import archive_info
import package_info

import toolchain_build
import toolchain_main

from file_update import Mkdir, Rmdir, Symlink
from file_update import NeedsUpdate, UpdateFromTo, UpdateText


BIONIC_VERSION = 'dfb812021017bceb2593657669dcdc6a902a0b2e'
ARCHES = ['arm']
TOOLCHAIN_BUILD_SRC = os.path.join(TOOLCHAIN_BUILD, 'src')
TOOLCHAIN_BUILD_OUT = os.path.join(TOOLCHAIN_BUILD, 'out')

BIONIC_SRC = os.path.join(TOOLCHAIN_BUILD_SRC, 'bionic')
TOOLCHAIN = os.path.join(NATIVE_CLIENT, 'toolchain')

PROJECTS = [
  'bionic_%s_work',
  'gcc_%s_work',
]

def GetToolchainPath(target_arch, libc, *extra_paths):
  os_name = pynacl.platform.GetOS()
  host_arch = pynacl.platform.GetArch()
  return os.path.join(TOOLCHAIN,
                      '%s_%s' % (os_name, host_arch),
                      'nacl_%s_%s' % (target_arch, libc),
                      *extra_paths)


def GetBionicBuildPath(target_arch, *extra_paths):
  os_name = pynacl.platform.GetOS()
  return os.path.join(TOOLCHAIN_BUILD_OUT,
                      "%s_%s_bionic" % (os_name, target_arch),
                      *extra_paths)


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
    Rmdir(GetToolchainBuildPath(arch, 'bionic'))
    for workdir in PROJECTS:
      Rmdir(os.path.join(TOOLCHAIN_BUILD_OUT, workdir % arch))


def FetchAndBuild_gcc_libs():
  tc_args = ['-y', '--no-use-remote-cache', 'gcc_libs_arm']
  # Call toolchain_build to build the gcc libs. We do not need to fill in
  # any package targets since we are using toolchain_build as an
  # intermediate step.
  toolchain_main.PackageBuilder(toolchain_build.PACKAGES, {}, tc_args).Main()


def FetchBionicSources():
  project = 'bionic'
  url = '%s/nacl-%s.git' % (toolchain_build.GIT_BASE_URL, project)
  pynacl.repo_tools.SyncGitRepo(url,
                                os.path.join(TOOLCHAIN_BUILD_SRC, project),
                                BIONIC_VERSION)


def MungeIRT(src, dst):
  replace_map = {
    'off_t': 'int64_t',
    'native_client/src/untrusted/irt/' : '',
  }

  with open(src, 'r') as srcf:
    text = srcf.read()
    text = ReplaceText(text, [replace_map])
    with open(dst, 'w') as dstf:
      dstf.write(text)


def CreateBasicToolchain():
  # Create a toolchain directory containing only the toolchain binaries and
  # basic files line nacl_arm_macros.s.
  arch = 'arm'
  UpdateFromTo(GetToolchainPath(arch, 'newlib'),
               GetBionicBuildPath(arch),
               filters=['*arm-nacl/include*', '*arm-nacl/lib*','*.a', '*.o'])
  UpdateFromTo(GetToolchainPath(arch, 'newlib'),
               GetBionicBuildPath(arch),
               paterns=['*.s'])

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
    ('bionic/libc/arch-nacl/syscalls/irt_poll.h',
        '$NACL-nacl/include/irt_poll.h'),
    ('bionic/libc/arch-nacl/syscalls/irt_socket.h',
        '$NACL-nacl/include/irt_socket.h'),
    ('bionic/libc/include', '$NACL-nacl/include'),
    ('bionic/libc/arch-nacl/syscalls/nacl_socket.h',
        '$NACL-nacl/include/nacl_socket.h'),
    ('bionic/libc/arch-nacl/syscalls/nacl_stat.h',
        '$NACL-nacl/include/nacl_stat.h'),
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
    for name in ['irt.h', 'irt_dev.h']:
      src = os.path.join(NATIVE_CLIENT, 'src', 'untrusted', 'irt', name)
      dst = GetBionicBuildPath(arch, '$NACL-nacl', 'include', name)
      MungeIRT(src, ReplaceArch(dst, arch))

    inspath = GetBionicBuildPath(arch)

    # Create empty objects and libraries
    libpath = ReplaceArch(os.path.join(inspath, '$NACL-nacl', 'lib'), arch)
    for name in EMPTY_FILES:
      UpdateText(os.path.join(libpath, name), EMPTY)

    # Copy BIONIC files to toolchain
    for src, dst in BIONIC_PAIRS:
      srcpath = ReplaceArch(os.path.join(TOOLCHAIN_BUILD_SRC, src), arch)
      dstpath = ReplaceArch(os.path.join(inspath, dst), arch)
      UpdateFromTo(srcpath, dstpath)

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
      '-lgcc': '-lgcc --as-needed %{!static: -lgcc_s} --no-as-needed %{!shared: -lgcc_eh}',
      '--hash-style=gnu': '--hash-style=sysv',
    }])
    open(specs, 'w').write(text)


def ConfigureAndBuild_libc():
  for arch in ARCHES:
    inspath = GetBionicBuildPath(arch)
    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/libc', workpath, inspath, )


def ConfigureAndBuild_libc():
  for arch in ARCHES:
    inspath = GetBionicBuildPath(arch)
    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/libc', workpath, inspath)
    ConfigureAndBuild(arch, 'bionic/libm', workpath, inspath)


def ConfigureAndBuildLinker():
  for arch in ARCHES:
    inspath = GetBionicBuildPath(arch)
    workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
    workpath = ReplaceArch(workpath, arch)
    ConfigureAndBuild(arch, 'bionic/linker', workpath, inspath)


def ConfigureGCCProject(arch, project, cfg, workpath, inspath):
  # configure does not always have +x
  filepath = os.path.abspath(os.path.join(workpath, cfg[0]))
  st_info  = os.stat(filepath)
  os.chmod(filepath, st_info.st_mode | stat.S_IEXEC)

  env = os.environ
  newpath = GetBionicBuildPath(arch, 'bin')  + ':' + env['PATH']

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


def MakeGCCProject(arch, project, workpath, targets=[]):
  env = os.environ
  newpath = GetBionicBuildPath(arch, 'bin')  + ':' + env['PATH']
  proj = '%s %s' % (project, arch)
  setpath = ['/usr/bin/env', 'PATH=' + newpath]

  if targets:
    proj = project = ': ' + ' '.join(targets)
  else:
    proj = project

  print 'Make ' + proj
  if process.Run(setpath + ['make', '-j16', 'V=1'] + targets,
                  cwd=workpath, outfile=sys.stdout):
    raise RuntimeError('Failed to build %s.\n' % proj)
  print 'Done ' + proj


def ConfigureAndBuild_libgcc(config=False):
  arch = 'arm'
  project = 'libgcc'
  tcpath = GetBionicBuildPath(arch)

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
  ConfigureGCCProject(arch, project, cfg, dstpath, inspath)
  MakeGCCProject(arch, project, dstpath, ['libgcc.a'])

  # Copy temp version of libgcc.a for linking libc.so
  UpdateFromTo(os.path.join(dstpath, 'libgcc.a'),
               os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc.a'))


def BuildAndInstall_libgcc_s():
  arch = 'arm'
  project = 'libgcc'
  tcpath = GetBionicBuildPath(arch)

  # Remove temp copy of libgcc.a, it get's installed at the end
  os.remove(os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc.a'))

  # Prep work path
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_work')
  workpath = ReplaceArch(workpath, arch)
  dstpath = ReplaceArch(os.path.join(workpath, '$NACL-nacl/libgcc'), arch)

  # Prep install path
  inspath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_install')
  inspath = ReplaceArch(inspath, arch)

  MakeGCCProject(arch, project, dstpath)
  MakeGCCProject(arch, project, dstpath, ['install'])

  UpdateFromTo(os.path.join(inspath, 'lib', 'gcc'),
               os.path.join(tcpath, 'lib', 'gcc'),
               filters=['*.o'])
  UpdateFromTo(os.path.join(inspath, 'lib', 'libgcc_s.so.1'),
               os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc_s.so.1'))
  UpdateFromTo(os.path.join(inspath, 'lib', 'libgcc_s.so'),
               os.path.join(tcpath, 'arm-nacl', 'lib', 'libgcc_s.so'))


def ConfigureAndBuild_libstdcpp():
  arch = 'arm'
  project = 'libstdc++'
  tcpath = GetBionicBuildPath(arch)

  # Prep work path
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_work')
  workpath = ReplaceArch(workpath, arch)

  # Prep install path
  inspath = os.path.join(TOOLCHAIN_BUILD_OUT, 'gcc_$GCC_bionic_install')
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

  ConfigureGCCProject(arch, project, cfg, dstpath, inspath)
  MakeGCCProject(arch, project, dstpath)
  MakeGCCProject(arch, project, dstpath, ['install'])

  UpdateFromTo(os.path.join(inspath, 'lib'),
               os.path.join(tcpath, 'arm-nacl', 'lib'))
  UpdateFromTo(os.path.join(inspath, 'include'),
               os.path.join(tcpath, 'arm-nacl', 'include'))


def GetProjectPaths(arch, project):
  srcpath = os.path.join(BIONIC_SRC, project)
  workpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_work')
  instpath = os.path.join(TOOLCHAIN_BUILD_OUT, 'bionic_$ARCH_install')

  toolpath = GetBionicBuildPath(arch)
  workpath = ReplaceArch(os.path.join(workpath, 'bionic', project), arch)
  instpath = ReplaceArch(os.path.join(toolpath, '$NACL-nacl', 'lib'), arch)
  out = {
    'src': srcpath,
    'work': workpath,
    'ins': instpath,
    'tc': toolpath,
  }
  return out


def CreateProject(arch, project, clobber=False):
  paths = GetProjectPaths(arch, project)

  MAKEFILE_TEMPLATE = """
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GNU Makefile based on shared rules provided by the Native Client SDK.
# See README.Makefiles for more details.

TOOLCHAIN_PATH?=$(tc_path)
TOOLCHAIN_PREFIX:=$(TOOLCHAIN_PATH)/bin/$GCC-nacl-

CC:=$(TOOLCHAIN_PREFIX)gcc
CXX:=$(TOOLCHAIN_PREFIX)g++
AR:=$(TOOLCHAIN_PREFIX)ar

SRC_ROOT=$(src_path)
DST_ROOT=$(dst_path)
INS_ROOT=$(ins_path)

NACL_ARCH=$NACL
GCC_ARCH=$GCC

MAKEFILE_DEPS:=$(build_tc_path)/tc_bionic.mk
MAKEFILE_DEPS+=$(src_path)/Makefile

include $(build_tc_path)/tc_bionic.mk
include $(src_path)/Makefile
"""
  remap = {
    '$(src_path)': paths['src'],
    '$(dst_path)': paths['work'],
    '$(ins_path)': paths['ins'],
    '$(tc_path)':  GetBionicBuildPath(arch),
    '$(build_tc_path)': TOOLCHAIN_BUILD
  }
  text = ReplaceText(MAKEFILE_TEMPLATE, [remap])
  text = ReplaceArch(text, arch)

  if clobber:
    print 'Clobbering Bionic project directory: ' + paths['work']
    Rmdir(paths['work'])

  Mkdir(paths['work'])
  Mkdir(paths['ins'])
  UpdateText(os.path.join(paths['work'], 'Makefile'), text)


def ConfigureBionicProjects(clobber=False):
  PROJECTS = ['libc', 'libm', 'linker', 'tests']
  arch = 'arm'
  for project in PROJECTS:
    print 'Configure %s for %s.' % (project, arch)
    CreateProject(arch, project, clobber)


def MakeBionicProject(project, targets=[], clobber=False):
  arch = 'arm'
  paths = GetProjectPaths(arch, project)
  workpath = paths['work']
  inspath = paths['ins']
  targetlist = ' '.join(targets)

  print 'Building %s for %s at %s %s.' % (project, arch, workpath, targetlist)
  if clobber:
    args = ['make', '-j12', 'V=1', 'clean']
    if process.Run(args, cwd=workpath, outfile=sys.stdout):
      raise RuntimeError('Failed to clean %s for %s.\n' % (project, arch))

  args = ['make', '-j12', 'V=1'] + targets
  if process.Run(args, cwd=workpath, outfile=sys.stdout):
    raise RuntimeError('Failed to build %s for %s.\n' % (project, arch))

  print 'Done with %s for %s.\n' % (project, arch)


def ArchiveAndUpload(version, zipname, zippath, packages_file):
  sys.stdout.flush()
  print >>sys.stderr, '@@@BUILD_STEP archive_and_upload@@@'

  bucket_path = 'nativeclient-archive2/toolchain/%s' % version
  gsd_store = pynacl.gsd_storage.GSDStorage(bucket_path, [bucket_path])

  zipname = os.path.join(TOOLCHAIN_BUILD_OUT, zipname)
  try:
    os.remove(zipname)
  except:
    pass

  # Archive the zippath to the zipname.
  if process.Run(['tar', '-czf', zipname, zippath],
                 cwd=TOOLCHAIN_BUILD_OUT,
                 outfile=sys.stdout):
      raise RuntimeError('Failed to zip %s from %s.\n' % (zipname, zippath))

  # Create Zip Hash file using the hash of the zip file.
  hashzipname = zipname + '.sha1hash'
  hashval = pynacl.hashing_tools.HashFileContents(zipname)
  with open(hashzipname, 'w') as f:
    f.write(hashval)

  # Upload the Zip file.
  zipurl = gsd_store.PutFile(zipname, os.path.basename(zipname))
  sys.stdout.flush()
  print >>sys.stderr, ('@@@STEP_LINK@download (%s)@%s@@@' %
                       (os.path.basename(zipname), zipurl))

  # Upload the Zip Hash file.
  hashurl = gsd_store.PutFile(hashzipname, os.path.basename(hashzipname))
  sys.stdout.flush()
  print >>sys.stderr, ('@@@STEP_LINK@download (%s)@%s@@@' %
                       (os.path.basename(hashzipname), hashurl))

  # Create a package info file for the nacl_arm_bionic package.
  archive_desc = archive_info.ArchiveInfo(name=os.path.basename(zipname),
                                          archive_hash=hashval,
                                          url=zipurl)
  package_desc = package_info.PackageInfo()
  package_desc.AppendArchive(archive_desc)

  os_name = pynacl.platform.GetOS()
  arch_name = pynacl.platform.GetArch()
  package_info_file = os.path.join(TOOLCHAIN_BUILD_OUT,
                                   'packages',
                                   '%s_%s' % (os_name, arch_name),
                                   'nacl_arm_bionic.json')
  package_desc.SavePackageFile(package_info_file)

  # If packages_file is specified, write out our packages file of 1 package.
  if packages_file:
    with open(packages_file, 'wt') as f:
      f.write(package_info_file)


def main(argv):
  parser = argparse.ArgumentParser(add_help=False)
  parser.add_argument(
      '-v', '--verbose', dest='verbose',
      default=False, action='store_true',
      help='Produce more output.')
  parser.add_argument(
      '-c', '--clobber', dest='clobber',
      default=False, action='store_true',
      help='Clobber working directories before building.')
  parser.add_argument(
      '-s', '--sync', dest='sync',
      default=False, action='store_true',
      help='Sync sources first.')

  parser.add_argument(
      '-b', '--buildbot', dest='buildbot',
      default=False, action='store_true',
      help='Running on the buildbot.')

  parser.add_argument(
      '-u', '--upload', dest='upload',
      default=False, action='store_true',
      help='Upload build artifacts.')

  parser.add_argument(
        '--packages-file', dest='packages_file',
        default=None,
        help='Output packages file describing list of package files built.')

  parser.add_argument(
      '--skip-gcc', dest='skip_gcc',
      default=False, action='store_true',
      help='Skip building GCC components.')

  options, leftover_args = parser.parse_known_args()
  if '-h' in leftover_args or '--help' in leftover_args:
    print 'The following arguments are specific to toolchain_build_bionic.py:'
    parser.print_help()
    print 'The rest of the arguments are generic, in toolchain_main.py'

  if options.buildbot or options.upload:
    version = os.environ['BUILDBOT_REVISION']

  if options.clobber:
    Clobber()

  if options.sync or options.buildbot:
    FetchBionicSources()

  # Copy headers and compiler tools
  CreateBasicToolchain()

  # Configure Bionic Projects, libc, libm, linker, tests, ...
  ConfigureBionicProjects(clobber=options.buildbot)

  # Build and install IRT header before building GCC
  MakeBionicProject('libc', ['irt'])

  if not options.skip_gcc:
    # Build newlib gcc_libs for use by bionic
    FetchAndBuild_gcc_libs()

  # Configure and build libgcc.a
  ConfigureAndBuild_libgcc()

  # With libgcc.a, we can now build libc.so
  MakeBionicProject('libc')

  # With libc.so, we can build libgcc_s.so
  BuildAndInstall_libgcc_s()

  # With libc and libgcc_s, we can now build libm
  MakeBionicProject('libm')

  # With libc, libgcc, and libm, we can now build libstdc++
  ConfigureAndBuild_libstdcpp()

  # Now we can build the linker
  MakeBionicProject('linker')

  # Now we have a full toolchain, so test it
  MakeBionicProject('tests')

  # We can run only off buildbots
  if not options.buildbot:
    MakeBionicProject('tests', ['run'])

  if options.buildbot or options.upload:
    zipname = 'naclsdk_linux_arm_bionic.tgz'
    ArchiveAndUpload(version, zipname, 'linux_arm_bionic',
                     options.packages_file)


if __name__ == '__main__':
  sys.exit(main(sys.argv))