# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes and functions for generic network communication over HTTP."""

import cookielib
import cStringIO as StringIO
import httplib
import itertools
import logging
import math
import os
import random
import socket
import ssl
import threading
import time
import urllib
import urllib2
import urlparse

from third_party.rietveld import upload

# Hack out upload logging.info()
upload.logging = logging.getLogger('upload')
# Mac pylint choke on this line.
upload.logging.setLevel(logging.WARNING)  # pylint: disable=E1103


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


class TimeoutError(IOError):
  """Timeout while reading HTTP response."""

  def __init__(self, inner_exc=None):
    super(TimeoutError, self).__init__(str(inner_exc or 'Timeout'))
    self.inner_exc = inner_exc


def url_open(url, **kwargs):
  """Attempts to open the given url multiple times.

  |data| can be either:
    -None for a GET request
    -str for pre-encoded data
    -list for data to be encoded
    -dict for data to be encoded (COUNT_KEY will be added in this case)

  Returns HttpResponse object, where the response may be read from, or None
  if it was unable to connect.
  """
  urlhost, urlpath = split_server_request_url(url)
  service = get_http_service(urlhost)
  return service.request(urlpath, **kwargs)


def url_read(url, **kwargs):
  """Attempts to open the given url multiple times and read all data from it.

  Accepts same arguments as url_open function.

  Returns all data read or None if it was unable to connect or read the data.
  """
  response = url_open(url, **kwargs)
  if not response:
    return None
  try:
    return response.read()
  except TimeoutError:
    return None


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
  COOKIE_FILE = os.path.join(os.path.expanduser('~'), '.isolated_cookies')

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
      jar = ThreadSafeCookieJar(HttpService.COOKIE_FILE)
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
      timeout=URL_OPEN_TIMEOUT,
      read_timeout=None):
    """Runs internal request-retry loop.

    - Optionally retries HTTP 404 and 50x.
    - Retries up to |max_attempts| times. If None or 0, there's no limit in the
      number of retries.
    - Retries up to |timeout| duration in seconds. If None or 0, there's no
      limit in the time taken to do retries.
    - If both |max_attempts| and |timeout| are None or 0, this functions retries
      indefinitely.

    If |read_timeout| is not None will configure underlying socket to
    raise TimeoutError exception whenever there's no response from the server
    for more than |read_timeout| seconds. It can happen during any read
    operation so once you pass non-None |read_timeout| be prepared to handle
    these exceptions in subsequent reads from the stream.
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
        url_response = self._url_open(request, timeout=read_timeout)
        logging.debug('url_open(%s) succeeded', request.get_full_url())
        # Some tests mock url_open to return StringIO without 'headers'.
        return HttpResponse(url_response, request.get_full_url(),
            getattr(url_response, 'headers', {}))
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

      except (urllib2.URLError, httplib.HTTPException,
              socket.timeout, ssl.SSLError) as e:
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

  def _url_open(self, request, timeout=None):
    """Low level method to execute urllib2.Request's.

    To be mocked in tests.
    """
    if timeout is not None:
      return self.opener.open(request, timeout=timeout)
    else:
      # Leave original default value for |timeout|. It's nontrivial.
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


class HttpResponse(object):
  """Response from HttpService."""

  def __init__(self, stream, url, headers):
    self._stream = stream
    self._url = url
    self._headers = headers
    self._read = 0

  @property
  def content_length(self):
    """Total length to the response or None if not known in advance."""
    length = self._headers.get('Content-Length')
    return int(length) if length is not None else None

  def read(self, size=None):
    """Reads up to |size| bytes from the stream and returns them.

    If |size| is None reads all available bytes.

    Raises TimeoutError on read timeout.
    """
    try:
      # cStringIO has a bug: stream.read(None) is not the same as stream.read().
      data = self._stream.read() if size is None else self._stream.read(size)
      self._read += len(data)
      return data
    except (socket.timeout, ssl.SSLError) as e:
      logging.error('Timeout while reading from %s, read %d of %s: %s',
          self._url, self._read, self.content_length, e)
      raise TimeoutError(e)

  @classmethod
  def get_fake_response(cls, content, url):
    """Returns HttpResponse with predefined content, useful in tests."""
    return cls(StringIO.StringIO(content),
        url, {'content-length': len(content)})



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
