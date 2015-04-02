#!/usr/bin/python
# Copyright (c) 2015 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipes for PNaCl sandboxed translator packages."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.gsd_storage
import pynacl.platform

import command

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)


def GSDJoin(*args):
  return '_'.join([pynacl.gsd_storage.LegalizeName(arg) for arg in args])


def SandboxedTranslators(arches):
  le32_packages = ['newlib_le32', 'libcxx_le32', 'libs_support_le32',
                   'core_sdk_libs_le32', 'metadata', 'compiler_rt_bc_le32']
  private_libs = ['libnacl_sys_private', 'libpthread_private', 'libplatform',
                  'libimc', 'libimc_syscalls', 'libsrpc', 'libgio']
  arch_packages = ['libs_support_translator', 'compiler_rt_translator']
  arch_deps = [GSDJoin(p, arch)
                   for p in arch_packages for arch in arches]


  def TranslatorLibDir(arch):
    return os.path.join('%(output)s', 'translator',
                        pynacl.platform.GetArch3264(arch), 'lib')
  translators = {
      # The translator build requires the PNaCl compiler, the le32 target libs,
      # the le32 core SDK libs, the native translator libs for the target arches
      # and the le32 private (non-IRT) libs. All except the last
      # are already built, so we copy those, and build the non-IRT libs here.
      'translator_compiler': {
        'type': 'work',
        'dependencies': ['target_lib_compiler'] + le32_packages + arch_deps,
        'inputs': {
            'src_untrusted': os.path.join(NACL_DIR, 'src', 'untrusted'),
            'src_include': os.path.join(NACL_DIR, 'src', 'include'),
            'scons.py': os.path.join(NACL_DIR, 'scons.py'),
            'site_scons': os.path.join(NACL_DIR, 'site_scons'),
        },
        'commands': [
          # Copy the le32 bitcode libs
          command.CopyRecursive('%(' + p + ')s', '%(output)s')
           for p in ['target_lib_compiler'] + le32_packages] + [
          # Build the non-IRT libs
          command.Command([sys.executable, '%(scons.py)s',
              '--verbose', 'bitcode=1', 'platform=x86-32',
              'pnacl_newlib_dir=%(output)s'] + private_libs,
              cwd=NACL_DIR)] + [
          command.Copy(
              os.path.join(NACL_DIR, 'scons-out',
                           'nacl-x86-32-pnacl-pexe-clang', 'lib', lib + '.a'),
              os.path.join('%(output)s', 'le32-nacl', 'lib', lib + '.a'))
           for lib in private_libs] + [
          # Copy the native translator libs
          command.CopyRecursive('%(' + GSDJoin(p, arch) + ')s',
                                TranslatorLibDir(arch))
           for p in arch_packages for arch in arches
          ],
      },
      'sandboxed_translators': {
          'type': 'build',
          'dependencies': ['translator_compiler'],
          'inputs': {
            'build': os.path.join(NACL_DIR, 'pnacl', 'build.sh'),
            '_': os.path.join(NACL_DIR, 'pnacl', 'scripts', 'common-tools.sh'),
          },
          'commands': [
              command.Command(['%(abs_build)s', 'translator-all']),
              command.Command(['%(abs_build)s', 'translator-prune']),
          ],
      },
  }
  return translators
