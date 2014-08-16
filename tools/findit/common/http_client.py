# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class HttpClient(object):
  """Represent a http client for sending request to a http[s] server.

  If cookies need to be sent, they should be in a file pointed to by
  COOKIE_FILE in the environment.
  """

  @staticmethod
  def Get(url, params={}, timeout=None):
    """Send a GET request to the given url with the given parameters.

    Returns:
      (status_code, data)
      state_code: the http status code in the response.
      data: the body of the response.
    """
    raise NotImplemented()
