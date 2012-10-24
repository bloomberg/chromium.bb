#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for NativeClient toolchain packages.

The real entry plumbing is in toolchain_main.py.
"""

import sys

import command
import toolchain_main


PACKAGE_NAME = 'Native Client SDK [%(build_signature)s]'
BUG_URL = 'http://gonacl.com/reportissue'

GIT_BASE_URL = 'http://git.chromium.org/native_client'

TAR_XV = ['tar', '-x', '-v']
EXTRACT_STRIP_TGZ = TAR_XV + ['--gzip', '--strip-components=1', '-f']
EXTRACT_STRIP_TBZ2 = TAR_XV + ['--bzip2', '--strip-components=1', '-f']
CONFIGURE_CMD = ['%(input0)s/configure']
MAKE_PARALLEL_CMD = ['make', '-j%(cores)s']
MAKE_DESTDIR_CMD = ['make', 'DESTDIR=%(output0)s']

if sys.platform.startswith('linux'):
  CONFIGURE_HOST_ARCH = [
      'CC=gcc -m32',
      'CXX=g++ -m32',
      ]
  ABI_OPTS = ['ABI=32']
else:
  CONFIGURE_HOST_ARCH = []
  ABI_OPTS = []

CONFIGURE_ARGS_COMMON = CONFIGURE_HOST_ARCH + [
    '--prefix=',
    '--with-pkgversion=' + PACKAGE_NAME,
    '--with-bugurl=' + BUG_URL,
    '--without-zlib',
]

CONFIGURE_ARGS_TARGET = [
    '--target=arm-nacl',
    '--with-sysroot=/arm-nacl',
]

def BuildCommand(cmd):
  return command.Command(cmd, cwd='build')

PACKAGES = {
    'binutils': {
        'git_url': GIT_BASE_URL + '/nacl-binutils.git',
        'git_revision': '0d99b0776661ebbf2949c644cb060612ccc11737',
        'commands': [
            command.Command(
                CONFIGURE_CMD +
                CONFIGURE_ARGS_COMMON + CONFIGURE_ARGS_TARGET + [
                    '--enable-gold',
                    '--enable-plugins',
                ]),
            command.Command(MAKE_PARALLEL_CMD),
            command.Command(MAKE_DESTDIR_CMD + ['install-strip']),
        ],
    },
    'gmp': {
        'tar_src': 'third_party/gmp/gmp-5.0.5.tar.bz2',
        'unpack_commands': [
            command.Mkdir('src'),
            command.Command(EXTRACT_STRIP_TBZ2 + ['%(input0)s'], cwd='src'),
        ],
        'hashed_inputs': ['src'],
        'commands': [
            command.Mkdir('build'),
            BuildCommand(
                CONFIGURE_CMD + CONFIGURE_ARGS_COMMON + ABI_OPTS + [
                    '--disable-shared',
                    '--enable-cxx',
                ]),
            BuildCommand(MAKE_PARALLEL_CMD),
            BuildCommand(MAKE_DESTDIR_CMD + ['install']),
        ],
    },
    'mpfr': {
        'dependencies': ['gmp'],
        'tar_src': 'third_party/mpfr/mpfr-3.1.1.tar.bz2',
        'unpack_commands': [
            command.Mkdir('src'),
            command.Command(EXTRACT_STRIP_TBZ2 + ['%(input0)s'], cwd='src'),
            command.Mkdir('all_deps'),
            command.Command('cp -r "%(input1)s/"* all_deps', shell=True),
        ],
        'hashed_inputs': ['src', 'all_deps'],
        'commands': [
            command.Mkdir('build'),
            BuildCommand(
                CONFIGURE_CMD + CONFIGURE_ARGS_COMMON + ABI_OPTS + [
                    '--disable-shared',
                    '--with-sysroot=%(cwd)s/../all_deps',
                    '--with-gmp=%(cwd)s/../all_deps',
                    '--enable-cxx',
                ]),
            BuildCommand(MAKE_PARALLEL_CMD),
            BuildCommand(MAKE_DESTDIR_CMD + ['install']),
        ],
    },
    'mpc': {
        'dependencies': ['gmp', 'mpfr'],
        'tar_src': 'third_party/mpc/mpc-1.0.tar.gz',
        'unpack_commands': [
            command.Mkdir('src'),
            command.Command(EXTRACT_STRIP_TGZ + ['%(input0)s'], cwd='src'),
            command.Mkdir('all_deps'),
            command.Command('cp -r "%(input1)s/"* all_deps', shell=True),
            command.Command('cp -r "%(input2)s/"* all_deps', shell=True),
        ],
        'hashed_inputs': ['src', 'all_deps'],
        'commands': [
            command.Mkdir('build'),
            BuildCommand(
                CONFIGURE_CMD + CONFIGURE_ARGS_COMMON + ABI_OPTS + [
                    '--disable-shared',
                    '--with-sysroot=%(cwd)s/../all_deps',
                    '--with-gmp=%(cwd)s/../all_deps',
                    '--with-mpfr=%(cwd)s/../all_deps',
                ]),
            BuildCommand(MAKE_PARALLEL_CMD),
            BuildCommand(MAKE_DESTDIR_CMD + ['install']),
        ],
    },
}


if __name__ == '__main__':
  tb = toolchain_main.PackageBuilder(PACKAGES, sys.argv[1:])
  tb.Main()
