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
import shutil
import sys
import tempfile
import urllib

# Hack to get buildbot_lib. Fix by moving scripts around?
sys.path.append(os.path.abspath(os.path.join(os.getcwd(),'buildbot')))
import buildbot_lib


# Config
# This directory will usually persist on the buildbot between runs so we don't
# always have to download the testsuite.
TEST_ROOT = os.path.join(tempfile.gettempdir(), 'nacl_compiler_test')
TEST_TARBALL_URL = ('http://commondatastorage.googleapis.com/'
                    'nativeclient-mirror/nacl/gcc-testsuite-4.6.1.tar.bz2')
TEST_PATH_C = os.path.join(TEST_ROOT, 'gcc-4.6.1', 'gcc', 'testsuite',
                           'gcc.c-torture', 'execute')
TEST_PATH_CPP = os.path.join(TEST_ROOT, 'gcc-4.6.1', 'gcc', 'testsuite',
                             'g++.dg')

def print_banner(*args):
  print '.' * 80
  print ' '.join(args)
  print '.' * 80

def download_or_copy(url, filename):
  ''' Download or copy the remote file pointed to by 'url' to the location
      pointed to by 'filename'. 'url' can be either an HTTP url or a filename.
  '''
  if os.path.exists(filename):
    print filename, 'exists'
  elif url.startswith('http'):
    print_banner('downloading from', url)
    urllib.urlretrieve(url, filename)
  else:
    print_banner('copying from', url)
    shutil.copy(url, filename)

def install_tests(context):
  if not os.path.exists(TEST_ROOT):
    os.makedirs(TEST_ROOT)
  tarball = os.path.join(TEST_ROOT, 'test_tarball.tgz')
  download_or_copy(TEST_TARBALL_URL, tarball)
  return buildbot_lib.Command(context, ('tar', 'jxf', tarball, '-C', TEST_ROOT))

def usage():
  print 'Usage:', sys.argv[0], '<compiler> <platform>',
  print '[<args for toolchain_tester.py>]'
  print 'or:'
  print sys.argv[0], 'clean'
  print 'or:'
  print sys.argv[0], 'install-tests'

def clean():
  shutil.rmtree(TEST_ROOT)

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
    command.append('--append=TRANSLATE_FLAGS:--pnacl-allow-exceptions')
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
  for optmode in ['O0', 'O3']:
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

#TODO: move this to buildbot_lib and factor wrt ParseStandardCommandLine?
def setup_buildbot_context(context):
  context['platform'] = buildbot_lib.GetHostPlatform()
  context['mode'] = ('opt-host', 'nacl')
  context['clang'] = False
  context['asan'] = False
  context['use_glibc'] = False
  context['max_jobs'] = int(os.environ.get('PNACL_CONCURRENCY', 4))

def main():
  context = buildbot_lib.BuildContext()
  setup_buildbot_context(context)
  status = buildbot_lib.BuildStatus(context)

  # TODO(dschuff): it's a pain to pass through unknown arguments with optparse,
  # but if we add more, or once we have argparse (python2.7) everywhere, switch.
  try:
    if sys.argv[1] == 'clean':
      clean()
      sys.exit(0)
    if sys.argv[1] == 'install-tests':
      install_tests(context)
      sys.exit(0)
    compiler = sys.argv[1]
    platform = sys.argv[2]
    tester_argv = sys.argv[3:]
  except IndexError:
    usage()
    sys.exit(1)

  # --bot is passed by the bot script, which needs to install the tests
  if '--bot' in tester_argv:
    install_tests(context)
    tester_argv.remove('--bot')

  return run_torture(status, compiler, platform, tester_argv)

if __name__ == '__main__':
  sys.exit(main())
