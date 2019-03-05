#!/usr/bin/python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates config files for building dav1d."""

from __future__ import print_function

import os
import re
import shutil
import subprocess
import sys
import tempfile

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_ROOT_DIR = os.path.abspath(os.path.join(BASE_DIR, '..', '..'))

sys.path.append(os.path.join(CHROMIUM_ROOT_DIR, 'build'))
import gn_helpers

MESON = ['meson.py']

DEFAULT_BUILD_ARGS = [
    '-Dbuild_tools=false', '-Dbuild_tests=false', '--buildtype', 'release'
]

WINDOWS_BUILD_ARGS = ['-Dc_winlibs=']


def PrintAndCheckCall(argv, *args, **kwargs):
  print('\n-------------------------------------------------\nRunning %s' %
        ' '.join(argv))
  c = subprocess.check_call(argv, *args, **kwargs)


def RewriteFile(path, search_replace):
  with open(path) as f:
    contents = f.read()
  with open(path, 'w') as f:
    for search, replace in search_replace:
      contents = re.sub(search, replace, contents)

    # Cleanup trailing newlines.
    f.write(contents.strip() + '\n')


def SetupWindowsCrossCompileToolchain(target_arch):
  # First retrieve various MSVC and Windows SDK paths.
  output = subprocess.check_output([
      os.path.join(CHROMIUM_ROOT_DIR, 'build', 'vs_toolchain.py'),
      'get_toolchain_dir'
  ])

  # Turn this into a dictionary.
  win_dirs = gn_helpers.FromGNArgs(output)

  # Use those paths with a second script which will tell us the proper include
  # and lib paths to specify for cflags and ldflags respectively.
  output = subprocess.check_output([
      'python',
      os.path.join(CHROMIUM_ROOT_DIR, 'build', 'toolchain', 'win',
                   'setup_toolchain.py'), win_dirs['vs_path'],
      win_dirs['sdk_path'], win_dirs['runtime_dirs'], 'win', target_arch, 'none'
  ])

  flags = gn_helpers.FromGNArgs(output)
  cwd = os.getcwd()

  target_env = os.environ

  include_paths = []
  for cflag in flags['include_flags_imsvc'].split(' '):
    # Apparently setup_toolchain prefers relative include paths, which
    # may work for chrome, but it does not work for ffmpeg, so let's make
    # them asbolute again.
    include_path = cflag.strip('"')
    if include_path.startswith('-imsvc'):
      include_path = os.path.abspath(os.path.join(cwd, include_path[6:]))
    include_paths.append(include_path)

  # TODO(dalecurtis): Why isn't the ucrt path printed?
  flags['vc_lib_ucrt_path'] = flags['vc_lib_um_path'].replace('/um/', '/ucrt/')

  # Unlike the cflags, the lib include paths are each in a separate variable.
  lib_paths = []
  for k in flags:
    # libpath_flags is like cflags. Since it is also redundant, skip it.
    if 'lib' in k and k != 'libpath_flags':
      lib_paths.append(flags[k])

  target_env = os.environ
  target_env['INCLUDE'] = ';'.join(include_paths)
  target_env['LIB'] = ';'.join(lib_paths)
  return target_env


def CopyConfigsAndCleanup(config_dir, dest_dir):
  if not os.path.exists(dest_dir):
    os.makedirs(dest_dir)

  shutil.copy(os.path.join(config_dir, 'config.h'), dest_dir)

  # The .asm file will not be present for all configurations.
  asm_file = os.path.join(config_dir, 'config.asm')
  if os.path.exists(asm_file):
    shutil.copy(asm_file, dest_dir)

  shutil.rmtree(config_dir)


def GenerateConfig(config_dir, env, special_args=[]):
  temp_dir = tempfile.mkdtemp()
  PrintAndCheckCall(
      MESON + DEFAULT_BUILD_ARGS + special_args + [temp_dir],
      cwd='libdav1d',
      env=env)

  # We don't want non-visible log strings polluting the official binary.
  RewriteFile(
      os.path.join(temp_dir, 'config.h'),
      [(r'(#define CONFIG_LOG .*)',
        r'// \1 -- Logging is controlled by Chromium')])

  # Clang LTO doesn't respect stack alignment, so we must use the platform's
  # default stack alignment in that case; https://crbug.com/928743.
  RewriteFile(
      os.path.join(temp_dir, 'config.h'),
      [(r'(#define STACK_ALIGNMENT \d{1,2})',
        r'// \1 -- Stack alignment is controlled by Chromium')])
  if (os.path.exists(os.path.join(config_dir, 'config.asm'))):
    RewriteFile(
        os.path.join(temp_dir, 'config.asm'),
        [(r'(%define STACK_ALIGNMENT \d{1,2})',
          r'; \1 -- Stack alignment is controlled by Chromium')])

  CopyConfigsAndCleanup(temp_dir, config_dir)


def main():
  linux_env = os.environ
  linux_env['CC'] = 'clang'

  GenerateConfig('config/linux/x64', linux_env)
  GenerateConfig('config/linux-noasm/x64', linux_env, ['-Dbuild_asm=false'])

  GenerateConfig('config/linux/x86', linux_env,
                 ['--cross-file', '../crossfiles/linux32.crossfile'])
  GenerateConfig('config/linux/arm', linux_env,
                 ['--cross-file', '../crossfiles/arm.crossfile'])
  GenerateConfig('config/linux/arm64', linux_env,
                 ['--cross-file', '../crossfiles/arm64.crossfile'])

  win_x86_env = SetupWindowsCrossCompileToolchain('x86')
  GenerateConfig(
      'config/win/x86', win_x86_env,
      ['--cross-file', '../crossfiles/win32.crossfile'] + WINDOWS_BUILD_ARGS)

  win_x64_env = SetupWindowsCrossCompileToolchain('x64')
  GenerateConfig(
      'config/win/x64', win_x64_env,
      ['--cross-file', '../crossfiles/win64.crossfile'] + WINDOWS_BUILD_ARGS)


if __name__ == '__main__':
  main()
