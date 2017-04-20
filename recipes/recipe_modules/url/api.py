# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

import urllib

class UrlApi(recipe_api.RecipeApi):
  quote = staticmethod(urllib.quote)
  urlencode = staticmethod(urllib.urlencode)

  @recipe_api.non_step
  def join(self, *parts):
    return '/'.join(str(x).strip('/') for x in parts)

  def fetch(self, url, step_name=None, attempts=None, headers=None, **kwargs):
    """Fetches data at given URL and returns it as str.

    Args:
      url: URL to fetch.
      step_name: optional step name, 'fetch <url>' by default.
      attempts: how many attempts to make (1 by default).
      headers: a {header_name: value} dictionary for HTTP headers.

    Returns:
      Fetched data as string.
    """
    fetch_result = self.fetch_to_file(
        url, None, step_name=step_name, attempts=attempts, headers=headers,
        **kwargs)
    return fetch_result.raw_io.output_text

  def fetch_to_file(self, url, path, step_name=None, attempts=None,
                    headers=None, **kwargs):
    """Fetches data at given URL and writes it to file.

    Args:
      url: URL to fetch.
      path: path to write the fetched file to. You are responsible for deleting
            it later if not needed.
      step_name: optional step name, 'fetch <url>' by default.
      attempts: how many attempts to make (1 by default).
      headers: a {header_name: value} dictionary for HTTP headers.

    Returns:
      Step.
    """
    if not step_name:
      step_name = 'fetch %s' % url
    args = [
        url,
        '--outfile', self.m.raw_io.output_text(leak_to=path),
    ]

    if headers:
      args.extend(['--headers-json', self.m.json.input(headers)])

    if attempts:
      args.extend(['--attempts', attempts])
    return self.m.python(
        name=step_name,
        script=self.resource('pycurl.py'),
        args=args,
        **kwargs)
