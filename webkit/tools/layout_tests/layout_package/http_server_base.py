# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class with common routines between the Apache and Lighttpd servers."""

import time
import urllib

class HttpServerBase(object):
  def WaitForAction(self, action):
    """Repeat the action for 20 seconds or until it succeeds. Returns whether
    it succeeded."""
    start_time = time.time()
    while time.time() - start_time < 20:
      if action():
        return True
      time.sleep(1)

    return False

  def IsServerRunningOnAllPorts(self):
    """Returns whether the server is running on all the desired ports."""
    for mapping in self.mappings:
      if 'sslcert' in mapping:
        http_suffix = 's'
      else:
        http_suffix = ''

      url = 'http%s://127.0.0.1:%d/' % (http_suffix, mapping['port'])

      try:
        response = urllib.urlopen(url)
        # Server is up and responding.
      except IOError:
        return False

    return True
