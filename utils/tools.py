# Copyright 2013 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

"""Various utility functions and classes not specific to any single area."""

import atexit
import functools
import json
import logging
import os
import re
import sys
import threading
import time

import utils
from . import zip_package

if sys.version_info.major == 2:
  import cStringIO
else:
  import io as cStringIO

# Path to (possibly extracted from zip) cacert.pem bundle file.
# See get_cacerts_bundle().
_ca_certs = None
_ca_certs_lock = threading.Lock()


# @cached decorators registered by report_cache_stats_at_exit.
_caches = []
_caches_lock = threading.Lock()


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


class ProfileCounter(object):
  """Records total time spent in a chunk of code during lifetime of a process.

  Recursive calls count as a single call (i.e. only the time spent in the outer
  call is recorded).

  Autoregisters itself in a global list when instantiated. All counters will be
  reported at the process exit time (in atexit hook). Best to be used as with
  @profile decorator.
  """

  _instances_lock = threading.Lock()
  _instances = []

  @staticmethod
  def summarize_all():
    print('\nProfiling report:')
    print('-' * 80)
    print(
        '{:<38}{:<10}{:<16}{:<16}'.format(
            'Name', 'Count', 'Total ms', 'Average ms'))
    print('-' * 80)
    with ProfileCounter._instances_lock:
      for i in sorted(ProfileCounter._instances, key=lambda x: -x.total_time):
        print(
            '{:<38}{:<10}{:<16.1f}{:<16.1f}'.format(
                i.name,
                i.call_count,
                i.total_time * 1000,
                i.average_time * 1000))
    print('-' * 80)

  def __init__(self, name):
    self._lock = threading.Lock()
    self._call_count = 0
    self._name = name
    self._total_time = 0
    self._active = threading.local()
    with self._instances_lock:
      self._instances.append(self)
      if len(self._instances) == 1:
        atexit.register(ProfileCounter.summarize_all)

  @property
  def name(self):
    return self._name

  @property
  def call_count(self):
    return self._call_count

  @property
  def total_time(self):
    return self._total_time

  @property
  def average_time(self):
    with self._lock:
      if self._call_count:
        return self._total_time / self._call_count
      return 0

  def __enter__(self):
    recursion = getattr(self._active, 'recursion', 0)
    if not recursion:
      self._active.started = time.time()
    self._active.recursion = recursion + 1

  def __exit__(self, _exc_type, _exec_value, _traceback):
    self._active.recursion -= 1
    if not self._active.recursion:
      time_inside = time.time() - self._active.started
      with self._lock:
        self._total_time += time_inside
        self._call_count += 1


def profile(func):
  """Decorator that profiles a function if SWARMING_PROFILE env var is set.

  Will gather a number of calls to that function and total time spent inside.
  The final report is emitted to stdout at the process exit time.
  """
  # No performance impact whatsoever if SWARMING_PROFILE is not set.
  if os.environ.get('SWARMING_PROFILE') != '1':
    return func
  timer = ProfileCounter(func.__name__)
  @functools.wraps(func)
  def wrapper(*args, **kwargs):
    with timer:
      return func(*args, **kwargs)
  return wrapper


def report_cache_stats_at_exit(func, cache):
  """Registers a hook that reports state of the cache on the process exit."""
  # Very dumb. Tries to account for object reuse though.
  def get_size(obj, seen):
    # Use id(...) to avoid triggering __hash__ and comparing by value instead.
    if id(obj) in seen:
      return 0
    seen.add(id(obj))
    size = sys.getsizeof(obj)
    if isinstance(obj, (list, tuple)):
      return size + sum(get_size(x, seen) for x in obj)
    elif isinstance(obj, dict):
      return size + sum(
          get_size(k, seen) + get_size(v, seen) for k, v in obj.iteritems())
    return size

  def report_caches_state():
    print('\nFunction cache report:')
    print('-' * 80)
    print('{:<40}{:<16}{:<26}'.format('Name', 'Items', 'Approx size, KB'))
    print('-' * 80)
    with _caches_lock:
      total = 0
      seen_objects = set()
      for func, cache in sorted(_caches, key=lambda x: -len(x[1])):
        size = get_size(cache, seen_objects)
        total += size
        print(
            '{:<40}{:<16}{:<26}'.format(func.__name__, len(cache), size / 1024))
    print('-' * 80)
    print('Total: %.1f MB' % (total / 1024 / 1024,))
    print('-' * 80)

  with _caches_lock:
    _caches.append((func, cache))
    if len(_caches) == 1:
      atexit.register(report_caches_state)


def cached(func):
  """Decorator that permanently caches a result of function invocation.

  It tries to be super fast and because of that is somewhat limited:
    * The function being cached can accept only positional arguments.
    * All arguments should be hashable.
    * The function may be called multiple times with same arguments in
      multithreaded environment.
    * The cache is not cleared up at all.

  If SWARMING_PROFILE env var is set, will produce a report about the state of
  the cache at the process exit (number of items and approximate size).
  """
  empty = object()
  cache = {}

  if os.environ.get('SWARMING_PROFILE') == '1':
    report_cache_stats_at_exit(func, cache)

  @functools.wraps(func)
  def wrapper(*args):
    v = cache.get(args, empty)
    if v is empty:
      v = func(*args)
      cache[args] = v
    return v

  wrapper.__cache__ = cache
  return wrapper


def clear_cache(func):
  """Clears all accumulated cached results of the given decorated function."""
  func.__cache__.clear()


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


