# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic infrastructure for implementing retries.

This code is adopted from autotest: client/common_lib/cros/retry.py
This implementation removes the timeout feature as that requires the retry to
be done in main thread. For devserver, the call is handled in a thread kicked
off by cherrypy, so timeotu can't be supported.
"""

from __future__ import print_function

import random
import sys
import time

from chromite.lib.xbuddy import cherrypy_log_util

# Module-local log function.
def _Log(message, *args):
  return cherrypy_log_util.LogWithTag('RETRY', message, *args)


def retry(ExceptionToCheck, timeout_min=1.0, delay_sec=3, blacklist=None):
  """Retry calling the decorated function using a delay with jitter.

  Will raise RPC ValidationError exceptions from the decorated
  function without retrying; a malformed RPC isn't going to
  magically become good. Will raise exceptions in blacklist as well.

  original from:
    http://www.saltycrane.com/blog/2009/11/trying-out-retry-decorator-python/

  Args:
    ExceptionToCheck: the exception to check.  May be a tuple of exceptions to
                      check.
    timeout_min: timeout in minutes until giving up.
    delay_sec: pre-jittered delay between retries in seconds.  Actual delays
               will be centered around this value, ranging up to 50% off this
               midpoint.
    blacklist: a list of exceptions that will be raised without retrying
  """
  def deco_retry(func):
    random.seed()

    def delay():
      """'Jitter' the delay, up to 50% in either direction."""
      random_delay = random.uniform(.5 * delay_sec, 1.5 * delay_sec)
      _Log('Retrying in %f seconds...', random_delay)
      time.sleep(random_delay)

    def func_retry(*args, **kwargs):
      # Used to cache exception to be raised later.
      exc_info = None
      delayed_enabled = False
      exception_tuple = () if blacklist is None else tuple(blacklist)
      start_time = time.time()
      remaining_time = timeout_min * 60

      while remaining_time > 0:
        if delayed_enabled:
          delay()
        else:
          delayed_enabled = True
        try:
          # Clear the cache
          exc_info = None
          return func(*args, **kwargs)
        except exception_tuple:
          raise
        except ExceptionToCheck as e:
          _Log('%s(%s)', e.__class__, e)
          # Cache the exception to be raised later.
          exc_info = sys.exc_info()

          remaining_time = int(timeout_min*60 - (time.time() - start_time))

      # Raise the cached exception with original backtrace.
      raise exc_info[0](exc_info[1], exc_info[2])

    return func_retry  # true decorator
  return deco_retry
