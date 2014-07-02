# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Various utility functions and classes not specific to any single area."""

import cStringIO
import json
import logging
import logging.handlers
import optparse
import os
import re
import sys
import time


class OptionParserWithLogging(optparse.OptionParser):
  """Adds --verbose option."""

  # Set to True to enable --log-file options.
  enable_log_file = True

  def __init__(self, verbose=0, log_file=None, **kwargs):
    kwargs.setdefault('description', sys.modules['__main__'].__doc__)
    optparse.OptionParser.__init__(self, **kwargs)
    self.group_logging = optparse.OptionGroup(self, 'Logging')
    self.group_logging.add_option(
        '-v', '--verbose',
        action='count',
        default=verbose,
        help='Use multiple times to increase verbosity')
    if self.enable_log_file:
      self.group_logging.add_option(
          '-l', '--log-file',
          default=log_file,
          help='The name of the file to store rotating log details')
      self.group_logging.add_option(
          '--no-log', action='store_const', const='', dest='log_file',
          help='Disable log file')

  def parse_args(self, *args, **kwargs):
    # Make sure this group is always the last one.
    self.add_option_group(self.group_logging)

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


def read_json(filepath):
  with open(filepath, 'r') as f:
    return json.load(f)


def write_json(filepath_or_handle, data, dense):
  """Writes data into filepath or file handle encoded as json.

  If dense is True, the json is packed. Otherwise, it is human readable.
  """
  if dense:
    kwargs = {'sort_keys': True, 'separators': (',',':')}
  else:
    kwargs = {'sort_keys': True, 'indent': 2}

  if hasattr(filepath_or_handle, 'write'):
    json.dump(data, filepath_or_handle, **kwargs)
  else:
    with open(filepath_or_handle, 'wb') as f:
      json.dump(data, f, **kwargs)


def format_json(data, dense):
  """Returns a string with json encoded data.

  If dense is True, the json is packed. Otherwise, it is human readable.
  """
  buf = cStringIO.StringIO()
  write_json(buf, data, dense)
  return buf.getvalue()


def gen_blacklist(regexes):
  """Returns a lambda to be used as a blacklist."""
  compiled = [re.compile(i) for i in regexes]
  return lambda f: any(j.match(f) for j in compiled)


def get_bool_env_var(name):
  """Return True if integer environment variable |name| value is non zero.

  If environment variable is missing or is set to '0', returns False.
  """
  return bool(int(os.environ.get(name, '0')))


def is_headless():
  """True if running in non-interactive mode on some bot machine.

  Examines os.environ for presence of SWARMING_HEADLESS var.
  """
  headless_env_keys = (
    # This is Chromium specific. Set when running under buildbot slave.
    'CHROME_HEADLESS',
    # Set when running under swarm bot.
    'SWARMING_HEADLESS',
  )
  return any(get_bool_env_var(key) for key in headless_env_keys)
