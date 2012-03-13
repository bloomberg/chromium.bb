#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs strace on a test and processes the logs to extract the dependencies from
the source tree.

Automatically extracts directories where all the files are used to make the
dependencies list more compact.
"""

import os
import re
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))

IGNORED = (
  '/dev',
  '/etc',
  '/home',
  '/lib',
  '/proc',
  '/sys',
  '/tmp',
  '/usr',
  '/var',
)


def gen_trace(cmd, cwd, logname, silent):
  """Runs strace on an executable."""
  strace = ['strace', '-f', '-e', 'trace=open', '-o', logname]
  stdout = stderr = None
  if silent:
    stdout = subprocess.PIPE
    stderr = subprocess.PIPE

  cmd = [os.path.normpath(os.path.join(cwd, c)) for c in cmd]
  p = subprocess.Popen(
      strace + cmd, cwd=cwd, stdout=stdout, stderr=stderr)
  out, err = p.communicate()
  if p.returncode != 0:
    print 'Failure: %d' % p.returncode
    # pylint: disable=E1103
    print ''.join(out.splitlines(True)[-100:])
    print ''.join(err.splitlines(True)[-100:])
  return p.returncode


def parse_log(filename, blacklist):
  """Processes a strace log and returns the files opened and the files that do
  not exist.

  Most of the time, files that do not exist are temporary test files that should
  be put in /tmp instead. See http://crbug.com/116251

  TODO(maruel): Process chdir() calls so relative paths can be processed.
  """
  files = set()
  non_existent = set()
  for line in open(filename):
    # 1=pid, 2=filepath, 3=mode, 4=result
    m = re.match(r'^(\d+)\s+open\("([^"]+)", ([^\)]+)\)\s+= (.+)$', line)
    if not m:
      continue
    if m.group(4).startswith('-1') or 'O_DIRECTORY' in m.group(3):
      # Not present or a directory.
      continue
    filepath = m.group(2)
    if blacklist(filepath):
      continue
    if not os.path.isfile(filepath):
      non_existent.add(filepath)
    else:
      files.add(filepath)
  return files, non_existent


def relevant_files(files, root):
  """Trims the list of files to keep the expected files and unexpected files.

  Unexpected files are files that are not based inside the |root| directory.
  """
  expected = []
  unexpected = []
  for f in files:
    if f.startswith(root):
      expected.append(f[len(root):])
    else:
      unexpected.append(f)
  return sorted(set(expected)), sorted(set(unexpected))


def extract_directories(files, root):
  """Detects if all the files in a directory were loaded and if so, replace the
  individual files by the directory entry.
  """
  directories = set(os.path.dirname(f) for f in files)
  files = set(files)
  for directory in sorted(directories, reverse=True):
    actual = set(
      os.path.join(directory, f) for f in
      os.listdir(os.path.join(root, directory))
      if not f.endswith(('.svn', '.pyc'))
    )
    if not (actual - files):
      files -= actual
      files.add(directory + '/')
  return sorted(files)


def strace_inputs(unittest, cmd):
  """Tries to load the logs if available. If not, strace the test."""
  logname = os.path.join(BASE_DIR, os.path.basename(unittest))
  if not os.path.isfile(logname):
    returncode = gen_trace(cmd, ROOT_DIR, logname, True)
    if returncode:
      return returncode

  def blacklist(f):
    """Strips ignored paths."""
    return f.startswith(IGNORED) or f.endswith('.pyc')

  files, non_existent = parse_log(logname, blacklist)
  print('Total: %d' % len(files))
  print('Non existent: %d' % len(non_existent))
  for f in non_existent:
    print('  %s' % f)

  expected, unexpected = relevant_files(files, ROOT_DIR + '/')
  if unexpected:
    print('Unexpected: %d' % len(unexpected))
    for f in unexpected:
      print('  %s' % f)

  simplified = extract_directories(expected, ROOT_DIR)
  print('Interesting: %d reduced to %d' % (len(expected), len(simplified)))
  for f in simplified:
    print('  %s' % f)

  return 0


def main():
  if len(sys.argv) < 3:
    print >> sys.stderr, (
        'Usage: strace_inputs.py [testname] [cmd line...]\n'
        '\n'
        'Example:\n'
        '  ./strace_inputs.py base_unittests testing/xvfb.py out/Release '
        'out/Release/base_unittests')
    return 1
  return strace_inputs(sys.argv[1], sys.argv[2:])


if __name__ == '__main__':
  sys.exit(main())
