# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is heavily based off of LUCI net.py. It's been adopted to remove
# AppEngine-ism and convert from urllib to httplib2.

"""Wrapper around httplib2 to call REST API with service account credentials."""

from __future__ import print_function

import httplib2
import six
from six.moves import urllib

from chromite.lib import auth
from chromite.lib import constants
from chromite.lib import cros_logging as logging


def httprequest(http, **kwargs):
  """To be mocked in tests."""
  return http.request(**kwargs)


class Error(Exception):
  """Raised on non-transient errors.

  Attribute response is response body.
  """

  def __init__(self, msg, status_code, response, headers=None):
    super(Error, self).__init__(msg)
    self.status_code = status_code
    self.headers = headers
    self.response = response


class NotFoundError(Error):
  """Raised if endpoint returns 404."""


class AuthError(Error):
  """Raised if endpoint returns 401 or 403."""


def is_transient_error(response, url):
  """Returns True to retry the request."""
  if response.status >= 500 or response.status == 408:
    return True
  # Retry 404 iff it is a Cloud Endpoints API call *and* the
  # result is not JSON. This assumes that we only use JSON encoding.
  if response.status == 404:
    content_type = response.get('content-type', '')
    return (urllib.parse.urlparse(url).path.startswith('/_ah/api/') and
            not content_type.startswith('application/json'))
  return False


def _error_class_for_status(status_code):
  if status_code == 404:
    return NotFoundError
  if status_code in (401, 403):
    return AuthError
  return Error


def request(url,
            method='GET',
            payload=None,
            params=None,
            headers=None,
            include_auth=False,
            deadline=10,
            max_attempts=4):
  """Sends a REST API request, returns raw unparsed response.

  Retries the request on transient errors for up to |max_attempts| times.

  Args:
    url: url to send the request to.
    method: HTTP method to use, e.g. GET, POST, PUT.
    payload: raw data to put in the request body.
    params: dict with query GET parameters (i.e. ?key=value&key=value).
    headers: additional request headers.
    include_auth: Whether to include an OAuth2 access token.
    delegation_token: delegation token returned by auth.delegate.
    deadline: deadline for a single attempt (10 sec by default).
    max_attempts: how many times to retry on errors (4 times by default).

  Returns:
    Buffer with raw response.

  Raises:
    NotFoundError on 404 response.
    AuthError on 401 or 403 response.
    Error on any other non-transient error.
  """
  protocols = ('http://', 'https://')
  assert url.startswith(protocols) and '?' not in url, url
  if params:
    url += '?' + urllib.parse.urlencode(params)

  headers = (headers or {}).copy()

  if include_auth:
    tok = auth.GetAccessToken(
        service_account_json=constants.CHROMEOS_SERVICE_ACCOUNT)
    headers['Authorization'] = 'Bearer %s' % tok

  if payload is not None:
    assert isinstance(payload, (six.string_types, six.binary_type)), \
        type(payload)
    assert method in ('CREATE', 'POST', 'PUT'), method

  attempt = 0
  response = None
  last_status_code = None
  http = httplib2.Http(cache=None, timeout=deadline)
  http.follow_redirects = False
  while attempt < max_attempts:
    if attempt:
      logging.info('Retrying: %s %s', method, url)
    attempt += 1
    try:
      response, content = httprequest(
          http, uri=url, method=method, headers=headers, body=payload)
    except httplib2.HttpLib2Error as e:
      # Transient network error or URL fetch service RPC deadline.
      logging.warning('%s %s failed: %s', method, url, e)
      continue

    last_status_code = response.status

    # Transient error on the other side.
    if is_transient_error(response, url):
      logging.warning('%s %s failed with HTTP %d\nHeaders: %r\nBody: %r',
                      method, url, response.status, response, content)
      continue

    # Non-transient error.
    if 300 <= response.status < 500:
      logging.warning('%s %s failed with HTTP %d\nHeaders: %r\nBody: %r',
                      method, url, response.status, response, content)
      raise _error_class_for_status(response.status)(
          'Failed to call %s: HTTP %d' % (url, response.status),
          response.status,
          content,
          headers=response)

    # Success. Beware of large responses.
    if len(content) > 1024 * 1024:
      logging.warning('Response size: %.1f KiB', len(content) / 1024.0)
    return content

  raise _error_class_for_status(last_status_code)(
      'Failed to call %s after %d attempts' % (url, max_attempts),
      response.status if response else None,
      content if response else None,
      headers=response if response else None)
