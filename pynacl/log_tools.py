#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging related tools."""

import logging
import os
import subprocess
import sys

# Module-level configuration
LOG_FH = None
VERBOSE = False

def SetupLogging(verbose, file_handle=None, quiet=False):
  """Set up python logging.

  Args:
    verbose: If True, log to stderr at DEBUG level and write subprocess output
             to stdout. Otherwise log to stderr at INFO level and do not print
             subprocess output unless there is an error.
    file_handle: If not None, must be a file-like object. All log output will
                 be written at DEBUG level, and all subprocess output will be
                 written, regardless of whether there are errors.
    quiet: If True, log to stderr at WARNING level only. Only valid if verbose
           is False.
  """
  # Since one of our handlers always wants debug, set the global level to debug.
  logging.getLogger().setLevel(logging.DEBUG)
  stderr_handler = logging.StreamHandler()
  stderr_handler.setFormatter(
      logging.Formatter(fmt='%(levelname)s: %(message)s'))
  if verbose:
    global VERBOSE
    VERBOSE = True
    stderr_handler.setLevel(logging.DEBUG)
  elif quiet:
    stderr_handler.setLevel(logging.WARN)
  else:
    stderr_handler.setLevel(logging.INFO)
  logging.getLogger().addHandler(stderr_handler)

  if file_handle:
    global LOG_FH
    file_handler = logging.StreamHandler(file_handle)
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(
        logging.Formatter(fmt='%(levelname)s: %(message)s'))
    logging.getLogger().addHandler(file_handler)
    LOG_FH = file_handle


def WriteToLog(text):
  if VERBOSE:
    sys.stdout.write(text)
  if LOG_FH:
    LOG_FH.write(text)


def CheckCall(command, stdout=None, **kwargs):
  """Modulate command output level based on logging level.

  If a logging file handle is set, always emit all output to it.
  If the log level is set at debug or lower, also emit all output to stdout.
  Otherwise, only emit output on error.
  Args:
    command: Command to run.
    stdout (optional): File name to redirect stdout to.
    **kwargs: Keyword args.
  """
  cwd = os.path.abspath(kwargs.get('cwd', os.getcwd()))
  logging.info('Running: subprocess.check_call(%r, cwd=%r)' % (command, cwd))

  if stdout is None:
    # Interleave stdout and stderr together and log that.
    p = subprocess.Popen(command,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT,
                         **kwargs)
    output = p.stdout
  else:
    p = subprocess.Popen(command,
                         stdout=open(stdout, 'w'),
                         stderr=subprocess.PIPE,
                         **kwargs)
    output = p.stderr

  # Capture the output as it comes and emit it immediately.
  line = output.readline()
  while line:
    WriteToLog(line)
    line = output.readline()

  if p.wait() != 0:
    raise subprocess.CalledProcessError(cmd=command, returncode=p.returncode)

  # Flush stdout so it does not get interleaved with future log or buildbot
  # output which goes to stderr.
  sys.stdout.flush()


def CheckOutput(command, **kwargs):
  """Capture stdout from a command, while logging its stderr.

  This is essentially subprocess.check_output, but stderr is
  handled the same way as in log_tools.CheckCall.
  Args:
    command: Command to run.
    **kwargs: Keyword args.
  """
  cwd = os.path.abspath(kwargs.get('cwd', os.getcwd()))
  logging.info('Running: subprocess.check_output(%r, cwd=%r)' % (command, cwd))

  p = subprocess.Popen(command,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       **kwargs)

  # Assume the output will not be huge or take a long time to come, so it
  # is viable to just consume it all synchronously before logging anything.
  # TODO(mcgrathr): Shovel stderr bits asynchronously if that ever seems
  # worth the hair.
  stdout_text, stderr_text = p.communicate()

  WriteToLog(stderr_text)

  if p.wait() != 0:
    raise subprocess.CalledProcessError(cmd=command, returncode=p.returncode)

  # Flush stdout so it does not get interleaved with future log or buildbot
  # output which goes to stderr.
  sys.stdout.flush()

  logging.info('Result: %r' % stdout_text)
  return stdout_text
