#!/usr/bin/env vpython
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import os
import requests
import time

# Constants describing TestStatus for ResultDB
STATUS_PASS = 'PASS'
STATUS_FAIL = 'FAIL'
STATUS_CRASH = 'CRASH'
STATUS_ABORT = 'ABORT'
STATUS_SKIP = 'SKIP'

class ResultSinkStatus(object):
  def __init__(self):
    self.status = STATUS_PASS

@contextlib.contextmanager
def setup_rdb(function_name, prefix):
  """Context Manager function for ResultDB reporting.

  Args:
    function_name (str): The name of the function we are about to run.
    prefix (str): The prefix for the name of the test. The format for this is
        presubmit:gerrit_host/folder/to/repo:path/to/file/
      for example,
        presubmit:chromium-review.googlesource.com/chromium/src/:services/viz/
  """
  sink = None
  if 'LUCI_CONTEXT' in os.environ:
    with open(os.environ['LUCI_CONTEXT']) as f:
      j = json.load(f)
      if 'result_sink' in j:
        sink = j['result_sink']

  my_status = ResultSinkStatus()
  start_time = time.time()
  try:
    yield my_status
  except Exception:
    my_status.status = STATUS_FAIL
    raise
  finally:
    end_time = time.time()
    elapsed_time = end_time - start_time
    if sink != None:
      tr = {
          'testId': '{0}:{1}'.format(prefix, function_name),
          'status': my_status.status,
          'expected': (my_status.status == STATUS_PASS),
          'duration': '{:.9f}s'.format(elapsed_time)
      }
      requests.post(
          url='http://{0}/prpc/luci.resultsink.v1.Sink/ReportTestResults'
                  .format(sink['address']),
          headers={
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            'Authorization': 'ResultSink {0}'.format(sink['auth_token'])
          },
          data=json.dumps({'testResults': [tr]})
    )
