# Copyright (C) 2021 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import os


def name():
  return 'blpwtk2'


def build_targets():
  return ['blpwtk2_all']


def gn_defines(version):
  return {
    'bb_generate_map_files': True,
    'bb_version': version,
    'is_official_build': True,
  }


def _get_bin_files(version):
  """Return a list of files to copy to the bin/<config> directory."""

  files = [
    'blpwtk2_shell.exe',
    'content_shell.exe',
    'content_shell.pak',
    'd3dcompiler_47.dll',
    'd8.exe',
    f'v8_context_snapshot.{version}.bin',
    f'blpwtk2.{version}.pak',
    f'icudtl.{version}.dat',
  ]

  modules = [
    f'blpcr_egl.{version}.dll',
    f'blpcr_glesv2.{version}.dll',
    f'blpwtk2.{version}.dll',
    f'blpwtk2_subprocess.{version}.exe',
    f'swiftshader/blpcr_egl.{version}.dll',
    f'swiftshader/blpcr_glesv2.{version}.dll',
  ]

  for m in modules:
    files += [ m, m + '.pdb', m[:-3] + 'map' ]

  return files


def _get_include_files(src_dir):
  """Return a list of files to copy to the include directory."""

  for dirpath, _, files in os.walk(src_dir):
    relpath = os.path.relpath(dirpath, src_dir)
    for f in files:
      if not f.endswith('.h'):
        continue
      yield os.path.join(relpath, f)


def _get_bin_file_copies(src_dir, dest_dir, version):
  return [(os.path.join(src_dir, x), os.path.join('bin', dest_dir, x))
          for x in _get_bin_files(version)]


def _get_include_file_copies(src_dir, dest_dir):
  return [(os.path.join(src_dir, x), os.path.join('include', dest_dir, x))
          for x in _get_include_files(src_dir)]


def get_file_copies(chromium_dir, x86, x64, version):
  """Return a list of files to copy.

  Each entry in the list is a tuple of two strings.  The first string is the
  source file, the second string is the path in the devkit where the source
  file should be copied.
  """

  copies = []
  if x86:
    out_dir = os.path.join(chromium_dir, 'src/out/static_release')
    copies += _get_bin_file_copies(out_dir, 'release', version)
    copies += _get_include_file_copies(
            os.path.join(out_dir, 'gen/blpwtk2/blpwtk2/public'), 'blpwtk2')
    copies.append((os.path.join(out_dir, f'blpwtk2.{version}.dll.lib'),
                   f'lib/release/blpwtk2.{version}.dll.lib'))

  if x64:
    out_dir = os.path.join(chromium_dir, 'src/out/static_release64')
    copies += _get_bin_file_copies(out_dir, 'release64', version)
    copies.append((os.path.join(out_dir, f'blpwtk2.{version}.dll.lib'),
                   f'lib/release64/blpwtk2.{version}.dll.lib'))

    if not x86:
      copies += _get_include_file_copies(
              os.path.join(out_dir, 'gen/blpwtk2/blpwtk2/public'), 'blpwtk2')

  copies += _get_include_file_copies(
          os.path.join(chromium_dir, 'src/blpwtk2/public'), 'blpwtk2')
  copies += _get_include_file_copies(
          os.path.join(chromium_dir, 'src/v8/include'), 'v8')

  return copies