def add_python_cmd(cmd):
  """Adds a Python executable to the front of the command if running a .py file.

  No-op if arg0 is anything besides a .py file.

  Args:
    cmd: A list containing the command to be run.

  Returns:
    |cmd| with a Python executable appended to the front if necessary.
  """
  if cmd[0].endswith('.py'):
    if sys.platform == 'win32':
      return ['python.exe'] + cmd
    return ['python'] + cmd
  return cmd


def find_executable(cmd, env=None):
  """Finds the executable to run the given command via $PATH.

  Automatically appends an appropriate Python executable to the front if arg0 is
  a .py file before searching.

  Slashes in cmd[0] are normalized to the current platform's default.

  On Windows if cmd[0] has no extension, this will try an .exe and .bat
  extension. If you want some more-esoteric extension (like .cmd), you need to
  explicitly include it.

  On POSIX, only files which are accessible to the current uid (as determined by
  os.access(..., X_OK)) are considered.

  If cmd[0] is an absolute path, $PATH will not be consulted.
  If cmd[0] is a relative path (i.e. contains a slash), it will be converted to
    an absolute path (i.e. against $CWD), and $PATH will not be consulted.
  Otherwise, cmd[0] will be evaluated against $PATH.

  NOTE: due to historical Swarming semantics, this prepends "." to PATH when
  searching. If we could do this over, it would be better to have the caller
  specify './executable' when they want something relative to the current
  directory.

  Args:
    cmd: A list containing the command to be run.
    env: The environment to use instead of os.environ.

  Returns:
    A copy of |cmd| with its arg0 executable changed to the absolute path for
    the executable found via $PATH. arg0 is left unchanged if it is not found in
    $PATH. Will not modify the original |cmd| list.
  """
  cmd = add_python_cmd(cmd)

  def _is_executable(candidate):
    return os.path.isfile(candidate) and os.access(candidate, os.X_OK)

  # anti_sep is like os.path.altsep, but it's always defined
  anti_sep = '/' if os.path.sep == '\\' else '\\'
  cmd = [cmd[0].replace(anti_sep, os.path.sep)] + cmd[1:]

  # exts are the file extensions to try. If the command already has an extension
  # or we're not on windows, then we don't try any extensions.
  has_ext = bool(os.path.splitext(cmd[0])[1])
  exts = ('',) if sys.platform != 'win32' or has_ext else ('.exe', '.bat')

  def _resolve_extension(candidate):
    for ext in exts:
      resolved = candidate + ext
      if _is_executable(resolved):
        return resolved
    return None

  # If the command is absolute or relative to cwd, check it directly and do not
  # consult $PATH.
  if os.path.sep in cmd[0]:
    # abspath is a noop on an already-absolute path
    resolved = _resolve_extension(os.path.abspath(cmd[0]))
    if resolved:
      cmd = [resolved] + cmd[1:]
    return cmd

  # We have a non-absolute, non-relative executable, so walk PATH.
  paths = (os.environ if env is None else env).get('PATH', '').split(os.pathsep)
  for path in ['.'] + paths:
    if path == '':
      continue
    resolved = _resolve_extension(os.path.join(os.path.abspath(path), cmd[0]))
    if resolved:
      cmd = [resolved] + cmd[1:]
      break

  return cmd


def read_json(filepath):
  with open(filepath, 'r') as f:
    return json.load(f)


def write_json(filepath_or_handle, data, dense):
  """Writes data into filepath or file handle encoded as json.

  If dense is True, the json is packed. Otherwise, it is human readable.
  """
  if dense:
    kwargs = {'sort_keys': True, 'separators': (',', ':')}
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
  compiled = [re.compile(i) for i in regexes or []]
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


def get_cacerts_bundle():
  """Returns path to a file with CA root certificates bundle.

  Python's ssl module needs a real file on disk, so if code is running from
  a zip archive, we need to extract the file first.
  """
  global _ca_certs
  with _ca_certs_lock:
    if _ca_certs is not None and os.path.exists(_ca_certs):
      return _ca_certs
    # Some rogue process clears /tmp and causes cacert.pem to disappear. Extract
    # to current directory instead. We use our own bundled copy of cacert.pem.
    _ca_certs = zip_package.extract_resource(utils, 'cacert.pem', temp_dir='.')
    return _ca_certs


def sliding_timeout(timeout):
  """Returns a function that returns how much time left till (now+timeout).

  If timeout is None, the returned function always returns None.
  """
  if timeout is None:
    return lambda: None
  deadline = time.time() + timeout
  return lambda: deadline - time.time()


_THIRD_PARTY_FIXED = False


def force_local_third_party():
  """Put the local third_party in front of sys.path.

  This is important for tools, especially the Swarming bot, as we don't know
  what python packages are installed and which version.
  """
  global _THIRD_PARTY_FIXED
  if _THIRD_PARTY_FIXED:
    return
  _THIRD_PARTY_FIXED = True
  src = os.path.abspath(zip_package.get_main_script_path())
  root = os.path.dirname(src)
  sys.path.insert(0, os.path.join(
      root, 'third_party', 'httplib2', 'python%d' % sys.version_info.major))
  sys.path.insert(0, os.path.join(root, 'third_party', 'pyasn1'))
  sys.path.insert(0, os.path.join(root, 'third_party', 'rsa'))
  sys.path.insert(0, os.path.join(root, 'third_party'))
