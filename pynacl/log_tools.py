#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging related tools."""

import logging
import os
import subprocess
import sys


_log_fh = None
_console_log_level = None
_no_annotator = False


def SetupLogging(verbose, file_handle=None, quiet=False, no_annotator=False):
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
    no_annotator: If True, only emit scrubbed annotator tags to the console.
                  Tags still go to the log.
  """
  # Since one of our handlers always wants debug, set the global level to debug.
  logging.getLogger().setLevel(logging.DEBUG)
  console_handler = logging.StreamHandler()
  console_handler.setFormatter(
      logging.Formatter(fmt='%(levelname)s: %(message)s'))
  global _no_annotator
  _no_annotator = no_annotator
  global _console_log_level
  if verbose:
    _console_log_level = logging.DEBUG
  elif quiet:
    _console_log_level = logging.WARN
  else:
    _console_log_level = logging.INFO
  console_handler.setLevel(_console_log_level)
  logging.getLogger().addHandler(console_handler)

  if file_handle:
    global _log_fh
    file_handler = logging.StreamHandler(file_handle)
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(
        logging.Formatter(fmt='%(levelname)s: %(message)s'))
    logging.getLogger().addHandler(file_handler)
    _log_fh = file_handle


def WriteToLog(text):
  """Write text to the current log file, and possibly stdout."""
  if _console_log_level == logging.DEBUG:
    sys.stdout.write(text)
  if _log_fh:
    _log_fh.write(text)


def WriteAnnotatorLine(text):
  """Flush stdout and print a message to stderr, also log.

  Buildbot annotator messages must be at the beginning of a line, and we want to
  ensure that any output from the script or from subprocesses appears in the
  correct order wrt BUILD_STEP messages. So we flush stdout before printing all
  buildbot messages here.

  Leading and trailing newlines are added.
  """
  if _console_log_level in [logging.DEBUG, logging.INFO]:
    sys.stdout.flush()
    if _no_annotator:
      sys.stderr.write('\n' + text.replace('@', '%') + '\n')
    else:
      sys.stderr.write('\n' + text + '\n')
  if _log_fh:
    _log_fh.write(text)


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
