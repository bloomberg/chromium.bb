#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sets environment variables needed to run a chromium unit test."""

import os
import stat
import subprocess
import sys

# This is hardcoded to be src/ relative to this script.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def should_enable_sandbox(cmd, sandbox_path):
  """Return a boolean indicating that the current slave is capable of using the
  sandbox and should enable it.  This should return True iff the slave is a
  Linux host with the sandbox file present and configured correctly."""
  if not (sys.platform.startswith('linux') and
          os.path.exists(sandbox_path)):
    return False

  # Copy the check in tools/build/scripts/slave/runtest.py.
  if '--lsan=1' in cmd:
    return False

  sandbox_stat = os.stat(sandbox_path)
  if ((sandbox_stat.st_mode & stat.S_ISUID) and
      (sandbox_stat.st_mode & stat.S_IRUSR) and
      (sandbox_stat.st_mode & stat.S_IXUSR) and
      (sandbox_stat.st_uid == 0)):
    return True
  return False


def get_sandbox_env(cmd, env, verbose=False):
  """Checks enables the sandbox if it is required, otherwise it disables it.
  Returns the environment flags to set."""
  extra_env = {}
  chrome_sandbox_path = env.get(CHROME_SANDBOX_ENV, CHROME_SANDBOX_PATH)

  if should_enable_sandbox(cmd, chrome_sandbox_path):
    if verbose:
      print 'Enabling sandbox. Setting environment variable:'
      print '  %s="%s"' % (CHROME_SANDBOX_ENV, chrome_sandbox_path)
    extra_env[CHROME_SANDBOX_ENV] = chrome_sandbox_path
  else:
    if verbose:
      print 'Disabling sandbox.  Setting environment variable:'
      print '  CHROME_DEVEL_SANDBOX=""'
    extra_env['CHROME_DEVEL_SANDBOX'] = ''

  return extra_env


def trim_cmd(cmd):
  """Removes internal flags from cmd since they're just used to communicate from
  the host machine to this script running on the swarm slaves."""
  internal_flags = frozenset(['--asan=0', '--asan=1', '--lsan=0', '--lsan=1'])
  return [i for i in cmd if i not in internal_flags]


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def get_asan_env(cmd, lsan):
  """Returns the envirnoment flags needed for ASan and LSan."""

  extra_env = {}

  # Instruct GTK to use malloc while running ASan or LSan tests.
  extra_env['G_SLICE'] = 'always-malloc'

  extra_env['NSS_DISABLE_ARENA_FREE_LIST'] = '1'
  extra_env['NSS_DISABLE_UNLOAD'] = '1'

  # TODO(glider): remove the symbolizer path once
  # https://code.google.com/p/address-sanitizer/issues/detail?id=134 is fixed.
  symbolizer_path = os.path.abspath(os.path.join(ROOT_DIR, 'third_party',
      'llvm-build', 'Release+Asserts', 'bin', 'llvm-symbolizer'))

  asan_options = []
  if lsan:
    asan_options.append('detect_leaks=1')
    if sys.platform == 'linux2':
      # Use the debug version of libstdc++ under LSan. If we don't, there will
      # be a lot of incomplete stack traces in the reports.
      extra_env['LD_LIBRARY_PATH'] = '/usr/lib/x86_64-linux-gnu/debug:'

    # LSan is not sandbox-compatible, so we can use online symbolization. In
    # fact, it needs symbolization to be able to apply suppressions.
    symbolization_options = ['symbolize=1',
                             'external_symbolizer_path=%s' % symbolizer_path]

    suppressions_file = os.path.join(ROOT_DIR, 'tools', 'lsan',
        'suppressions.txt')
    lsan_options = ['suppressions=%s' % suppressions_file,
                    'print_suppressions=1']
    extra_env['LSAN_OPTIONS'] = ' '.join(lsan_options)
  else:
    # ASan uses a script for offline symbolization.
    # Important note: when running ASan with leak detection enabled, we must use
    # the LSan symbolization options above.
    symbolization_options = ['symbolize=0']
    # Set the path to llvm-symbolizer to be used by asan_symbolize.py
    extra_env['LLVM_SYMBOLIZER_PATH'] = symbolizer_path

  asan_options.extend(symbolization_options)

  extra_env['ASAN_OPTIONS'] = ' '.join(asan_options)

  if sys.platform == 'darwin':
    isolate_output_dir = os.path.abspath(os.path.dirname(cmd[0]))
    # This is needed because the test binary has @executable_path embedded in it
    # it that the OS tries to resolve to the cache directory and not the mapped
    #  directory.
    extra_env['DYLD_LIBRARY_PATH'] = str(isolate_output_dir)

  return extra_env


def run_executable(cmd, env):
  """Runs an executable with:
    - environment variable CR_SOURCE_ROOT set to the root directory.
    - environment variable LANGUAGE to en_US.UTF-8.
    - environment variable CHROME_DEVEL_SANDBOX set if need
    - Reuses sys.executable automatically.
  """
  extra_env = {}
  # Many tests assume a English interface...
  extra_env['LANG'] = 'en_US.UTF-8'
  # Used by base/base_paths_linux.cc as an override. Just make sure the default
  # logic is used.
  env.pop('CR_SOURCE_ROOT', None)
  extra_env.update(get_sandbox_env(cmd, env))

  # Copy logic from  tools/build/scripts/slave/runtest.py.
  asan = '--asan=1' in cmd
  lsan = '--lsan=1' in cmd

  if asan:
    extra_env.update(get_asan_env(cmd, lsan))
  if lsan:
    cmd.append('--no-sandbox')

  cmd = trim_cmd(cmd)

  # Ensure paths are correctly separated on windows.
  cmd[0] = cmd[0].replace('/', os.path.sep)
  cmd = fix_python_path(cmd)

  print('Additional test environment:\n%s\n'
        'Command: %s\n' % (
        '\n'.join('    %s=%s' %
            (k, v) for k, v in sorted(extra_env.iteritems())),
        ' '.join(cmd)))
  env.update(extra_env or {})
  try:
    # See above comment regarding offline symbolization.
    if asan and not lsan:
      # Need to pipe to the symbolizer script.
      p1 = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE,
                            stderr=sys.stdout)
      p2 = subprocess.Popen(["../tools/valgrind/asan/asan_symbolize.py"],
                            stdin=p1.stdout)
      p1.stdout.close()  # Allow p1 to receive a SIGPIPE if p2 exits.
      p2.wait()
      return p2.returncode
    else:
      return subprocess.call(cmd, env=env)
  except OSError:
    print >> sys.stderr, 'Failed to start %s' % cmd
    raise


def main():
  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == '__main__':
  sys.exit(main())
