#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a .isolated, creates a tree of hardlinks and runs the test.

Keeps a local cache.
"""

import cookielib
import ctypes
import hashlib
import httplib
import inspect
import itertools
import json
import locale
import logging
import logging.handlers
import math
import optparse
import os
import Queue
import random
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import urllib
import urllib2
import urlparse
import zlib

# Try to import 'upload' module used by AppEngineService for authentication.
# If it is not there, app engine authentication support will be disabled.
try:
  from third_party import upload
  # Hack out upload logging.info()
  upload.logging = logging.getLogger('upload')
  # Mac pylint choke on this line.
  upload.logging.setLevel(logging.WARNING)  # pylint: disable=E1103
except ImportError:
  upload = None


# Types of action accepted by link_file().
HARDLINK, SYMLINK, COPY = range(1, 4)

RE_IS_SHA1 = re.compile(r'^[a-fA-F0-9]{40}$')

# The file size to be used when we don't know the correct file size,
# generally used for .isolated files.
UNKNOWN_FILE_SIZE = None

# The size of each chunk to read when downloading and unzipping files.
ZIPPED_FILE_CHUNK = 16 * 1024

# The name of the log file to use.
RUN_ISOLATED_LOG_FILE = 'run_isolated.log'

# The base directory containing this file.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# The name of the log to use for the run_test_cases.py command
RUN_TEST_CASES_LOG = os.path.join(BASE_DIR, 'run_test_cases.log')

# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30

# The name of the key to store the count of url attempts.
COUNT_KEY = 'UrlOpenAttempt'

# Default maximum number of attempts to trying opening a url before aborting.
URL_OPEN_MAX_ATTEMPTS = 30
# Default timeout when retrying.
URL_OPEN_TIMEOUT = 6*60.

# Global (for now) map: server URL (http://example.com) -> HttpService instance.
# Used by get_http_service to cache HttpService instances.
_http_services = {}
_http_services_lock = threading.Lock()

# Used by get_flavor().
FLAVOR_MAPPING = {
  'cygwin': 'win',
  'win32': 'win',
  'darwin': 'mac',
  'sunos5': 'solaris',
  'freebsd7': 'freebsd',
  'freebsd8': 'freebsd',
}


class ConfigError(ValueError):
  """Generic failure to load a .isolated file."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def get_flavor():
  """Returns the system default flavor. Copied from gyp/pylib/gyp/common.py."""
  return FLAVOR_MAPPING.get(sys.platform, 'linux')


def fix_default_encoding():
  """Forces utf8 solidly on all platforms.

  By default python execution environment is lazy and defaults to ascii
  encoding.

  http://uucode.com/blog/2007/03/23/shut-up-you-dummy-7-bit-python/
  """
  if sys.getdefaultencoding() == 'utf-8':
    return False

  # Regenerate setdefaultencoding.
  reload(sys)
  # Module 'sys' has no 'setdefaultencoding' member
  # pylint: disable=E1101
  sys.setdefaultencoding('utf-8')
  for attr in dir(locale):
    if attr[0:3] != 'LC_':
      continue
    aref = getattr(locale, attr)
    try:
      locale.setlocale(aref, '')
    except locale.Error:
      continue
    try:
      lang = locale.getlocale(aref)[0]
    except (TypeError, ValueError):
      continue
    if lang:
      try:
        locale.setlocale(aref, (lang, 'UTF-8'))
      except locale.Error:
        os.environ[attr] = lang + '.UTF-8'
  try:
    locale.setlocale(locale.LC_ALL, '')
  except locale.Error:
    pass
  return True


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


def os_link(source, link_name):
  """Add support for os.link() on Windows."""
  if sys.platform == 'win32':
    if not ctypes.windll.kernel32.CreateHardLinkW(
        unicode(link_name), unicode(source), 0):
      raise OSError()
  else:
    os.link(source, link_name)


def readable_copy(outfile, infile):
  """Makes a copy of the file that is readable by everyone."""
  shutil.copy(infile, outfile)
  read_enabled_mode = (os.stat(outfile).st_mode | stat.S_IRUSR |
                       stat.S_IRGRP | stat.S_IROTH)
  os.chmod(outfile, read_enabled_mode)


def link_file(outfile, infile, action):
  """Links a file. The type of link depends on |action|."""
  logging.debug('Mapping %s to %s' % (infile, outfile))
  if action not in (HARDLINK, SYMLINK, COPY):
    raise ValueError('Unknown mapping action %s' % action)
  if not os.path.isfile(infile):
    raise MappingError('%s is missing' % infile)
  if os.path.isfile(outfile):
    raise MappingError(
        '%s already exist; insize:%d; outsize:%d' %
        (outfile, os.stat(infile).st_size, os.stat(outfile).st_size))

  if action == COPY:
    readable_copy(outfile, infile)
  elif action == SYMLINK and sys.platform != 'win32':
    # On windows, symlink are converted to hardlink and fails over to copy.
    os.symlink(infile, outfile)  # pylint: disable=E1101
  else:
    try:
      os_link(infile, outfile)
    except OSError:
      # Probably a different file system.
      logging.warning(
          'Failed to hardlink, failing back to copy %s to %s' % (
            infile, outfile))
      readable_copy(outfile, infile)


def _set_write_bit(path, read_only):
  """Sets or resets the executable bit on a file or directory."""
  mode = os.lstat(path).st_mode
  if read_only:
    mode = mode & 0500
  else:
    mode = mode | 0200
  if hasattr(os, 'lchmod'):
    os.lchmod(path, mode)  # pylint: disable=E1101
  else:
    if stat.S_ISLNK(mode):
      # Skip symlink without lchmod() support.
      logging.debug('Can\'t change +w bit on symlink %s' % path)
      return

    # TODO(maruel): Implement proper DACL modification on Windows.
    os.chmod(path, mode)


def make_writable(root, read_only):
  """Toggle the writable bit on a directory tree."""
  assert os.path.isabs(root), root
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    for filename in filenames:
      _set_write_bit(os.path.join(dirpath, filename), read_only)

    for dirname in dirnames:
      _set_write_bit(os.path.join(dirpath, dirname), read_only)


def rmtree(root):
  """Wrapper around shutil.rmtree() to retry automatically on Windows."""
  make_writable(root, False)
  if sys.platform == 'win32':
    for i in range(3):
      try:
        shutil.rmtree(root)
        break
      except WindowsError:  # pylint: disable=E0602
        delay = (i+1)*2
        print >> sys.stderr, (
            'The test has subprocess outliving it. Sleep %d seconds.' % delay)
        time.sleep(delay)
  else:
    shutil.rmtree(root)


def is_same_filesystem(path1, path2):
  """Returns True if both paths are on the same filesystem.

  This is required to enable the use of hardlinks.
  """
  assert os.path.isabs(path1), path1
  assert os.path.isabs(path2), path2
  if sys.platform == 'win32':
    # If the drive letter mismatches, assume it's a separate partition.
    # TODO(maruel): It should look at the underlying drive, a drive letter could
    # be a mount point to a directory on another drive.
    assert re.match(r'^[a-zA-Z]\:\\.*', path1), path1
    assert re.match(r'^[a-zA-Z]\:\\.*', path2), path2
    if path1[0].lower() != path2[0].lower():
      return False
  return os.stat(path1).st_dev == os.stat(path2).st_dev


