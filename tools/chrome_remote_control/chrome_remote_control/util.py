# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import inspect
import time

_timeout = 60

class TimeoutException(Exception):
  pass

class TimeoutChanger(object):
  def __init__(self, new_timeout):
    self._timeout = new_timeout

  def __enter__(self):
    _timeout, self._timeout = self._timeout, _timeout
    return self

  def __exit__(self, *args):
    _timeout = self._timeout

def WaitFor(condition):
  assert isinstance(condition, type(lambda: None))  # is function
  start_time = time.time()
  while not condition():
    if time.time() - start_time > _timeout:
      if condition.__name__ == '<lambda>':
        condition_string = inspect.getsource(condition).strip()
      else:
        condition_string = condition.__name__
      raise TimeoutException('Timed out while waiting %ds for %s.' %
                             (_timeout, condition_string))
    time.sleep(0.01)
