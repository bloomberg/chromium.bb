#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Installs and runs (a subset of) the gcc toolchain test suite against
various nacl and non-nacl toolchains
'''

import glob
import os
import os.path
import sys

# Hack to get buildbot_lib. Fix by moving scripts around?
sys.path.append(os.path.abspath(os.path.join(os.getcwd(),'buildbot')))
import buildbot_lib


# Config
TEST_PATH_C = 'pnacl/git/gcc/gcc/testsuite/gcc.c-torture/execute'
TEST_PATH_CPP = 'pnacl/git/gcc/gcc/testsuite/g++.dg'

def usage():
  print 'Usage:', sys.argv[0], '<compiler> <platform>',
  print '[<args for toolchain_tester.py>]'

def prereqs_scons(context, platform):
  if not platform in ('x86-32', 'x86-64', 'arm'):
    raise ValueError('Bad platform,', platform)
  return buildbot_lib.SCons(context, mode=('opt-host', 'nacl'),
                            platform=platform, parallel=True,
                            args=('irt_core', 'run_intrinsics_test'))

def prereqs_pnacl(context):
  return buildbot_lib.Command(context, ('pnacl/build.sh', 'sdk', 'newlib'))

def standard_tests(context, config, exclude, extra_args):
  # TODO: make toolchain_tester.py runnable as a library?
  command = ['tools/toolchain_tester/toolchain_tester.py',
             '--exclude=tools/toolchain_tester/' + exclude,
             '--exclude=tools/toolchain_tester/known_failures_base.txt',
             '--config=' + config,
             '--append=CFLAGS:-std=gnu89']
  if 'pnacl' in config:
    command.append('--append_file=tools/toolchain_tester/extra_flags_pnacl.txt')
  command.extend(extra_args)
  command.extend(glob.glob(os.path.join(TEST_PATH_C, '*c')))
  command.extend(glob.glob(os.path.join(TEST_PATH_C, 'ieee', '*c')))
  print command
  try:
    return buildbot_lib.Command(context, command)
  except buildbot_lib.StepFailed:
    return 1

def eh_tests(context, config, exclude, extra_args):
  # TODO: toolchain_tester.py runnable as a library?
  command = ['tools/toolchain_tester/toolchain_tester.py',
             '--exclude=tools/toolchain_tester/' + exclude,
             '--exclude=tools/toolchain_tester/unsuitable_dejagnu_tests.txt',
             '--config=' + config]
  if 'pnacl' in config:
    command.append('--append=CFLAGS:--pnacl-allow-exceptions')
    command.append('--append=FINALIZE_FLAGS:--no-finalize')
    command.append('--append=TRANSLATE_FLAGS:--pnacl-allow-exceptions')
    command.append('--append=TRANSLATE_FLAGS:--allow-llvm-bitcode-input')
    command.append('--append_file=tools/toolchain_tester/extra_flags_pnacl.txt')
  command.extend(extra_args)
  command.extend(glob.glob(os.path.join(TEST_PATH_CPP, 'eh', '*.C')))
  print command
  try:
    return buildbot_lib.Command(context, command)
  except buildbot_lib.StepFailed:
    return 1

def run_torture(status, compiler, platform, extra_args):
  if platform not in ('x86-32', 'x86-64', 'arm'):
    print 'Unknown platform:', platform
  prereqs_scons(status.context, platform)
  if compiler == 'pnacl':
    prereqs_pnacl(status.context)

  config_map = { 'pnacl': 'llvm_pnacl',
                 'naclgcc': 'nacl_gcc',
                 'localgcc': 'local_gcc'}

  failures = []
  if compiler == 'pnacl':
    # O3_O0 is clang -O3 followed by pnacl-translate -O0
    optmodes = ['O0', 'O3', 'O0_O0', 'O3_O0']
  else:
    optmodes = ['O0', 'O3']
  for optmode in optmodes:
    # TODO: support an option like -k? For now, always keep going
    retcode = eh_tests(status.context,
                      '_'.join((config_map[compiler], platform, optmode)),
                      'known_eh_failures_' + compiler + '.txt', extra_args)
    if retcode:
      failures.append(optmode + ' eh')

    retcode = standard_tests(
        status.context,
        '_'.join((config_map[compiler], platform, optmode)),
        'known_failures_' + compiler + '.txt', extra_args)
    if retcode:
      failures.append(optmode + ' standard')
  if len(failures) > 0:
    print 'There were failed steps in modes:', failures
    return 1
  return 0

def main():
  context = buildbot_lib.BuildContext()
  buildbot_lib.SetDefaultContextAttributes(context)
  context['max_jobs'] = int(os.environ.get('PNACL_CONCURRENCY', 4))
  status = buildbot_lib.BuildStatus(context)

  # TODO(dschuff): it's a pain to pass through unknown arguments with optparse,
  # but if we add more, or once we have argparse (python2.7) everywhere, switch.
  try:
    compiler = sys.argv[1]
    platform = sys.argv[2]
    tester_argv = sys.argv[3:]
  except IndexError:
    usage()
    sys.exit(1)

  return run_torture(status, compiler, platform, tester_argv)

if __name__ == '__main__':
  sys.exit(main())
