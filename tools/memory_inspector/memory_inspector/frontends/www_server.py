# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module implements a simple WSGI server for the memory_inspector Web UI.

The WSGI server essentially handles two kinds of requests:
 - /ajax/foo/bar: The AJAX endpoints which exchange JSON data with the JS.
    Requests routing is achieved using a simple @uri decorator which simply
    performs regex matching on the request path.
 - /static/content: Anything not matching the /ajax/ prefix is treated as a
    static content request (for serving the index.html and JS/CSS resources).

The following HTTP status code are returned by the server:
 - 200 - OK: The request was handled correctly.
 - 404 - Not found: None of the defined handlers did match the /request/path.
 - 410 - Gone: The path was matched but the handler returned an empty response.
    This typically happens when the target device is disconnected.
"""

import collections
import datetime
import os
import mimetypes
import json
import re
import urlparse
import wsgiref.simple_server

from memory_inspector.core import backends
from memory_inspector.data import serialization
from memory_inspector.data import file_storage


_HTTP_OK = '200 - OK'
_HTTP_GONE = '410 - Gone'
_HTTP_NOT_FOUND = '404 - Not Found'
_PERSISTENT_STORAGE_PATH = os.path.join(
    os.path.expanduser('~'), '.config', 'memory_inspector')
_CONTENT_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), 'www_content'))
_APP_PROCESS_RE = r'^[\w.:]+$'  # Regex for matching app processes.
_STATS_HIST_SIZE = 120  # Keep at most 120 samples of stats per process.

_persistent_storage = file_storage.Storage(_PERSISTENT_STORAGE_PATH)
_proc_stats_history = {}  # /Android/device/PID -> deque([stats@T=0, stats@T=1])


class UriHandler(object):
  """Base decorator used to automatically route /requests/by/path.

  Each handler is called with the following args:
    args: a tuple of the matching regex groups.
    req_vars: a dictionary of request args (querystring for GET, body for POST).
  Each handler must return a tuple with the following elements:
    http_code: a string with the HTTP status code (e.g., '200 - OK')
    headers: a list of HTTP headers (e.g., [('Content-Type': 'foo/bar')])
    body: the HTTP response body.
  """
  _handlers = []

  def __init__(self, path_regex, verb='GET', output_filter=None):
    self._path_regex = path_regex
    self._verb = verb
    default_output_filter = lambda *x: x  # Just return the same args unchanged.
    self._output_filter = output_filter or default_output_filter

  def __call__(self, handler):
    UriHandler._handlers += [(
        self._verb, self._path_regex, self._output_filter, handler)]

  @staticmethod
  def Handle(method, path, req_vars):
    """Finds a matching handler and calls it (or returns a 404 - Not Found)."""
    for (match_method, path_regex, output_filter, fn) in UriHandler._handlers:
      if method != match_method:
        continue
      m = re.match(path_regex, path)
      if not m:
        continue
      (http_code, headers, body) = fn(m.groups(), req_vars)
      return output_filter(http_code, headers, body)
    return (_HTTP_NOT_FOUND, [], 'No AJAX handlers found')


class AjaxHandler(UriHandler):
  """Decorator for routing AJAX requests.

  This decorator essentially groups the JSON serialization and the cache headers
  which is shared by most of the handlers defined below.
  """
  def __init__(self, path_regex, verb='GET'):
    super(AjaxHandler, self).__init__(
        path_regex, verb, AjaxHandler.AjaxOutputFilter)

  @staticmethod
  def AjaxOutputFilter(http_code, headers, body):
    serialized_content = json.dumps(body, cls=serialization.Encoder)
    extra_headers = [('Cache-Control', 'no-cache'),
                     ('Expires', 'Fri, 19 Sep 1986 05:00:00 GMT')]
    return http_code, headers + extra_headers, serialized_content

@AjaxHandler('/ajax/backends')
def _ListBackends(args, req_vars):  # pylint: disable=W0613
  return _HTTP_OK, [], [backend.name for backend in backends.ListBackends()]


@AjaxHandler('/ajax/devices')
def _ListDevices(args, req_vars):  # pylint: disable=W0613
  resp = []
  for device in backends.ListDevices():
    # The device settings must loaded at discovery time (i.e. here), not during
    # startup, because it might have been plugged later.
    for k, v in _persistent_storage.LoadSettings(device.id).iteritems():
      device.settings[k] = v

    resp += [{'backend': device.backend.name,
              'id': device.id,
              'name': device.name}]
  return _HTTP_OK, [], resp


@AjaxHandler('/ajax/initialize/(\w+)/(\w+)$')  # /ajax/initialize/Android/a0b1c2
def _InitializeDevice(args, req_vars):  # pylint: disable=W0613
  device = _GetDevice(args)
  if not device:
    return _HTTP_GONE, [], 'Device not found'
  device.Initialize()
  return _HTTP_OK, [], {
    'is_mmap_tracing_enabled': device.IsMmapTracingEnabled(),
    'is_native_alloc_tracing_enabled': device.IsNativeAllocTracingEnabled()}


@AjaxHandler(r'/ajax/ps/(\w+)/(\w+)$')  # /ajax/ps/Android/a0b1c2[?all=1]
def _ListProcesses(args, req_vars):  # pylint: disable=W0613
  """Lists processes and their CPU / mem stats.

  The response is formatted according to the Google Charts DataTable format.
  """
  device = _GetDevice(args)
  if not device:
    return _HTTP_GONE, [], 'Device not found'
  resp = {
      'cols': [
          {'label': 'Pid', 'type':'number'},
          {'label': 'Name', 'type':'string'},
          {'label': 'Cpu %', 'type':'number'},
          {'label': 'Mem RSS Kb', 'type':'number'},
          {'label': '# Threads', 'type':'number'},
        ],
      'rows': []}
  for process in device.ListProcesses():
    # Exclude system apps if the request didn't contain the ?all=1 arg.
    if not req_vars.get('all') and not re.match(_APP_PROCESS_RE, process.name):
      continue
    stats = process.GetStats()
    resp['rows'] += [{'c': [
        {'v': process.pid, 'f': None},
        {'v': process.name, 'f': None},
        {'v': stats.cpu_usage, 'f': None},
        {'v': stats.vm_rss, 'f': None},
        {'v': stats.threads, 'f': None},
    ]}]
  return _HTTP_OK, [], resp


@AjaxHandler(r'/ajax/stats/(\w+)/(\w+)$')  # /ajax/stats/Android/a0b1c2
def _GetDeviceStats(args, req_vars):  # pylint: disable=W0613
  """Lists device CPU / mem stats.

  The response is formatted according to the Google Charts DataTable format.
  """
  device = _GetDevice(args)
  if not device:
    return _HTTP_GONE, [], 'Device not found'
  device_stats = device.GetStats()

  cpu_stats = {
      'cols': [
          {'label': 'CPU', 'type':'string'},
          {'label': 'Usr %', 'type':'number'},
          {'label': 'Sys %', 'type':'number'},
          {'label': 'Idle %', 'type':'number'},
        ],
      'rows': []}

  for cpu_idx in xrange(len(device_stats.cpu_times)):
    cpu = device_stats.cpu_times[cpu_idx]
    cpu_stats['rows'] += [{'c': [
        {'v': '# %d' % cpu_idx, 'f': None},
        {'v': cpu['usr'], 'f': None},
        {'v': cpu['sys'], 'f': None},
        {'v': cpu['idle'], 'f': None},
    ]}]

  mem_stats = {
      'cols': [
          {'label': 'Section', 'type':'string'},
          {'label': 'MB', 'type':'number',  'pattern': ''},
        ],
      'rows': []}

  for key, value in device_stats.memory_stats.iteritems():
    mem_stats['rows'] += [{'c': [
        {'v': key, 'f': None},
        {'v': value / 1024, 'f': None}
    ]}]

  return _HTTP_OK, [], {'cpu': cpu_stats, 'mem': mem_stats}


@AjaxHandler(r'/ajax/stats/(\w+)/(\w+)/(\d+)$')  # /ajax/stats/Android/a0b1c2/42
def _GetProcessStats(args, req_vars):  # pylint: disable=W0613
  """Lists CPU / mem stats for a given process (and keeps history).

  The response is formatted according to the Google Charts DataTable format.
  """
  process = _GetProcess(args)
  if not process:
    return _HTTP_GONE, [], 'Device not found'

  proc_uri = '/'.join(args)
  cur_stats = process.GetStats()
  if proc_uri not in _proc_stats_history:
    _proc_stats_history[proc_uri] = collections.deque(maxlen=_STATS_HIST_SIZE)
  history = _proc_stats_history[proc_uri]
  history.append(cur_stats)

  cpu_stats = {
      'cols': [
          {'label': 'T', 'type':'string'},
          {'label': 'CPU %', 'type':'number'},
          {'label': '# Threads', 'type':'number'},
        ],
      'rows': []
  }

  mem_stats = {
      'cols': [
          {'label': 'T', 'type':'string'},
          {'label': 'Mem RSS Kb', 'type':'number'},
          {'label': 'Page faults', 'type':'number'},
        ],
      'rows': []
  }

  for stats in history:
    cpu_stats['rows'] += [{'c': [
          {'v': str(datetime.timedelta(seconds=stats.run_time)), 'f': None},
          {'v': stats.cpu_usage, 'f': None},
          {'v': stats.threads, 'f': None},
    ]}]
    mem_stats['rows'] += [{'c': [
          {'v': str(datetime.timedelta(seconds=stats.run_time)), 'f': None},
          {'v': stats.vm_rss, 'f': None},
          {'v': stats.page_faults, 'f': None},
    ]}]

  return _HTTP_OK, [], {'cpu': cpu_stats, 'mem': mem_stats}


@AjaxHandler(r'/ajax/settings/(\w+)/?(\w+)?$')  # /ajax/settings/Android[/id]
def _GetDeviceOrBackendSettings(args, req_vars):  # pylint: disable=W0613
  backend = backends.GetBackend(args[0])
  if not backend:
    return _HTTP_GONE, [], 'Backend not found'
  if args[1]:
    device = _GetDevice(args)
    if not device:
      return _HTTP_GONE, [], 'Device not found'
    settings = device.settings
  else:
    settings = backend.settings

  assert(isinstance(settings, backends.Settings))
  resp = {}
  for key  in settings.expected_keys:
    resp[key] = {'description': settings.expected_keys[key],
                 'value': settings.values[key]}
  return _HTTP_OK, [], resp


@AjaxHandler(r'/ajax/settings/(\w+)/?(\w+)?$', 'POST')
def _SetDeviceOrBackendSettings(args, req_vars):  # pylint: disable=W0613
  backend = backends.GetBackend(args[0])
  if not backend:
    return _HTTP_GONE, [], 'Backend not found'
  if args[1]:
    device = _GetDevice(args)
    if not device:
      return _HTTP_GONE, [], 'Device not found'
    settings = device.settings
    storage_name = device.id
  else:
    settings = backend.settings
    storage_name = backend.name

  for key in req_vars.iterkeys():
    settings[key] = req_vars[key]
  _persistent_storage.StoreSettings(storage_name, settings.values)
  return _HTTP_OK, [], ''


@UriHandler(r'^(?!/ajax)/(.*)$')
def _StaticContent(args, req_vars):  # pylint: disable=W0613
  # Give the browser a 1-day TTL cache to minimize the start-up time.
  cache_headers = [('Cache-Control', 'max-age=86400, public')]
  req_path = args[0] if args[0] else 'index.html'
  file_path = os.path.abspath(os.path.join(_CONTENT_DIR, req_path))
  if (os.path.isfile(file_path) and
      os.path.commonprefix([file_path, _CONTENT_DIR]) == _CONTENT_DIR):
    mtype = 'text/plain'
    guessed_mime = mimetypes.guess_type(file_path)
    if guessed_mime and guessed_mime[0]:
      mtype = guessed_mime[0]
    with open(file_path, 'rb') as f:
      body = f.read()
    return _HTTP_OK, cache_headers + [('Content-Type', mtype)], body
  return _HTTP_NOT_FOUND, cache_headers,  file_path + ' not found'


def _GetDevice(args):
  """Returns a |backends.Device| instance from a /backend/device URI."""
  assert(len(args) >= 2), 'Malformed request. Expecting /backend/device'
  return backends.GetDevice(backend_name=args[0], device_id=args[1])


def _GetProcess(args):
  """Returns a |backends.Process| instance from a /backend/device/pid URI."""
  assert(len(args) >= 3 and args[2].isdigit()), (
      'Malformed request. Expecting /backend/device/pid')
  device = _GetDevice(args)
  if not device:
    return None
  return device.GetProcess(int(args[2]))


def _HttpRequestHandler(environ, start_response):
  """Parses a single HTTP request and delegates the handling through UriHandler.

  This essentially wires up wsgiref.simple_server with our @UriHandler(s).
  """
  path = environ['PATH_INFO']
  method = environ['REQUEST_METHOD']
  if method == 'POST':
    req_body_size = int(environ.get('CONTENT_LENGTH', 0))
    req_body = environ['wsgi.input'].read(req_body_size)
    req_vars = json.loads(req_body)
  else:
    req_vars = urlparse.parse_qs(environ['QUERY_STRING'])
  (http_code, headers, body) = UriHandler.Handle(method, path, req_vars)
  start_response(http_code, headers)
  return [body]


def Start(http_port):
  # Load the saved backends' settings (some of them might be needed to bootstrap
  # as, for instance, the adb path for the Android backend).
  for backend in backends.ListBackends():
    for k, v in _persistent_storage.LoadSettings(backend.name).iteritems():
      backend.settings[k] = v

  httpd = wsgiref.simple_server.make_server('', http_port, _HttpRequestHandler)
  httpd.serve_forever()