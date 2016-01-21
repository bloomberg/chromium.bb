# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses a JSON request log created by log_requests.py."""

import collections
import json
import operator
import urlparse

Timing = collections.namedtuple(
    'Timing',
    ['connectEnd', 'connectStart', 'dnsEnd', 'dnsStart', 'proxyEnd',
     'proxyStart', 'receiveHeadersEnd', 'requestTime', 'sendEnd', 'sendStart',
     'sslEnd', 'sslStart', 'workerReady', 'workerStart', 'loadingFinished'])


class Resource(object):
  """Describes a resource."""

  def __init__(self, url, content_type):
    """Creates an instance of Resource.

    Args:
      url: URL of the resource
      content_type: Content-Type of the resources.
    """
    self.url = url
    self.content_type = content_type

  def GetShortName(self):
    """Returns either the hostname of the resource, or the filename,
    or the end of the path. Tries to include the domain as much as possible.
    """
    parsed = urlparse.urlparse(self.url)
    path = parsed.path
    if path != '' and path != '/':
      last_path = parsed.path.split('/')[-1]
      if len(last_path) < 10:
        if len(path) < 10:
          return parsed.hostname + '/' + path
        else:
          return parsed.hostname + '/..' + parsed.path[-10:]
      elif len(last_path) > 10:
        return parsed.hostname + '/..' + last_path[:5]
      else:
        return parsed.hostname + '/..' + last_path
    else:
      return parsed.hostname

  def GetContentType(self):
    mime = self.content_type
    if 'magic-debug-content' in mime:
      # A silly hack to make the unittesting easier.
      return 'magic-debug-content'
    elif mime == 'text/html':
      return 'html'
    elif mime == 'text/css':
      return 'css'
    elif mime in ('application/x-javascript', 'text/javascript',
                  'application/javascript'):
      return 'script'
    elif mime == 'application/json':
      return 'json'
    elif mime == 'image/gif':
      return 'gif_image'
    elif mime.startswith('image/'):
      return 'image'
    else:
      return 'other'

  @classmethod
  def FromRequest(cls, request):
    """Creates a Resource from an instance of RequestData."""
    return Resource(request.url, request.GetContentType())

  def __Fields(self):
    return (self.url, self.content_type)

  def __eq__(self, o):
    return  self.__Fields() == o.__Fields()

  def __hash__(self):
    return hash(self.__Fields())


class RequestData(object):
  """Represents a request, as dumped by log_requests.py."""

  def __init__(self, status, headers, request_headers, timestamp, timing, url,
               served_from_cache, initiator):
    self.status = status
    self.headers = headers
    self.request_headers = request_headers
    self.timestamp = timestamp
    self.timing = Timing(**timing) if timing else None
    self.url = url
    self.served_from_cache = served_from_cache
    self.initiator = initiator

  def IsDataUrl(self):
    return self.url.startswith('data:')

  def GetContentType(self):
    content_type = self.headers['Content-Type']
    if ';' in content_type:
      return content_type[:content_type.index(';')]
    else:
      return content_type

  @classmethod
  def FromDict(cls, r):
    """Creates a RequestData object from a dict."""
    return RequestData(r['status'], r['headers'], r['request_headers'],
                       r['timestamp'], r['timing'], r['url'],
                       r['served_from_cache'], r['initiator'])


def ParseJsonFile(filename):
  """Converts a JSON file to a sequence of RequestData."""
  with open(filename) as f:
    json_data = json.load(f)
    return [RequestData.FromDict(r) for r in json_data]


def FilterRequests(requests):
  """Filters a list of requests.

  Args:
    requests: [RequestData, ...]

  Returns:
    A list of requests that are not data URL, have a Content-Type, and are
    not served from the cache.
  """
  return [r for r in requests if not r.IsDataUrl()
          and 'Content-Type' in r.headers and not r.served_from_cache]


def ResourceToRequestMap(requests):
  """Returns a Resource -> Request map.

  A resource can be requested several times in a single page load. Keeps the
  first request in this case.

  Args:
    requests: [RequestData, ...]

  Returns:
    [Resource, ...]
  """
  # reversed(requests) because we want the first one to win.
  return dict([(Resource.FromRequest(r), r) for r in reversed(requests)])


def GetResources(requests):
  """Returns an ordered list of resources from a list of requests.

  The same resource can be requested several time for a single page load. This
  keeps only the first request.

  Args:
    requests: [RequestData]

  Returns:
    [Resource]
  """
  resources = []
  known_resources = set()
  for r in requests:
    resource = Resource.FromRequest(r)
    if r in known_resources:
      continue
    known_resources.add(resource)
    resources.append(resource)
  return resources


def ParseCacheControl(headers):
  """Parses the "Cache-Control" header and returns a dict representing it.

  Args:
    headers: (dict) Response headers.

  Returns:
    {Directive: Value, ...}
  """
  # TODO(lizeb): Handle the "Expires" header as well.
  result = {}
  cache_control = headers.get('Cache-Control', None)
  if cache_control is None:
    return result
  directives = [s.strip() for s in cache_control.split(',')]
  for directive in directives:
    parts = [s.strip() for s in directive.split('=')]
    if len(parts) == 1:
      result[parts[0]] = True
    else:
      result[parts[0]] = parts[1]
  return result


def MaxAge(request):
  """Returns the max-age of a resource, or -1."""
  cache_control = ParseCacheControl(request.headers)
  if (u'no-store' in cache_control
      or u'no-cache' in cache_control
      or len(cache_control) == 0):
    return -1
  if 'max-age' in cache_control:
    return int(cache_control['max-age'])
  return -1


def SortedByCompletion(requests):
  """Returns the requests, sorted by completion time."""
  return sorted(requests, key=operator.attrgetter('timestamp'))
