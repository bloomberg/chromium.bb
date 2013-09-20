# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Various utility functions and classes not specific to any single area."""

import logging
import logging.handlers
import optparse
import os
import sys
import time


class OptionParserWithLogging(optparse.OptionParser):
  """Adds --verbose option."""

  # Set to True to enable --log-file options.
  enable_log_file = True

  def __init__(self, verbose=0, log_file=None, **kwargs):
    kwargs.setdefault('description', sys.modules['__main__'].__doc__)
    optparse.OptionParser.__init__(self, **kwargs)
    self.add_option(
        '-v', '--verbose',
        action='count',
        default=verbose,
        help='Use multiple times to increase verbosity')
    if self.enable_log_file:
      self.add_option(
          '-l', '--log-file',
          default=log_file,
          help='The name of the file to store rotating log details')
      self.add_option(
          '--no-log', action='store_const', const='', dest='log_file',
          help='Disable log file')

  def parse_args(self, *args, **kwargs):
    options, args = optparse.OptionParser.parse_args(self, *args, **kwargs)
    levels = [logging.ERROR, logging.INFO, logging.DEBUG]
    level = levels[min(len(levels) - 1, options.verbose)]

    logging_console = logging.StreamHandler()
    logging_console.setFormatter(logging.Formatter(
        '%(levelname)5s %(module)15s(%(lineno)3d): %(message)s'))
    logging_console.setLevel(level)
    logging.getLogger().setLevel(level)
    logging.getLogger().addHandler(logging_console)

    if self.enable_log_file and options.log_file:
      # This is necessary otherwise attached handler will miss the messages.
      logging.getLogger().setLevel(logging.DEBUG)

      logging_rotating_file = logging.handlers.RotatingFileHandler(
          options.log_file,
          maxBytes=10 * 1024 * 1024,
          backupCount=5,
          encoding='utf-8')
      # log files are always at DEBUG level.
      logging_rotating_file.setLevel(logging.DEBUG)
      logging_rotating_file.setFormatter(logging.Formatter(
          '%(asctime)s %(levelname)-8s %(module)15s(%(lineno)3d): %(message)s'))
      logging.getLogger().addHandler(logging_rotating_file)

    return options, args


class Profiler(object):
  """Context manager that records time spend inside its body."""
  def __init__(self, name):
    self.name = name
    self.start_time = None

  def __enter__(self):
    self.start_time = time.time()
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    time_taken = time.time() - self.start_time
    logging.info('Profiling: Section %s took %3.3f seconds',
                 self.name, time_taken)


class Unbuffered(object):
  """Disable buffering on a file object."""
  def __init__(self, stream):
    self.stream = stream

  def write(self, data):
    self.stream.write(data)
    if '\n' in data:
      self.stream.flush()

  def __getattr__(self, attr):
    return getattr(self.stream, attr)


def disable_buffering():
  """Makes this process and child processes stdout unbuffered."""
  if not os.environ.get('PYTHONUNBUFFERED'):
    # Since sys.stdout is a C++ object, it's impossible to do
    # sys.stdout.write = lambda...
    sys.stdout = Unbuffered(sys.stdout)
    os.environ['PYTHONUNBUFFERED'] = 'x'


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out
