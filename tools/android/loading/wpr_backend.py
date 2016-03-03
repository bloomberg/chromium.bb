# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Opens and modifies WPR archive.
"""

import collections
import os
import re
import sys


_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

_WEBPAGEREPLAY_DIR = os.path.join(_SRC_DIR, 'third_party', 'webpagereplay')
_WEBPAGEREPLAY_HTTPARCHIVE = os.path.join(_WEBPAGEREPLAY_DIR, 'httparchive.py')

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'webpagereplay'))
import httparchive

# Regex used to parse httparchive.py stdout's when listing all urls.
_PARSE_WPR_REQUEST_REGEX = re.compile(r'^\S+\s+(?P<url>\S+)')


class WprUrlEntry(object):
  """Wpr url entry holding request and response infos. """

  def __init__(self, wpr_request, wpr_response):
    self._wpr_response = wpr_response
    self.url = self._ExtractUrl(str(wpr_request))

  def GetResponseHeadersDict(self):
    """Get a copied dictionary of available headers.

    Returns:
      dict(name -> value)
    """
    headers = collections.defaultdict(list)
    for (key, value) in self._wpr_response.headers:
      headers[key.lower()].append(value)
    return {k: ','.join(v) for (k, v) in headers.items()}

  def SetResponseHeader(self, name, value):
    """Set a header value.

    In the case where the <name> response header is present more than once
    in the response header list, then the given value is set only to the first
    occurrence of that given headers, and the next ones are removed.

    Args:
      name: The name of the response header to set.
      value: The value of the response header to set.
    """
    assert name.islower()
    new_headers = []
    new_header_set = False
    for header in self._wpr_response.headers:
      if header[0].lower() != name:
        new_headers.append(header)
      elif not new_header_set:
        new_header_set = True
        new_headers.append((header[0], value))
    if new_header_set:
      self._wpr_response.headers = new_headers
    else:
      self._wpr_response.headers.append((name, value))

  def DeleteResponseHeader(self, name):
    """Delete a header.

    In the case where the <name> response header is present more than once
    in the response header list, this method takes care of removing absolutely
    all them.

    Args:
      name: The name of the response header field to delete.
    """
    assert name.islower()
    self._wpr_response.headers = \
        [x for x in self._wpr_response.headers if x[0].lower() != name]

  @classmethod
  def _ExtractUrl(cls, request_string):
    match = _PARSE_WPR_REQUEST_REGEX.match(request_string)
    assert match, 'Looks like there is an issue with: {}'.format(request_string)
    return match.group('url')


class WprArchiveBackend(object):
  """WPR archive back-end able to read and modify. """

  def __init__(self, wpr_archive_path):
    """Constructor:

    Args:
      wpr_archive_path: The path of the WPR archive to read/modify.
    """
    self._wpr_archive_path = wpr_archive_path
    self._http_archive = httparchive.HttpArchive.Load(wpr_archive_path)

  def ListUrlEntries(self):
    """Iterates over all url entries

    Returns:
      A list of WprUrlEntry.
    """
    return [WprUrlEntry(request, self._http_archive[request])
            for request in self._http_archive.get_requests()]

  def Persist(self):
    """Persists the archive to disk. """
    self._http_archive.Persist(self._wpr_archive_path)


if __name__ == '__main__':
  import argparse
  parser = argparse.ArgumentParser(description='Tests cache back-end.')
  parser.add_argument('wpr_archive', type=str)
  command_line_args = parser.parse_args()

  wpr_backend = WprArchiveBackend(command_line_args.wpr_archive)
  url_entries = wpr_backend.ListUrlEntries()
  print url_entries[0].url
  wpr_backend.Persist()
