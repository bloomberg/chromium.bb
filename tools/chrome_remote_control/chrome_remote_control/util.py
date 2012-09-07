# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import inspect
import time

class TimeoutException(Exception):
  pass

def WaitFor(condition, timeout, poll_interval=0.1):
  assert isinstance(condition, type(lambda: None))  # is function
  start_time = time.time()
  while not condition():
    if time.time() - start_time > timeout:
      if condition.__name__ == '<lambda>':
        try:
          condition_string = inspect.getsource(condition).strip()
        except IOError:
          condition_string = condition.__name__
      else:
        condition_string = condition.__name__
      raise TimeoutException('Timed out while waiting %ds for %s.' %
                             (timeout, condition_string))
    time.sleep(poll_interval)
