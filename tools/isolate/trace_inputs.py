#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs strace or dtrace on a test and processes the logs to extract the
dependencies from the source tree.

Automatically extracts directories where all the files are used to make the
dependencies list more compact.
"""

import logging
import optparse
import os
import re
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


def isEnabledFor(level):
  return logging.getLogger().isEnabledFor(level)


class Strace(object):
  """strace implies linux."""
  IGNORED = (
    '/dev',
    '/etc',
    '/lib',
    '/proc',
    '/sys',
    '/tmp',
    '/usr',
    '/var',
  )

  @staticmethod
  def gen_trace(cmd, cwd, logname):
    """Runs strace on an executable."""
    silent = not isEnabledFor(logging.INFO)
    stdout = stderr = None
    if silent:
      stdout = subprocess.PIPE
      stderr = subprocess.PIPE
    trace_cmd = ['strace', '-f', '-e', 'trace=open', '-o', logname]
    cmd = [os.path.normpath(os.path.join(cwd, c)) for c in cmd]
    p = subprocess.Popen(
        trace_cmd + cmd, cwd=cwd, stdout=stdout, stderr=stderr)
    out, err = p.communicate()
    if p.returncode != 0:
      print 'Failure: %d' % p.returncode
      # pylint: disable=E1103
      if out:
        print ''.join(out.splitlines(True)[-100:])
      if err:
        print ''.join(err.splitlines(True)[-100:])
    return p.returncode

  @staticmethod
  def parse_log(filename, blacklist):
    """Processes a strace log and returns the files opened and the files that do
    not exist.

    Most of the time, files that do not exist are temporary test files that
    should be put in /tmp instead. See http://crbug.com/116251

    TODO(maruel): Process chdir() calls so relative paths can be processed.
    """
    files = set()
    non_existent = set()
    # 1=pid, 2=filepath, 3=mode, 4=result
    RE = re.compile(
        # PID open(PATH, MODE) = RESULT
        r'^(\d+)\s+open\("([^"]+)", ([^\)]+)\)\s+= (.+)$')
    for line in open(filename):
      m = RE.match(line)
      if not m:
        continue
      if m.group(4).startswith('-1') or 'O_DIRECTORY' in m.group(3):
        # Not present or a directory.
        continue
      filepath = m.group(2)
      if blacklist(filepath):
        continue
      if filepath not in files and filepath not in non_existent:
        if os.path.isfile(filepath):
          files.add(filepath)
        else:
          non_existent.add(filepath)
    return files, non_existent


class Dtrace(object):
  """Uses DTrace framework through dtrace. Requires root access.

  Implies Mac OSX.

  dtruss can't be used because it has compatibility issues with python.
  """
  IGNORED = (
    '/.vol',
    '/Library',
    '/System',
    '/dev',
    '/etc',
    '/private/var',
    '/tmp',
    '/usr',
    '/var',
  )

  # pylint: disable=C0301
  # To understand the following code, you'll want to take a look at:
  # http://developers.sun.com/solaris/articles/dtrace_quickref/dtrace_quickref.html
  # and
  # https://wikis.oracle.com/display/DTrace/Variables
  D_CODE = """
      /* Child process tracking.
       * I'm really depressed that I need to do it myself. */
      dtrace:::BEGIN {
        trackedpid[ppid] = 1;
        trackedpid[pid] = 1;
        /* Signal gen_trace() that we are ready to trace. */
        printf("Go! 1 %d:%d %s", ppid, pid, execname);
      }

      /* Make sure all child processes are tracked. This is not very efficient
       * but for our use case, it's fine enough.
       * TODO(maruel): We should properly track fork, execve, vfork and friends
       * instead. */
      syscall:::entry /trackedpid[ppid]/ {
        trackedpid[pid] = 1;
      }
      syscall::exit:entry /trackedpid[pid]/ {
        trackedpid[pid] = 0;
      }

      /* Finally what we care about! */
      syscall::open*:entry /trackedpid[pid]/ {
        self->arg0 = copyinstr(arg0);
        self->arg1 = arg1;
        self->arg2 = arg2;
      }
      syscall::open*:return /trackedpid[pid]/ {
        printf("%d:%d \\"%s\\"; \\"%s\\"; \\"%d\\"; \\"%d\\" = %d",
              ppid, pid, execname, self->arg0, self->arg1, self->arg2, errno);
        self->arg0 = 0;
        self->arg1 = 0;
        self->arg2 = 0;
      }
      """

  @classmethod
  def gen_trace(cls, cmd, cwd, logname):
    """Runs dtrace on an executable."""
    silent = not isEnabledFor(logging.INFO)
    print 'Running: %s' % cmd
    signal = 'Go!'
    logging.debug('Our pid: %d' % os.getpid())

    # Part 1: start the child process.
    stdout = stderr = None
    if silent:
      stdout = subprocess.PIPE
      stderr = subprocess.PIPE
    child_cmd = [
      sys.executable, os.path.join(BASE_DIR, 'trace_child_process.py'),
    ]
    child = subprocess.Popen(
        child_cmd + cmd,
        stdin=subprocess.PIPE,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd)
    logging.debug('Started child pid: %d' % child.pid)

    # Part 2: start dtrace process.
    trace_cmd = [
      'sudo',
      'dtrace',
      '-x', 'dynvarsize=4m',
      '-x', 'evaltime=exec',
      '-n', cls.D_CODE,
      '-o', '/dev/stderr',
      '-p', str(child.pid),
    ]
    with open(logname, 'w') as logfile:
      dtrace = subprocess.Popen(
          trace_cmd, stdout=logfile, stderr=subprocess.STDOUT)
    logging.debug('Started dtrace pid: %d' % dtrace.pid)

    # Part 3: Read until the Go! signal is sent.
    with open(logname, 'r') as logfile:
      while True:
        x = logfile.readline()
        if signal in x:
          break

    # Part 4: We can now tell our child to go.
    child.communicate(signal)

    dtrace.wait()
    if dtrace.returncode != 0:
      print 'Failure: %d' % dtrace.returncode
      with open(logname) as logfile:
        print ''.join(logfile.readlines()[-100:])
      # Find a better way.
      os.remove(logname)
    return dtrace.returncode

  @staticmethod
  def parse_log(filename, blacklist):
    """Processes a dtrace log and returns the files opened and the files that do
    not exist.

    Most of the time, files that do not exist are temporary test files that
    should be put in /tmp instead. See http://crbug.com/116251

    TODO(maruel): Process chdir() calls so relative paths can be processed.
    """
    files = set()
    non_existent = set()
    ignored = set()
    # 1=filepath, 2=flags, 3=mode, 4=result
    RE = re.compile(
        # CPU ID PROBE PPID PID EXECNAME
        r'^\s+\d+\s+\d+\s+open[^\:]+:return \d+:\d+ \"[^\"]+\"; '
        # PATH FLAGS MODE RESULT
        r'\"([^\"]+)\"; \"([^\"]+)\"; \"([^\"]+)\" \= (.+)$')
    for line in open(filename):
      m = RE.match(line)
      if not m:
        continue
      if not m.group(4).startswith('0'):
        # Called failed.
        continue
      filepath = m.group(1)
      if blacklist(filepath):
        continue
      if (filepath not in files and
          filepath not in non_existent and
          filepath not in ignored):
        if os.path.isfile(filepath):
          files.add(filepath)
        elif not os.path.isdir(filepath):
          # Silently ignore directories.
          non_existent.add(filepath)
        else:
          ignored.add(filepath)
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


def trace_inputs(log, cmd, api, gyp_proj_dir, product_dir):
  """Tries to load the logs if available. If not, trace the test."""
  def print_if(txt):
    if not gyp_proj_dir:
      print(txt)

  logname = os.path.join(BASE_DIR, os.path.basename(log))
  if not os.path.isfile(logname):
    print_if('Tracing... %s' % cmd)
    returncode = api.gen_trace(cmd, ROOT_DIR, logname)
    if returncode:
      return returncode

  def blacklist(f):
    """Strips ignored paths."""
    return f.startswith(api.IGNORED) or f.endswith('.pyc')

  print_if('Loading traces... %s' % logname)
  files, non_existent = api.parse_log(logname, blacklist)

  print_if('Total: %d' % len(files))
  print_if('Non existent: %d' % len(non_existent))
  for f in non_existent:
    print_if('  %s' % f)

  expected, unexpected = relevant_files(files, ROOT_DIR + '/')
  if unexpected:
    print_if('Unexpected: %d' % len(unexpected))
    for f in unexpected:
      print_if('  %s' % f)

  simplified = extract_directories(expected, ROOT_DIR)
  print_if('Interesting: %d reduced to %d' % (len(expected), len(simplified)))
  for f in simplified:
    print_if('  %s' % f)

  if gyp_proj_dir:
    def fix(f):
      if f.startswith(product_dir):
        return '<(PRODUCT_DIR)%s' % f[len(product_dir):]
      elif f.startswith(gyp_proj_dir):
        return f[len(gyp_proj_dir)+1:]
      else:
        return '<(DEPTH)/%s' % f
    corrected = [fix(f) for f in simplified]
    files = [f for f in corrected if not f.endswith('/')]
    dirs = [f for f in corrected if f.endswith('/')]
    # Constructs the python code manually.
    print(
        '{\n'
        '  \'variables\': {\n'
        '    \'isolate_files\': [\n') + (
            ''.join('      \'%s\',\n' % f for f in files)) + (
        '    ],\n'
        '    \'isolate_dirs\': [\n') + (
            ''.join('      \'%s\',\n' % f for f in dirs)) + (
        '    ],\n'
        '  },\n'
        '},')
  return 0


def main():
  parser = optparse.OptionParser(
      usage='%prog <options> [cmd line...]')
  parser.allow_interspersed_args = False
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option('-l', '--log', help='Log file')
  parser.add_option(
      '-g', '--gyp',
      help='When specified, outputs the inputs files in a way compatible for '
           'gyp processing. Should be set to the relative path containing the '
           'gyp file, e.g. \'chrome\' or \'net\'')
  parser.add_option(
      '-p', '--product-dir', default='out/Release',
      help='Directory for PRODUCT_DIR')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
        level=level,
        format='%(levelname)5s %(module)15s(%(lineno)3d):%(message)s')

  if not args:
    parser.error('Must supply a command to run')
  if not options.log:
    parser.error('Must supply a log file with -l')

  if sys.platform == 'linux2':
    api = Strace
  elif sys.platform == 'darwin':
    api = Dtrace
  else:
    print >> sys.stderr, 'Unsupported platform'
    return 1

  return trace_inputs(options.log, args, api, options.gyp, options.product_dir)


if __name__ == '__main__':
  sys.exit(main())