def get_free_space(path):
  """Returns the number of free bytes."""
  if sys.platform == 'win32':
    free_bytes = ctypes.c_ulonglong(0)
    ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        ctypes.c_wchar_p(path), None, None, ctypes.pointer(free_bytes))
    return free_bytes.value
  # For OSes other than Windows.
  f = os.statvfs(path)  # pylint: disable=E1101
  return f.f_bfree * f.f_frsize


def make_temp_dir(prefix, root_dir):
  """Returns a temporary directory on the same file system as root_dir."""
  base_temp_dir = None
  if not is_same_filesystem(root_dir, tempfile.gettempdir()):
    base_temp_dir = os.path.dirname(root_dir)
  return tempfile.mkdtemp(prefix=prefix, dir=base_temp_dir)


def load_isolated(content):
  """Verifies the .isolated file is valid and loads this object with the json
  data.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise ConfigError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise ConfigError('Expected dict, got %r' % data)

  for key, value in data.iteritems():
    if key == 'command':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty command')
      for subvalue in value:
        if not isinstance(subvalue, basestring):
          raise ConfigError('Expected string, got %r' % subvalue)

    elif key == 'files':
      if not isinstance(value, dict):
        raise ConfigError('Expected dict, got %r' % value)
      for subkey, subvalue in value.iteritems():
        if not isinstance(subkey, basestring):
          raise ConfigError('Expected string, got %r' % subkey)
        if not isinstance(subvalue, dict):
          raise ConfigError('Expected dict, got %r' % subvalue)
        for subsubkey, subsubvalue in subvalue.iteritems():
          if subsubkey == 'l':
            if not isinstance(subsubvalue, basestring):
              raise ConfigError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'm':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'h':
            if not RE_IS_SHA1.match(subsubvalue):
              raise ConfigError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 's':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          else:
            raise ConfigError('Unknown subsubkey %s' % subsubkey)
        if bool('h' in subvalue) and bool('l' in subvalue):
          raise ConfigError(
              'Did not expect both \'h\' (sha-1) and \'l\' (link), got: %r' %
              subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty includes list')
      for subvalue in value:
        if not RE_IS_SHA1.match(subvalue):
          raise ConfigError('Expected sha-1, got %r' % subvalue)

    elif key == 'read_only':
      if not isinstance(value, bool):
        raise ConfigError('Expected bool, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)

    elif key == 'os':
      if value != get_flavor():
        raise ConfigError(
            'Expected \'os\' to be \'%s\' but got \'%s\'' %
            (get_flavor(), value))

    else:
      raise ConfigError('Unknown key %s' % key)

  return data


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def url_open(url, **kwargs):
  """Attempts to open the given url multiple times.

  |data| can be either:
    -None for a GET request
    -str for pre-encoded data
    -list for data to be encoded
    -dict for data to be encoded (COUNT_KEY will be added in this case)

  Returns a file-like object, where the response may be read from, or None
  if it was unable to connect.
  """
  urlhost, urlpath = split_server_request_url(url)
  service = get_http_service(urlhost)
  return service.request(urlpath, **kwargs)


def split_server_request_url(url):
  """Splits the url into scheme+netloc and path+params+query+fragment."""
  url_parts = list(urlparse.urlparse(url))
  urlhost = '%s://%s' % (url_parts[0], url_parts[1])
  urlpath = urlparse.urlunparse(['', ''] + url_parts[2:])
  return urlhost, urlpath


def get_http_service(urlhost):
  """Returns existing or creates new instance of HttpService that can send
  requests to given base urlhost.
  """
  # Ensure consistency.
  urlhost = str(urlhost).lower().rstrip('/')
  with _http_services_lock:
    service = _http_services.get(urlhost)
    if not service:
      service = AppEngineService(urlhost)
      _http_services[urlhost] = service
    return service


class HttpService(object):
  """Base class for a class that provides an API to HTTP based service:
    - Provides 'request' method.
    - Supports automatic request retries.
    - Supports persistent cookies.
    - Thread safe.
  """

  # File to use to store all auth cookies.
  COOKIE_FILE = os.path.join('~', '.isolated_cookies')

  # CookieJar reused by all services + lock that protects its instantiation.
  _cookie_jar = None
  _cookie_jar_lock = threading.Lock()

  def __init__(self, urlhost):
    self.urlhost = urlhost
    self.cookie_jar = self.load_cookie_jar()
    self.opener = self.create_url_opener()

  def authenticate(self):  # pylint: disable=R0201
    """Called when HTTP server asks client to authenticate.
    Can be implemented in subclasses.
    """
    return False

  @staticmethod
  def load_cookie_jar():
    """Returns global CoookieJar object that stores cookies in the file."""
    with HttpService._cookie_jar_lock:
      if HttpService._cookie_jar is not None:
        return HttpService._cookie_jar
      jar = ThreadSafeCookieJar(os.path.expanduser(HttpService.COOKIE_FILE))
      jar.load()
      HttpService._cookie_jar = jar
      return jar

  @staticmethod
  def save_cookie_jar():
    """Called when cookie jar needs to be flushed to disk."""
    with HttpService._cookie_jar_lock:
      if HttpService._cookie_jar is not None:
        HttpService._cookie_jar.save()

  def create_url_opener(self):  # pylint: disable=R0201
    """Returns OpenerDirector that will be used when sending requests.
    Can be reimplemented in subclasses."""
    return urllib2.build_opener(urllib2.HTTPCookieProcessor(self.cookie_jar))

  def request(self, urlpath, data=None, content_type=None, **kwargs):
    """Attempts to open the given url multiple times.

    |urlpath| is relative to the server root, i.e. '/some/request?param=1'.

    |data| can be either:
      -None for a GET request
      -str for pre-encoded data
      -list for data to be encoded
      -dict for data to be encoded (COUNT_KEY will be added in this case)

    Returns a file-like object, where the response may be read from, or None
    if it was unable to connect.
    """
    assert urlpath and urlpath[0] == '/'

    if isinstance(data, dict) and COUNT_KEY in data:
      logging.error('%s already existed in the data passed into UlrOpen. It '
                    'would be overwritten. Aborting UrlOpen', COUNT_KEY)
      return None

    method = 'GET' if data is None else 'POST'
    assert not ((method != 'POST') and content_type), (
        'Can\'t use content_type on GET')

    def make_request(extra):
      """Returns a urllib2.Request instance for this specific retry."""
      if isinstance(data, str) or data is None:
        payload = data
      else:
        if isinstance(data, dict):
          payload = data.items()
        else:
          payload = data[:]
        payload.extend(extra.iteritems())
        payload = urllib.urlencode(payload)
      new_url = urlparse.urljoin(self.urlhost, urlpath[1:])
      if isinstance(data, str) or data is None:
        # In these cases, add the extra parameter to the query part of the url.
        url_parts = list(urlparse.urlparse(new_url))
        # Append the query parameter.
        if url_parts[4] and extra:
          url_parts[4] += '&'
        url_parts[4] += urllib.urlencode(extra)
        new_url = urlparse.urlunparse(url_parts)
      request = urllib2.Request(new_url, data=payload)
      if payload is not None:
        if content_type:
          request.add_header('Content-Type', content_type)
        request.add_header('Content-Length', len(payload))
      return request

    return self._retry_loop(make_request, **kwargs)

  def _retry_loop(
      self,
      make_request,
      max_attempts=URL_OPEN_MAX_ATTEMPTS,
      retry_404=False,
      retry_50x=True,
      timeout=URL_OPEN_TIMEOUT):
    """Runs internal request-retry loop.

    - Optionally retries HTTP 404 and 50x.
    - Retries up to |max_attempts| times. If None or 0, there's no limit in the
      number of retries.
    - Retries up to |timeout| duration in seconds. If None or 0, there's no
      limit in the time taken to do retries.
    - If both |max_attempts| and |timeout| are None or 0, this functions retries
      indefinitely.
    """
    authenticated = False
    last_error = None
    attempt = 0
    start = self._now()
    for attempt in itertools.count():
      if max_attempts and attempt >= max_attempts:
        # Too many attempts.
        break
      if timeout and (self._now() - start) >= timeout:
        # Retried for too long.
        break
      extra = {COUNT_KEY: attempt} if attempt else {}
      request = make_request(extra)
      try:
        url_response = self._url_open(request)
        logging.debug('url_open(%s) succeeded', request.get_full_url())
        return url_response
      except urllib2.HTTPError as e:
        # Unauthorized. Ask to authenticate and then try again.
        if e.code in (401, 403):
          # Try to authenticate only once. If it doesn't help, then server does
          # not support app engine authentication.
          logging.error(
              'Authentication is required for %s on attempt %d.\n%s',
              request.get_full_url(), attempt,
              self._format_exception(e, verbose=True))
          if not authenticated and self.authenticate():
            authenticated = True
            # Do not sleep.
            continue
          # If authentication failed, return.
          logging.error(
              'Unable to authenticate to %s.\n%s',
              request.get_full_url(), self._format_exception(e, verbose=True))
          return None

        if ((e.code < 500 and not (retry_404 and e.code == 404)) or
            (e.code >= 500 and not retry_50x)):
          # This HTTPError means we reached the server and there was a problem
          # with the request, so don't retry.
          logging.error(
              'Able to connect to %s but an exception was thrown.\n%s',
              request.get_full_url(), self._format_exception(e, verbose=True))
          return None

        # The HTTPError was due to a server error, so retry the attempt.
        logging.warning('Able to connect to %s on attempt %d.\n%s',
                        request.get_full_url(), attempt,
                        self._format_exception(e))
        last_error = e

      except (urllib2.URLError, httplib.HTTPException) as e:
        logging.warning('Unable to open url %s on attempt %d.\n%s',
                        request.get_full_url(), attempt,
                        self._format_exception(e))
        last_error = e

      # Only sleep if we are going to try again.
      if max_attempts and attempt != max_attempts:
        remaining = None
        if timeout:
          remaining = timeout - (self._now() - start)
          if remaining <= 0:
            break
        self.sleep_before_retry(attempt, remaining)

    logging.error('Unable to open given url, %s, after %d attempts.\n%s',
                  request.get_full_url(), max_attempts,
                  self._format_exception(last_error, verbose=True))
    return None

  def _url_open(self, request):
    """Low level method to execute urllib2.Request's.

    To be mocked in tests.
    """
    return self.opener.open(request)

  @staticmethod
  def _now():
    """To be mocked in tests."""
    return time.time()

  @staticmethod
  def calculate_sleep_before_retry(attempt, max_duration):
    # Maximum sleeping time. We're hammering a cloud-distributed service, it'll
    # survive.
    MAX_SLEEP = 10.
    # random.random() returns [0.0, 1.0). Starts with relatively short waiting
    # time by starting with 1.5/2+1.5^-1 median offset.
    duration = (random.random() * 1.5) + math.pow(1.5, (attempt - 1))
    assert duration > 0.1
    duration = min(MAX_SLEEP, duration)
    if max_duration:
      duration = min(max_duration, duration)
    return duration

  @classmethod
  def sleep_before_retry(cls, attempt, max_duration):
    """Sleeps for some amount of time when retrying the request.

    To be mocked in tests.
    """
    time.sleep(cls.calculate_sleep_before_retry(attempt, max_duration))

  @staticmethod
  def _format_exception(exc, verbose=False):
    """Given an instance of some exception raised by urlopen returns human
    readable piece of text with detailed information about the error.
    """
    out = ['Exception: %s' % (exc,)]
    if verbose:
      if isinstance(exc, urllib2.HTTPError):
        out.append('-' * 10)
        if exc.hdrs:
          for header, value in exc.hdrs.items():
            if not header.startswith('x-'):
              out.append('%s: %s' % (header.capitalize(), value))
          out.append('')
        out.append(exc.read() or '<empty body>')
        out.append('-' * 10)
    return '\n'.join(out)


class AppEngineService(HttpService):
  """This class implements authentication support for
  an app engine based services.
  """

  # This lock ensures that user won't be confused with multiple concurrent
  # login prompts.
  _auth_lock = threading.Lock()

  def __init__(self, urlhost, email=None, password=None):
    super(AppEngineService, self).__init__(urlhost)
    self.email = email
    self.password = password
    self._keyring = None

  def authenticate(self):
    """Authenticates in the app engine application.
    Returns True on success.
    """
    if not upload:
      logging.error('\'upload\' module is missing, '
                    'app engine authentication is disabled.')
      return False
    cookie_jar = self.cookie_jar
    save_cookie_jar = self.save_cookie_jar
    # RPC server that uses AuthenticationSupport's cookie jar.
    class AuthServer(upload.AbstractRpcServer):
      def _GetOpener(self):
        # Authentication code needs to know about 302 response.
        # So make OpenerDirector without HTTPRedirectHandler.
        opener = urllib2.OpenerDirector()
        opener.add_handler(urllib2.ProxyHandler())
        opener.add_handler(urllib2.UnknownHandler())
        opener.add_handler(urllib2.HTTPHandler())
        opener.add_handler(urllib2.HTTPDefaultErrorHandler())
        opener.add_handler(urllib2.HTTPSHandler())
        opener.add_handler(urllib2.HTTPErrorProcessor())
        opener.add_handler(urllib2.HTTPCookieProcessor(cookie_jar))
        return opener
      def PerformAuthentication(self):
        self._Authenticate()
        save_cookie_jar()
        return self.authenticated
    with AppEngineService._auth_lock:
      rpc_server = AuthServer(self.urlhost, self.get_credentials)
      return rpc_server.PerformAuthentication()

  def get_credentials(self):
    """Called during authentication process to get the credentials.
    May be called mutliple times if authentication fails.
    Returns tuple (email, password).
    """
    # 'authenticate' calls this only if 'upload' is present.
    # Ensure other callers (if any) fail non-cryptically if 'upload' is missing.
    assert upload, '\'upload\' module is required for this to work'
    if self.email and self.password:
      return (self.email, self.password)
    if not self._keyring:
      self._keyring = upload.KeyringCreds(self.urlhost,
                                          self.urlhost,
                                          self.email)
    return self._keyring.GetUserCredentials()


class ThreadSafeCookieJar(cookielib.MozillaCookieJar):
  """MozillaCookieJar with thread safe load and save."""

  def load(self, filename=None, ignore_discard=False, ignore_expires=False):
    """Loads cookies from the file if it exists."""
    filename = os.path.expanduser(filename or self.filename)
    with self._cookies_lock:
      if os.path.exists(filename):
        try:
          cookielib.MozillaCookieJar.load(self, filename,
                                          ignore_discard,
                                          ignore_expires)
          logging.debug('Loaded cookies from %s', filename)
        except (cookielib.LoadError, IOError):
          pass
      else:
        try:
          fd = os.open(filename, os.O_CREAT, 0600)
          os.close(fd)
        except OSError:
          logging.error('Failed to create %s', filename)
      try:
        os.chmod(filename, 0600)
      except OSError:
        logging.error('Failed to fix mode for %s', filename)

  def save(self, filename=None, ignore_discard=False, ignore_expires=False):
    """Saves cookies to the file, completely overwriting it."""
    logging.debug('Saving cookies to %s', filename or self.filename)
    with self._cookies_lock:
      try:
        cookielib.MozillaCookieJar.save(self, filename,
                                        ignore_discard,
                                        ignore_expires)
      except OSError:
        logging.error('Failed to save %s', filename)


class ThreadPoolError(Exception):
  """Base class for exceptions raised by ThreadPool."""


class ThreadPoolEmpty(ThreadPoolError):
  """Trying to get task result from a thread pool with no pending tasks."""


class ThreadPoolClosed(ThreadPoolError):
  """Trying to do something with a closed thread pool."""


class ThreadPool(object):
  """Implements a multithreaded worker pool oriented for mapping jobs with
  thread-local result storage.

  Arguments:
  - initial_threads: Number of threads to start immediately. Can be 0 if it is
    uncertain that threads will be needed.
  - max_threads: Maximum number of threads that will be started when all the
                 threads are busy working. Often the number of CPU cores.
  - queue_size: Maximum number of tasks to buffer in the queue. 0 for unlimited
                queue. A non-zero value may make add_task() blocking.
  - prefix: Prefix to use for thread names. Pool's threads will be
            named '<prefix>-<thread index>'.
  """
  QUEUE_CLASS = Queue.PriorityQueue

  def __init__(self, initial_threads, max_threads, queue_size, prefix=None):
    prefix = prefix or 'tp-0x%0x' % id(self)
    logging.debug(
        'New ThreadPool(%d, %d, %d): %s', initial_threads, max_threads,
        queue_size, prefix)
    assert initial_threads <= max_threads
    # Update this check once 256 cores CPU are common.
    assert max_threads <= 256

    self.tasks = self.QUEUE_CLASS(queue_size)
    self._max_threads = max_threads
    self._prefix = prefix

    # Mutables.
    self._num_of_added_tasks_lock = threading.Lock()
    self._num_of_added_tasks = 0
    self._outputs_exceptions_cond = threading.Condition()
    self._outputs = []
    self._exceptions = []

    # List of threads, number of threads in wait state, number of terminated and
    # starting threads. All protected by _workers_lock.
    self._workers_lock = threading.Lock()
    self._workers = []
    self._ready = 0
    # Number of terminated threads, used to handle some edge cases in
    # _is_task_queue_empty.
    self._dead = 0
    # Number of threads already added to _workers, but not yet running the loop.
    self._starting = 0
    # True if close was called. Forbids adding new tasks.
    self._is_closed = False

    for _ in range(initial_threads):
      self._add_worker()

  def _add_worker(self):
    """Adds one worker thread if there isn't too many. Thread-safe."""
    with self._workers_lock:
      if len(self._workers) >= self._max_threads or self._is_closed:
        return False
      worker = threading.Thread(
        name='%s-%d' % (self._prefix, len(self._workers)), target=self._run)
      self._workers.append(worker)
      self._starting += 1
    logging.debug('Starting worker thread %s', worker.name)
    worker.daemon = True
    worker.start()
    return True

  def add_task(self, priority, func, *args, **kwargs):
    """Adds a task, a function to be executed by a worker.

    |priority| can adjust the priority of the task versus others. Lower priority
    takes precedence.

    |func| can either return a return value to be added to the output list or
    be a generator which can emit multiple values.

    Returns the index of the item added, e.g. the total number of enqueued items
    up to now.
    """
    assert isinstance(priority, int)
    assert callable(func)
    with self._workers_lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not add a task to a closed ThreadPool')
      start_new_worker = (
        # Pending task count plus new task > number of available workers.
        self.tasks.qsize() + 1 > self._ready + self._starting and
        # Enough slots.
        len(self._workers) < self._max_threads
      )
    with self._num_of_added_tasks_lock:
      self._num_of_added_tasks += 1
      index = self._num_of_added_tasks
    self.tasks.put((priority, index, func, args, kwargs))
    if start_new_worker:
      self._add_worker()
    return index

  def _run(self):
    """Worker thread loop. Runs until a None task is queued."""
    started = False
    while True:
      try:
        with self._workers_lock:
          self._ready += 1
          if not started:
            self._starting -= 1
            started = True
        task = self.tasks.get()
      finally:
        with self._workers_lock:
          self._ready -= 1
      try:
        if task is None:
          # We're done.
          with self._workers_lock:
            self._dead += 1
          return
        _priority, _index, func, args, kwargs = task
        if inspect.isgeneratorfunction(func):
          for out in func(*args, **kwargs):
            self._output_append(out)
        else:
          out = func(*args, **kwargs)
          self._output_append(out)
      except Exception as e:
        logging.warning('Caught exception: %s', e)
        exc_info = sys.exc_info()
        logging.info(''.join(traceback.format_tb(exc_info[2])))
        self._outputs_exceptions_cond.acquire()
        try:
          self._exceptions.append(exc_info)
          self._outputs_exceptions_cond.notifyAll()
        finally:
          self._outputs_exceptions_cond.release()
      finally:
        try:
          self.tasks.task_done()
        except Exception as e:
          # We need to catch and log this error here because this is the root
          # function for the thread, nothing higher will catch the error.
          logging.exception('Caught exception while marking task as done: %s',
                            e)

  def _output_append(self, out):
    if out is not None:
      self._outputs_exceptions_cond.acquire()
      try:
        self._outputs.append(out)
        self._outputs_exceptions_cond.notifyAll()
      finally:
        self._outputs_exceptions_cond.release()

  def join(self):
    """Extracts all the results from each threads unordered.

    Call repeatedly to extract all the exceptions if desired.

    Note: will wait for all work items to be done before returning an exception.
    To get an exception early, use get_one_result().
    """
    # TODO(maruel): Stop waiting as soon as an exception is caught.
    self.tasks.join()
    self._outputs_exceptions_cond.acquire()
    try:
      if self._exceptions:
        e = self._exceptions.pop(0)
        raise e[0], e[1], e[2]
      out = self._outputs
      self._outputs = []
    finally:
      self._outputs_exceptions_cond.release()
    return out

  def get_one_result(self):
    """Returns the next item that was generated or raises an exception if one
    occurred.

    Raises:
      ThreadPoolEmpty - no results available.
    """
    # Get first available result.
    for result in self.iter_results():
      return result
    # No results -> tasks queue is empty.
    raise ThreadPoolEmpty('Task queue is empty')

  def iter_results(self):
    """Yields results as they appear until all tasks are processed."""
    while True:
      # Check for pending results.
      result = None
      self._outputs_exceptions_cond.acquire()
      try:
        if self._exceptions:
          e = self._exceptions.pop(0)
          raise e[0], e[1], e[2]
        if self._outputs:
          # Remember the result to yield it outside of the lock.
          result = self._outputs.pop(0)
        else:
          # No pending results and no pending tasks -> all tasks are done.
          if self._is_task_queue_empty():
            return
          # Some task is queued, wait for its result to appear.
          # Use non-None timeout so that process reacts to Ctrl+C and other
          # signals, see http://bugs.python.org/issue8844.
          self._outputs_exceptions_cond.wait(timeout=5)
          continue
      finally:
        self._outputs_exceptions_cond.release()
      yield result

  def _is_task_queue_empty(self):
    """True if task queue is empty and all workers are idle.

    Doesn't check for pending results from already finished tasks.

    Note: this property is not reliable in case tasks are still being
    enqueued by concurrent threads.
    """
    # Some pending tasks?
    if not self.tasks.empty():
      return False
    # Some workers are busy?
    with self._workers_lock:
      idle = self._ready + self._dead + self._starting
      if idle != len(self._workers):
        return False
    return True

  def close(self):
    """Closes all the threads."""
    # Ensure no new threads can be started, self._workers is effectively
    # a constant after that and can be accessed outside the lock.
    with self._workers_lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not close already closed ThreadPool')
      self._is_closed = True
    for _ in range(len(self._workers)):
      # Enqueueing None causes the worker to stop.
      self.tasks.put(None)
    for t in self._workers:
      t.join()
    logging.debug(
      'Thread pool \'%s\' closed: spawned %d threads total',
      self._prefix, len(self._workers))

  def __enter__(self):
    """Enables 'with' statement."""
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Enables 'with' statement."""
    self.close()


def valid_file(filepath, size):
  """Determines if the given files appears valid (currently it just checks
  the file's size)."""
  if size == UNKNOWN_FILE_SIZE:
    return True
  actual_size = os.stat(filepath).st_size
  if size != actual_size:
    logging.warning(
        'Found invalid item %s; %d != %d',
        os.path.basename(filepath), actual_size, size)
    return False
  return True


class Profiler(object):
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


class Remote(object):
  """Priority based worker queue to fetch or upload files from a
  content-address server. Any function may be given as the fetcher/upload,
  as long as it takes two inputs (the item contents, and their relative
  destination).

  Supports local file system, CIFS or http remotes.

  When the priority of items is equals, works in strict FIFO mode.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16
  # Priorities.
  LOW, MED, HIGH = (1<<8, 2<<8, 3<<8)
  INTERNAL_PRIORITY_BITS = (1<<8) - 1
  RETRIES = 5

  def __init__(self, destination_root):
    # Function to fetch a remote object or upload to a remote location..
    self._do_item = self.get_file_handler(destination_root)
    # Contains tuple(priority, obj).
    self._done = Queue.PriorityQueue()
    self._pool = ThreadPool(self.INITIAL_WORKERS, self.MAX_WORKERS, 0, 'upload')

  def join(self):
    """Blocks until the queue is empty."""
    return self._pool.join()

  def close(self):
    """Terminates all worker threads."""
    self._pool.close()

  def add_item(self, priority, obj, dest, size):
    """Retrieves an object from the remote data store.

    The smaller |priority| gets fetched first.

    Thread-safe.
    """
    assert (priority & self.INTERNAL_PRIORITY_BITS) == 0
    return self._add_item(priority, obj, dest, size)

  def _add_item(self, priority, obj, dest, size):
    assert isinstance(obj, basestring), obj
    assert isinstance(dest, basestring), dest
    assert size is None or isinstance(size, int), size
    return self._pool.add_task(
        priority, self._task_executer, priority, obj, dest, size)

  def get_one_result(self):
    return self._pool.get_one_result()

  def _task_executer(self, priority, obj, dest, size):
    """Wraps self._do_item to trap and retry on IOError exceptions."""
    try:
      self._do_item(obj, dest)
      if size and not valid_file(dest, size):
        download_size = os.stat(dest).st_size
        os.remove(dest)
        raise IOError('File incorrect size after download of %s. Got %s and '
                      'expected %s' % (obj, download_size, size))
      # TODO(maruel): Technically, we'd want to have an output queue to be a
      # PriorityQueue.
      return obj
    except IOError as e:
      logging.debug('Caught IOError: %s', e)
      # Retry a few times, lowering the priority.
      if (priority & self.INTERNAL_PRIORITY_BITS) < self.RETRIES:
        self._add_item(priority + 1, obj, dest, size)
        return
      raise

  def get_file_handler(self, file_or_url):  # pylint: disable=R0201
    """Returns a object to retrieve objects from a remote."""
    if re.match(r'^https?://.+$', file_or_url):
      def download_file(item, dest):
        # TODO(maruel): Reuse HTTP connections. The stdlib doesn't make this
        # easy.
        try:
          zipped_source = file_or_url + item
          logging.debug('download_file(%s)', zipped_source)

          # Because the app engine DB is only eventually consistent, retry
          # 404 errors because the file might just not be visible yet (even
          # though it has been uploaded).
          connection = url_open(zipped_source, retry_404=True)
          if not connection:
            raise IOError('Unable to open connection to %s' % zipped_source)
          decompressor = zlib.decompressobj()
          size = 0
          with open(dest, 'wb') as f:
            while True:
              chunk = connection.read(ZIPPED_FILE_CHUNK)
              if not chunk:
                break
              size += len(chunk)
              f.write(decompressor.decompress(chunk))
          # Ensure that all the data was properly decompressed.
          uncompressed_data = decompressor.flush()
          assert not uncompressed_data
        except IOError as e:
          logging.error(
              'Failed to download %s at %s.\n%s', item, dest, e)
          raise
        except httplib.HTTPException as e:
          msg = 'HTTPException while retrieving %s at %s.\n%s' % (
              item, dest, e)
          logging.error(msg)
          raise IOError(msg)
        except zlib.error as e:
          remaining_size = len(connection.read())
          msg = 'Corrupted zlib for item %s. Processed %d of %d bytes.\n%s' % (
              item, size, size + remaining_size, e)
          logging.error(msg)

          # Testing seems to show that if a few machines are trying to download
          # the same blob, they can cause each other to fail. So if we hit a
          # zip error, this is the most likely cause (it only downloads some of
          # the data). Randomly sleep for between 5 and 25 seconds to try and
          # spread out the downloads.
          # TODO(csharp): Switch from blobstorage to cloud storage and see if
          # that solves the issue.
          sleep_duration = (random.random() * 20) + 5
          time.sleep(sleep_duration)

          raise IOError(msg)


      return download_file

    def copy_file(item, dest):
      source = os.path.join(file_or_url, item)
      if source == dest:
        logging.info('Source and destination are the same, no action required')
        return
      logging.debug('copy_file(%s, %s)', source, dest)
      shutil.copy(source, dest)
    return copy_file


class CachePolicies(object):
  def __init__(self, max_cache_size, min_free_space, max_items):
    """
    Arguments:
    - max_cache_size: Trim if the cache gets larger than this value. If 0, the
                      cache is effectively a leak.
    - min_free_space: Trim if disk free space becomes lower than this value. If
                      0, it unconditionally fill the disk.
    - max_items: Maximum number of items to keep in the cache. If 0, do not
                 enforce a limit.
    """
    self.max_cache_size = max_cache_size
    self.min_free_space = min_free_space
    self.max_items = max_items


class NoCache(object):
  """This class is intended to be usable everywhere the Cache class is.
  Instead of downloading to a cache, all files are downloaded to the target
  directory and then moved to where they are needed.
  """

  def __init__(self, target_directory, remote):
    self.target_directory = target_directory
    self.remote = remote

  def retrieve(self, priority, item, size):
    """Get the request file."""
    self.remote.add_item(priority, item, self.path(item), size)
    self.remote.get_one_result()

  def wait_for(self, items):
    """Download the first item of the given list if it is missing."""
    item = items.iterkeys().next()

    if not os.path.exists(self.path(item)):
      self.remote.add_item(Remote.MED, item, self.path(item), UNKNOWN_FILE_SIZE)
      downloaded = self.remote.get_one_result()
      assert downloaded == item

    return item

  def path(self, item):
    return os.path.join(self.target_directory, item)


class Cache(object):
  """Stateful LRU cache.

  Saves its state as json file.
  """
  STATE_FILE = 'state.json'

  def __init__(self, cache_dir, remote, policies):
    """
    Arguments:
    - cache_dir: Directory where to place the cache.
    - remote: Remote where to fetch items from.
    - policies: cache retention policies.
    """
    self.cache_dir = cache_dir
    self.remote = remote
    self.policies = policies
    self.state_file = os.path.join(cache_dir, self.STATE_FILE)
    # The tuple(file, size) are kept as an array in a LRU style. E.g.
    # self.state[0] is the oldest item.
    self.state = []
    self._state_need_to_be_saved = False
    # A lookup map to speed up searching.
    self._lookup = {}
    self._lookup_is_stale = True

    # Items currently being fetched. Keep it local to reduce lock contention.
    self._pending_queue = set()

    # Profiling values.
    self._added = []
    self._removed = []
    self._free_disk = 0

    with Profiler('Setup'):
      if not os.path.isdir(self.cache_dir):
        os.makedirs(self.cache_dir)
      if os.path.isfile(self.state_file):
        try:
          self.state = json.load(open(self.state_file, 'r'))
        except (IOError, ValueError), e:
          # Too bad. The file will be overwritten and the cache cleared.
          logging.error(
              'Broken state file %s, ignoring.\n%s' % (self.STATE_FILE, e))
          self._state_need_to_be_saved = True
        if (not isinstance(self.state, list) or
            not all(
              isinstance(i, (list, tuple)) and len(i) == 2
              for i in self.state)):
          # Discard.
          self._state_need_to_be_saved = True
          self.state = []

      # Ensure that all files listed in the state still exist and add new ones.
      previous = set(filename for filename, _ in self.state)
      if len(previous) != len(self.state):
        logging.warning('Cache state is corrupted, found duplicate files')
        self._state_need_to_be_saved = True
        self.state = []

      added = 0
      for filename in os.listdir(self.cache_dir):
        if filename == self.STATE_FILE:
          continue
        if filename in previous:
          previous.remove(filename)
          continue
        # An untracked file.
        if not RE_IS_SHA1.match(filename):
          logging.warning('Removing unknown file %s from cache', filename)
          os.remove(self.path(filename))
          continue
        # Insert as the oldest file. It will be deleted eventually if not
        # accessed.
        self._add(filename, False)
        logging.warning('Add unknown file %s to cache', filename)
        added += 1

      if added:
        logging.warning('Added back %d unknown files', added)
      if previous:
        logging.warning('Removed %d lost files', len(previous))
        # Set explicitly in case self._add() wasn't called.
        self._state_need_to_be_saved = True
        # Filter out entries that were not found while keeping the previous
        # order.
        self.state = [
          (filename, size) for filename, size in self.state
          if filename not in previous
        ]
      self.trim()

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    with Profiler('CleanupTrimming'):
      self.trim()

    logging.info(
        '%5d (%8dkb) added', len(self._added), sum(self._added) / 1024)
    logging.info(
        '%5d (%8dkb) current',
        len(self.state),
        sum(i[1] for i in self.state) / 1024)
    logging.info(
        '%5d (%8dkb) removed', len(self._removed), sum(self._removed) / 1024)
    logging.info('       %8dkb free', self._free_disk / 1024)

  def remove_file_at_index(self, index):
    """Removes the file at the given index."""
    try:
      self._state_need_to_be_saved = True
      filename, size = self.state.pop(index)
      # If the lookup was already stale, its possible the filename was not
      # present yet.
      self._lookup_is_stale = True
      self._lookup.pop(filename, None)
      self._removed.append(size)
      os.remove(self.path(filename))
    except OSError as e:
      logging.error('Error attempting to delete a file\n%s' % e)

  def remove_lru_file(self):
    """Removes the last recently used file."""
    self.remove_file_at_index(0)

  def trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    # Ensure maximum cache size.
    if self.policies.max_cache_size and self.state:
      while sum(i[1] for i in self.state) > self.policies.max_cache_size:
        self.remove_lru_file()

    # Ensure maximum number of items in the cache.
    if self.policies.max_items and self.state:
      while len(self.state) > self.policies.max_items:
        self.remove_lru_file()

    # Ensure enough free space.
    self._free_disk = get_free_space(self.cache_dir)
    trimmed_due_to_space = False
    while (
        self.policies.min_free_space and
        self.state and
        self._free_disk < self.policies.min_free_space):
      trimmed_due_to_space = True
      self.remove_lru_file()
      self._free_disk = get_free_space(self.cache_dir)
    if trimmed_due_to_space:
      total = sum(i[1] for i in self.state)
      logging.warning(
          'Trimmed due to not enough free disk space: %.1fkb free, %.1fkb '
          'cache (%.1f%% of its maximum capacity)',
          self._free_disk / 1024.,
          total / 1024.,
          100. * self.policies.max_cache_size / float(total),
          )
    self.save()

  def retrieve(self, priority, item, size):
    """Retrieves a file from the remote, if not already cached, and adds it to
    the cache.

    If the file is in the cache, verifiy that the file is valid (i.e. it is
    the correct size), retrieving it again if it isn't.
    """
    assert not '/' in item
    path = self.path(item)
    self._update_lookup()
    index = self._lookup.get(item)

    if index is not None:
      if not valid_file(self.path(item), size):
        self.remove_file_at_index(index)
        index = None
      else:
        assert index < len(self.state)
        # Was already in cache. Update it's LRU value by putting it at the end.
        self._state_need_to_be_saved = True
        self._lookup_is_stale = True
        self.state.append(self.state.pop(index))

    if index is None:
      if item in self._pending_queue:
        # Already pending. The same object could be referenced multiple times.
        return
      # TODO(maruel): It should look at the free disk space, the current cache
      # size and the size of the new item on every new item:
      # - Trim the cache as more entries are listed when free disk space is low,
      #   otherwise if the amount of data downloaded during the run > free disk
      #   space, it'll crash.
      # - Make sure there's enough free disk space to fit all dependencies of
      #   this run! If not, abort early.
      self.remote.add_item(priority, item, path, size)
      self._pending_queue.add(item)

  def add(self, filepath, obj):
    """Forcibly adds a file to the cache."""
    self._update_lookup()
    if not obj in self._lookup:
      link_file(self.path(obj), filepath, HARDLINK)
      self._add(obj, True)

  def path(self, item):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, item)

  def save(self):
    """Saves the LRU ordering."""
    if self._state_need_to_be_saved:
      json.dump(self.state, open(self.state_file, 'wb'), separators=(',',':'))
      self._state_need_to_be_saved = False

  def wait_for(self, items):
    """Starts a loop that waits for at least one of |items| to be retrieved.

    Returns the first item retrieved.
    """
    # Flush items already present.
    self._update_lookup()
    for item in items:
      if item in self._lookup:
        return item

    assert all(i in self._pending_queue for i in items), (
        items, self._pending_queue)
    # Note that:
    #   len(self._pending_queue) ==
    #   ( len(self.remote._workers) - self.remote._ready +
    #     len(self._remote._queue) + len(self._remote.done))
    # There is no lock-free way to verify that.
    while self._pending_queue:
      item = self.remote.get_one_result()
      self._pending_queue.remove(item)
      self._add(item, True)
      if item in items:
        return item

  def _add(self, item, at_end):
    """Adds an item in the internal state.

    If |at_end| is False, self._lookup becomes inconsistent and
    self._update_lookup() must be called.
    """
    size = os.stat(self.path(item)).st_size
    self._added.append(size)
    self._state_need_to_be_saved = True
    if at_end:
      self.state.append((item, size))
      self._lookup[item] = len(self.state) - 1
    else:
      self._lookup_is_stale = True
      self.state.insert(0, (item, size))

  def _update_lookup(self):
    if self._lookup_is_stale:
      self._lookup = dict(
          (filename, index) for index, (filename, _) in enumerate(self.state))
      self._lookup_is_stale = False


class IsolatedFile(object):
  """Represents a single parsed .isolated file."""
  def __init__(self, obj_hash):
    """|obj_hash| is really the sha-1 of the file."""
    logging.debug('IsolatedFile(%s)' % obj_hash)
    self.obj_hash = obj_hash
    # Set once all the left-side of the tree is parsed. 'Tree' here means the
    # .isolate and all the .isolated files recursively included by it with
    # 'includes' key. The order of each sha-1 in 'includes', each representing a
    # .isolated file in the hash table, is important, as the later ones are not
    # processed until the firsts are retrieved and read.
    self.can_fetch = False

    # Raw data.
    self.data = {}
    # A IsolatedFile instance, one per object in self.includes.
    self.children = []

    # Set once the .isolated file is loaded.
    self._is_parsed = False
    # Set once the files are fetched.
    self.files_fetched = False

  def load(self, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_parsed
    self.data = load_isolated(content)
    self.children = [IsolatedFile(i) for i in self.data.get('includes', [])]
    self._is_parsed = True

  def fetch_files(self, cache, files):
    """Adds files in this .isolated file not present in |files| dictionary.

    Preemptively request files.

    Note that |files| is modified by this function.
    """
    assert self.can_fetch
    if not self._is_parsed or self.files_fetched:
      return
    logging.debug('fetch_files(%s)' % self.obj_hash)
    for filepath, properties in self.data.get('files', {}).iteritems():
      # Root isolated has priority on the files being mapped. In particular,
      # overriden files must not be fetched.
      if filepath not in files:
        files[filepath] = properties
        if 'h' in properties:
          # Preemptively request files.
          logging.debug('fetching %s' % filepath)
          cache.retrieve(Remote.MED, properties['h'], properties['s'])
    self.files_fetched = True


class Settings(object):
  """Results of a completely parsed .isolated file."""
  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main .isolated file, a IsolatedFile instance.
    self.root = None

  def load(self, cache, root_isolated_hash):
    """Loads the .isolated and all the included .isolated asynchronously.

    It enables support for "included" .isolated files. They are processed in
    strict order but fetched asynchronously from the cache. This is important so
    that a file in an included .isolated file that is overridden by an embedding
    .isolated file is not fetched neededlessly. The includes are fetched in one
    pass and the files are fetched as soon as all the ones on the left-side
    of the tree were fetched.

    The prioritization is very important here for nested .isolated files.
    'includes' have the highest priority and the algorithm is optimized for both
    deep and wide trees. A deep one is a long link of .isolated files referenced
    one at a time by one item in 'includes'. A wide one has a large number of
    'includes' in a single .isolated file. 'left' is defined as an included
    .isolated file earlier in the 'includes' list. So the order of the elements
    in 'includes' is important.
    """
    self.root = IsolatedFile(root_isolated_hash)
    cache.retrieve(Remote.HIGH, root_isolated_hash, UNKNOWN_FILE_SIZE)
    pending = {root_isolated_hash: self.root}
    # Keeps the list of retrieved items to refuse recursive includes.
    retrieved = [root_isolated_hash]

    def update_self(node):
      node.fetch_files(cache, self.files)
      # Grabs properties.
      if not self.command and node.data.get('command'):
        self.command = node.data['command']
      if self.read_only is None and node.data.get('read_only') is not None:
        self.read_only = node.data['read_only']
      if (self.relative_cwd is None and
          node.data.get('relative_cwd') is not None):
        self.relative_cwd = node.data['relative_cwd']

    def traverse_tree(node):
      if node.can_fetch:
        if not node.files_fetched:
          update_self(node)
        will_break = False
        for i in node.children:
          if not i.can_fetch:
            if will_break:
              break
            # Automatically mark the first one as fetcheable.
            i.can_fetch = True
            will_break = True
          traverse_tree(i)

    while pending:
      item_hash = cache.wait_for(pending)
      item = pending.pop(item_hash)
      item.load(open(cache.path(item_hash), 'r').read())
      if item_hash == root_isolated_hash:
        # It's the root item.
        item.can_fetch = True

      for new_child in item.children:
        h = new_child.obj_hash
        if h in retrieved:
          raise ConfigError('IsolatedFile %s is retrieved recursively' % h)
        pending[h] = new_child
        cache.retrieve(Remote.HIGH, h, UNKNOWN_FILE_SIZE)

      # Traverse the whole tree to see if files can now be fetched.
      traverse_tree(self.root)
    def check(n):
      return all(check(x) for x in n.children) and n.files_fetched
    assert check(self.root)
    self.relative_cwd = self.relative_cwd or ''
    self.read_only = self.read_only or False


def create_directories(base_directory, files):
  """Creates the directory structure needed by the given list of files."""
  logging.debug('create_directories(%s, %d)', base_directory, len(files))
  # Creates the tree of directories to create.
  directories = set(os.path.dirname(f) for f in files)
  for item in list(directories):
    while item:
      directories.add(item)
      item = os.path.dirname(item)
  for d in sorted(directories):
    if d:
      os.mkdir(os.path.join(base_directory, d))


def create_links(base_directory, files):
  """Creates any links needed by the given set of files."""
  for filepath, properties in files:
    if 'l' not in properties:
      continue
    if sys.platform == 'win32':
      # TODO(maruel): Create junctions or empty text files similar to what
      # cygwin do?
      logging.warning('Ignoring symlink %s', filepath)
      continue
    outfile = os.path.join(base_directory, filepath)
    # symlink doesn't exist on Windows. So the 'link' property should
    # never be specified for windows .isolated file.
    os.symlink(properties['l'], outfile)  # pylint: disable=E1101
    if 'm' in properties:
      lchmod = getattr(os, 'lchmod', None)
      if lchmod:
        lchmod(outfile, properties['m'])


def setup_commands(base_directory, cwd, cmd):
  """Correctly adjusts and then returns the required working directory
  and command needed to run the test.
  """
  assert not os.path.isabs(cwd), 'The cwd must be a relative path, got %s' % cwd
  cwd = os.path.join(base_directory, cwd)
  if not os.path.isdir(cwd):
    os.makedirs(cwd)

  # Ensure paths are correctly separated on windows.
  cmd[0] = cmd[0].replace('/', os.path.sep)
  cmd = fix_python_path(cmd)

  return cwd, cmd


def generate_remaining_files(files):
  """Generates a dictionary of all the remaining files to be downloaded."""
  remaining = {}
  for filepath, props in files:
    if 'h' in props:
      remaining.setdefault(props['h'], []).append((filepath, props))

  return remaining


def download_test_data(isolated_hash, target_directory, remote):
  """Downloads the dependencies to the given directory."""
  if not os.path.exists(target_directory):
    os.makedirs(target_directory)

  settings = Settings()
  no_cache = NoCache(target_directory, Remote(remote))

  # Download all the isolated files.
  with Profiler('GetIsolateds') as _prof:
    settings.load(no_cache, isolated_hash)

  if not settings.command:
    print >> sys.stderr, 'No command to run'
    return 1

  with Profiler('GetRest') as _prof:
    create_directories(target_directory, settings.files)
    create_links(target_directory, settings.files.iteritems())

    cwd, cmd = setup_commands(target_directory, settings.relative_cwd,
                              settings.command[:])

    remaining = generate_remaining_files(settings.files.iteritems())

    # Now block on the remaining files to be downloaded and mapped.
    logging.info('Retrieving remaining files')
    last_update = time.time()
    while remaining:
      obj = no_cache.wait_for(remaining)
      files = remaining.pop(obj)

      for i, (filepath, properties) in enumerate(files):
        outfile = os.path.join(target_directory, filepath)
        logging.info(no_cache.path(obj))

        if i + 1 == len(files):
          os.rename(no_cache.path(obj), outfile)
        else:
          shutil.copyfile(no_cache.path(obj), outfile)

        if 'm' in properties and not sys.platform == 'win32':
          # It's not set on Windows. It could be set only in the case of
          # downloading content generated from another OS. Do not crash in that
          # case.
          os.chmod(outfile, properties['m'])

      if time.time() - last_update > DELAY_BETWEEN_UPDATES_IN_SECS:
        logging.info('%d files remaining...' % len(remaining))
        last_update = time.time()

  print('.isolated files successfully downloaded and setup in %s' %
        target_directory)
  print('To run this test please run the command %s from the directory %s' %
        (cmd, cwd))

  return 0


def run_tha_test(isolated_hash, cache_dir, remote, policies):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  settings = Settings()
  with Cache(cache_dir, Remote(remote), policies) as cache:
    outdir = make_temp_dir('run_tha_test', cache_dir)
    try:
      # Initiate all the files download.
      with Profiler('GetIsolateds') as _prof:
        # Optionally support local files.
        if not RE_IS_SHA1.match(isolated_hash):
          # Adds it in the cache. While not strictly necessary, this simplifies
          # the rest.
          h = hashlib.sha1(open(isolated_hash, 'rb').read()).hexdigest()
          cache.add(isolated_hash, h)
          isolated_hash = h
        settings.load(cache, isolated_hash)

      if not settings.command:
        print >> sys.stderr, 'No command to run'
        return 1

      with Profiler('GetRest') as _prof:
        create_directories(outdir, settings.files)
        create_links(outdir, settings.files.iteritems())
        remaining = generate_remaining_files(settings.files.iteritems())

        # Do bookkeeping while files are being downloaded in the background.
        cwd, cmd = setup_commands(outdir, settings.relative_cwd,
                                  settings.command[:])

        # Now block on the remaining files to be downloaded and mapped.
        logging.info('Retrieving remaining files')
        last_update = time.time()
        while remaining:
          obj = cache.wait_for(remaining)
          for filepath, properties in remaining.pop(obj):
            outfile = os.path.join(outdir, filepath)
            link_file(outfile, cache.path(obj), HARDLINK)
            if 'm' in properties:
              # It's not set on Windows.
              os.chmod(outfile, properties['m'])

          if time.time() - last_update > DELAY_BETWEEN_UPDATES_IN_SECS:
            logging.info('%d files remaining...' % len(remaining))
            last_update = time.time()

      if settings.read_only:
        make_writable(outdir, True)
      logging.info('Running %s, cwd=%s' % (cmd, cwd))

      # TODO(csharp): This should be specified somewhere else.
      # Add a rotating log file if one doesn't already exist.
      env = os.environ.copy()
      env.setdefault('RUN_TEST_CASES_LOG_FILE', RUN_TEST_CASES_LOG)
      try:
        with Profiler('RunTest') as _prof:
          return subprocess.call(cmd, cwd=cwd, env=env)
      except OSError:
        print >> sys.stderr, 'Failed to run %s; cwd=%s' % (cmd, cwd)
        raise
    finally:
      rmtree(outdir)


def main():
  disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog <options>', description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')

  group = optparse.OptionGroup(parser, 'Download')
  group.add_option(
      '--download', metavar='DEST',
      help='Downloads files to DEST and returns without running, instead of '
           'downloading and then running from a temporary directory.')
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, 'Data source')
  group.add_option(
      '-s', '--isolated',
      metavar='FILE',
      help='File/url describing what to map or run')
  group.add_option(
      '-H', '--hash',
      help='Hash of the .isolated to grab from the hash table')
  parser.add_option_group(group)

  group.add_option(
      '-r', '--remote', metavar='URL',
      default=
          'https://isolateserver.appspot.com/content/retrieve/default-gzip/',
      help='Remote where to get the items. Defaults to %default')
  group = optparse.OptionGroup(parser, 'Cache management')
  group.add_option(
      '--cache',
      default='cache',
      metavar='DIR',
      help='Cache directory, default=%default')
  group.add_option(
      '--max-cache-size',
      type='int',
      metavar='NNN',
      default=20*1024*1024*1024,
      help='Trim if the cache gets larger than this value, default=%default')
  group.add_option(
      '--min-free-space',
      type='int',
      metavar='NNN',
      default=2*1024*1024*1024,
      help='Trim if disk free space becomes lower than this value, '
           'default=%default')
  group.add_option(
      '--max-items',
      type='int',
      metavar='NNN',
      default=100000,
      help='Trim if more than this number of items are in the cache '
           'default=%default')
  parser.add_option_group(group)

  options, args = parser.parse_args()
  levels = [logging.WARNING, logging.INFO, logging.DEBUG]
  level = levels[min(len(levels) - 1, options.verbose)]

  logging_console = logging.StreamHandler()
  logging_console.setFormatter(logging.Formatter(
      '%(levelname)5s %(module)15s(%(lineno)3d): %(message)s'))
  logging_console.setLevel(level)
  logging.getLogger().addHandler(logging_console)

  logging_rotating_file = logging.handlers.RotatingFileHandler(
      RUN_ISOLATED_LOG_FILE,
      maxBytes=10 * 1024 * 1024, backupCount=5)
  logging_rotating_file.setLevel(logging.DEBUG)
  logging_rotating_file.setFormatter(logging.Formatter(
      '%(asctime)s %(levelname)-8s %(module)15s(%(lineno)3d): %(message)s'))
  logging.getLogger().addHandler(logging_rotating_file)

  logging.getLogger().setLevel(logging.DEBUG)

  if bool(options.isolated) == bool(options.hash):
    logging.debug('One and only one of --isolated or --hash is required.')
    parser.error('One and only one of --isolated or --hash is required.')
  if args:
    logging.debug('Unsupported args %s' % ' '.join(args))
    parser.error('Unsupported args %s' % ' '.join(args))

  options.cache = os.path.abspath(options.cache)
  policies = CachePolicies(
      options.max_cache_size, options.min_free_space, options.max_items)

  if options.download:
    return download_test_data(options.isolated or options.hash,
                              options.download, options.remote)
  else:
    try:
      return run_tha_test(
          options.isolated or options.hash,
          options.cache,
          options.remote,
          policies)
    except Exception, e:
      # Make sure any exception is logged.
      logging.exception(e)
      return 1


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_default_encoding()
  sys.exit(main())
