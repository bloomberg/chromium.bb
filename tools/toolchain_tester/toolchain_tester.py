#!/usr/bin/python
# Copyright 2008 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Simple test suite for toolchains espcially llvm arm toolchains.

Sample invocations

tools/toolchain_tester/toolchain_tester.py --config=<config>
tools/toolchain_tester/toolchain_tester.py --config=<config> test1.c test2.c ...

NOTE: the location of tmp files is intentionally hardcoded, so you
can only run one instance of this at a time.
"""


import getopt
import glob
import os
import subprocess
import sys
import time

import toolchain_config
# ======================================================================
# Options
# ======================================================================
REPORT_STREAMS = [sys.stdout]
TIMEOUT = 120
VERBOSE = 0
TMP_PREFIX = '/tmp/tc_test_'
SHOW_CONSOLE = 1

# module with settings for compiler, etc.
CFG = None

# ======================================================================
# Hook print to we can print to both stdout and a file
def Print(message):
  for s in REPORT_STREAMS:
    print >>s, message


# ======================================================================
def Banner(message):
  Print('=' * 70)
  Print(message)
  Print('=' * 70)

# ======================================================================
def RunCommand(cmd, always_dump_stdout_stderr):
  """Run a shell command given as an argv style vector."""
  if VERBOSE:
    Print(str(cmd))
    Print(" ".join(cmd))
  start = time.time()
  p = subprocess.Popen(cmd,
                       bufsize=1000*1000,
                       stderr=subprocess.PIPE,
                       stdout=subprocess.PIPE)
  while p.poll() is None:
    time.sleep(1)
    now = time.time()
    if now - start > TIMEOUT:
      Print('Error: timeout')
      Print('Killing pid %d' % p.pid)
      os.waitpid(-1, os.WNOHANG)
      return -1
  stdout = p.stdout.read()
  stderr = p.stderr.read()
  retcode = p.wait()

  if retcode != 0:
    Print('Error: command failed %d %s' % (retcode, str(cmd)))
    always_dump_stdout_stderr = True

  if always_dump_stdout_stderr:
    Print(stderr)
    Print(stdout)
  return retcode


def RemoveTempFiles():
  for f in glob.glob(TMP_PREFIX + '.*'):
    os.remove(f)


def MakeExecutableCustom(config, test, extra):
  RemoveTempFiles()
  d = extra.copy()
  d['tmp'] = TMP_PREFIX
  d['src'] = test
  for phase, command in config.GetCommands(d):
    command = command.split()
    try:
      retcode = RunCommand(command, SHOW_CONSOLE)
    except Exception, err:
      Print("cannot run phase %s: %s" % (phase, str(err)))
      return phase
    if retcode:
      return phase
  # success
  return ''


def ParseCommandLineArgs(argv):
  """Process command line options and return the unprocessed left overs."""
  global VERBOSE, COMPILE_MODE, RUN_MODE, TMP_PREFIX, CFG
  try:
    opts, args = getopt.getopt(argv[1:], '',
                               ['verbose',
                                'show_console',
                                'config=',
                                'tmp='])
  except getopt.GetoptError, err:
    Print(str(err))  # will print something like 'option -a not recognized'
    sys.exit(-1)

  for o, a in opts:
    # strip the leading '--'
    o = o[2:]
    if o == 'verbose':
      VERBOSE = 1
    elif o == 'show_console':
      SHOW_CONSOLE = 1
    elif o == 'tmp':
      TMP_PREFIX = a
    elif o == 'config':
      CFG = a
    else:
      Print('ERROR: bad commandline arg: %s' % o)
      sys.exit(-1)
    # return the unprocessed options, i.e. the command
  return args


def RunSuite(config, files, extra_flags, errors):
  """Run a collection of benchmarks."""
  no_errors = sum(len(errors[k]) for k in errors)
  Banner('running %d tests' % (len(files)))
  for no, test in enumerate(files):
    result = MakeExecutableCustom(config, test, extra_flags)
    if result:
      Print('Failure %s: %s' % (result, test))
      errors[result].append(test)
      no_errors += 1
    Print('%03d/%03d err %2d: %s  %s' %
          (no, len(files), no_errors, os.path.basename(test), result))


def main(argv):
  extra = ParseCommandLineArgs(argv)

  if not CFG:
    print "ERROR: you must specify a toolchain-config using --config=<config>"
    print "Available configs are: "
    print "\n".join(toolchain_config.TOOLCHAIN_CONFIGS.keys())
    print
    return -1

  global TMP_PREFIX
  TMP_PREFIX = TMP_PREFIX + CFG

  Banner('Config: %s' % CFG)
  config = toolchain_config.TOOLCHAIN_CONFIGS[CFG]
  config.SanityCheck()
  Print('TMP_PREFIX: %s' % TMP_PREFIX)


  errors = {}
  for phase in config.GetPhases():
    errors[phase] = []


  RunSuite(config, extra, {}, errors)


  for k in errors:
    lst = errors[k]
    if not lst: continue
    Banner('%d failures in phase %s' % (len(lst), k))
    for e in lst:
      Print(e)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
