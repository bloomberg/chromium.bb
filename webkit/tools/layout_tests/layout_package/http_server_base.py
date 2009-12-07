# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class with common routines between the Apache and Lighttpd servers."""

import time

import google.httpd_utils

class HttpServerBase(object):
  def WaitForAction(self, action):
    """Repeat the action for 20 seconds or until it succeeds. Returns whether
    it succeeded."""
    succeeded = False

    start_time = time.time()
    while time.time() - start_time < 20 and not succeeded:
      time.sleep(0.1)
      succeeded = action()

    return succeeded

  def IsServerRunningOnAllPorts(self):
    """Returns whether the server is running on all the desired ports."""
    for mapping in self.mappings:
      if 'sslcert' in mapping:
        http_suffix = 's'
      else:
        http_suffix = ''

      url = 'http%s://127.0.0.1:%d/' % (http_suffix, mapping['port'])
      if not google.httpd_utils.UrlIsAlive(url):
        return False

    return True
