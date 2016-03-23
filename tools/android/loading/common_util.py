# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time


def PollFor(condition, condition_name, interval=5):
  """Polls for a function to return true.

  Args:
    condition: Function to wait its return to be True.
    condition_name: The condition's name used for logging.
    interval: Periods to wait between tries in seconds.

  Returns:
    What condition has returned to stop waiting.
  """
  while True:
    result = condition()
    logging.info('Polling condition %s is %s' % (
        condition_name, 'met' if result else 'not met'))
    if result:
      return result
    time.sleep(interval)
