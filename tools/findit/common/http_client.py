# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class HttpClient(object):
  """Represent a http client for sending request to a http[s] server.

  If cookies need to be sent, they should be in a file pointed to by
  COOKIE_FILE in the environment.
  """

  @staticmethod
  def Get(url, params={}, timeout=120, retries=5, retry_interval=0.5,
          retry_if_not=None):
    """Send a GET request to the given url with the given parameters.

    Args:
      url: the url to send request to.
      params: parameters to send as part of the http request.
      timeout: timeout for the http request, default is 120 seconds.
      retries: indicate how many retries before failing, default is 5.
      retry_interval: interval in second to wait before retry, default is 0.5.
      retry_if_not: a http status code. If set, retry only when the failed http
                    status code is a different value.

    Returns:
      (status_code, data)
      state_code: the http status code in the response.
      data: the body of the response.
    """
    raise NotImplemented()
