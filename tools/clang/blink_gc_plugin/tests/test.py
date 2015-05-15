#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import glob
import os
import subprocess
import sys


def run_test(test_base_name, cmd):
  """Run a test case.

  Args:
    test_base_name: The name for the test C++ source file without the extension.
    cmd: The actual command to run for the test.

  Returns:
    None on pass, or a str with the description of the failure.
  """
  try:
    actual = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    # Some of the Blink GC plugin tests intentionally trigger compile errors, so
    # just ignore an exit code that indicates failure.
    actual = e.output
  except Exception as e:
    return 'could not execute %s (%s)' % (cmd, e)

  # Some Blink GC plugins dump a JSON representation of the object graph, and
  # use the processed results as the actual results of the test.
  if os.path.exists('%s.graph.json' % test_base_name):
    try:
      actual = subprocess.check_output(
          ['python', '../process-graph.py', '-c',
           '%s.graph.json' % test_base_name],
          stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError, e:
      # The graph processing script returns a failure exit code if the graph is
      # 'bad' (e.g. it has a cycle). The output still needs to be captured in
      # that case, since the expected results capture the errors.
      actual = e.output
    finally:
      # Clean up the .graph.json file to prevent false passes from stale results
      # from a previous run.
      os.remove('%s.graph.json' % test_base_name)

  # TODO(dcheng): Remove the rstrip() and just rebaseline the tests to match.
  actual = actual.rstrip()

  # On Windows, clang emits CRLF as the end of line marker. Normalize it to LF
  # to match posix systems.
  actual = actual.replace('\r\n', '\n')

  try:
    expected = open('%s.txt' % test_base_name).read().rstrip()
  except IOError:
    open('%s.txt.actual' % test_base_name, 'w').write(actual)
    return 'no expected file found'

  if expected != actual:
    open('%s.txt.actual' % test_base_name, 'w').write(actual)
    return 'expected and actual differed'


def run_tests(clang_path, plugin_path):
  """Runs the tests.

  Args:
    clang_path: The path to the clang binary to be tested.
    plugin_path: An optional path to the plugin to test. This may be None, if
                 plugin is built directly into clang, like on Windows.

  Returns:
    (passing, failing): Two lists containing the base names of the passing and
                        failing tests respectively.
  """
  passing = []
  failing = []

  # The plugin option to dump the object graph is incompatible with
  # -fsyntax-only. It generates the .graph.json file based on the name of the
  # output file, but there is no output filename with -fsyntax-only.
  base_cmd = [clang_path, '-c', '-std=c++11']
  base_cmd.extend(['-Wno-inaccessible-base'])
  if plugin_path:
    base_cmd.extend(['-Xclang', '-load', '-Xclang', plugin_path])
  base_cmd.extend(['-Xclang', '-add-plugin', '-Xclang', 'blink-gc-plugin'])

  tests = glob.glob('*.cpp')
  for test in tests:
    sys.stdout.write('Testing %s... ' % test)
    test_base_name, _ = os.path.splitext(test)

    cmd = base_cmd[:]
    try:
      cmd.extend(file('%s.flags' % test_base_name).read().split())
    except IOError:
      pass
    cmd.append(test)

    failure_message = run_test(test_base_name, cmd)
    if failure_message:
      print 'failed: %s' % failure_message
      failing.append(test_base_name)
    else:
      print 'passed!'
      passing.append(test_base_name)

  return passing, failing


def main():
  if len(sys.argv) < 2:
    print 'Usage: <path to clang>[ <path to plugin>]'
    return -1

  os.chdir(os.path.dirname(os.path.realpath(__file__)))

  clang_path = sys.argv[1]
  plugin_path = sys.argv[2] if len(sys.argv) > 2 else None
  print 'Using clang %s...' % clang_path
  print 'Using plugin %s...' % plugin_path

  passing, failing = run_tests(clang_path, plugin_path)
  print 'Ran %d tests: %d succeeded, %d failed' % (
      len(passing) + len(failing), len(passing), len(failing))
  for test in failing:
    print '    %s' % test
  return len(failing)


if __name__ == '__main__':
  sys.exit(main())
