# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides the web interface to query the status of a buildbucket job."""
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import json
import time

from dashboard.common import request_handler
from dashboard.services import buildbucket_service
from dashboard.services import request
from dashboard.common import utils


class BuildbucketJobStatusHandler(request_handler.RequestHandler):
  """Handler for requests of the form /buildbucket_job_status/01234567...

  This displays information regarding the status of the buildbucket job in a
  human-readable format.
  """
  def get(self, job_id):
    if utils.IsRunningBuildBucketV2():
      self.GetV2(job_id)
    else:
      self.GetV1(job_id)

  def GetV1(self, job_id):
    original_status = buildbucket_service.GetJobStatus(job_id)

    error = 'error' in original_status

    if error:
      error_reason = original_status['error'].get('reason')
      status_text = json.dumps(original_status, sort_keys=True, indent=4)
    else:
      clean_status = _ConvertTimes(_ParseJsonKeys(original_status.get('build')))
      status_text = json.dumps(clean_status, sort_keys=True, indent=4)

    self.RenderHtml(
        'buildbucket_job_status.html', {
            'job_id': job_id,
            'status_text': 'DATA:' + status_text,
            'build': None if error else clean_status,
            'error': error_reason if error else None,
            'original_response': original_status,
        })

  def GetV2(self, job_id):
    error, error_code = False, None
    try:
      original_status = buildbucket_service.GetJobStatus(job_id)
      # The _ParseJsonKeys and _ConvertTimes should be no longer needed in
      # buildbucket V2 as no fields in the current proto has those suffixes.
      status_text = json.dumps(original_status, sort_keys=True, indent=4)
    except (request.NotFoundError, request.RequestError) as e:
      error = True
      original_status = e.content
      status_text = original_status
      error_code = e.headers.get('x-prpc-grpc-code', None)

    # In V2, the error_reason (e.g., BUILD_NOT_FOUND) is no longer part of the
    # response. We have a numeric value 'x-prpc-grpc-code' in the header.
    self.RenderHtml(
        'buildbucket_job_status.html', {
            'job_id': job_id,
            'status_text': 'DATA:' + status_text,
            'build': None if error else original_status,
            'error': ('gRPC code: %s' % error_code) if error else None,
            'original_response': original_status,
        })


def _ConvertTimes(dictionary):
  """Replaces all keys that end in '_ts' with human readable times.

  It seems from sample results that *_ts in the response are specified in
  microseconds since the UNIX epoch.

  Args:
    dictionary: A dictionary with the original data.

  Returns:
    A copy of the original dictionary with appropriate replacements.
  """
  result = dictionary.copy()

  for key in result:
    if key.endswith('_ts'):
      # We cast as float because the data comes as a string containing the
      # number of microseconds since the unix epoch.
      time_seconds = float(result[key]) / 1000000
      time_string = time.ctime(time_seconds)
      result[key.replace('_ts', '_utc')] = time_string
      result.pop(key)
  return result


def _ParseJsonKeys(dictionary):
  """Replaces values with json strings with objects parsed from them.

  Certain nested json objects are returned as strings. We parse them to access
  their properties. Note this method is not recursive and only parses objects
  in the topmost level, i.e. only string values in the given dictionary and
  not in nested dictionaries it might contain.

  Args:
    dictionary: A dictionary with the original data.

  Returns:
    A copy of the original dictionary with appropriate replacements.
  """
  result = dictionary.copy()

  for key in result:
    if key.endswith('_json'):
      result[key.replace('_json', '')] = json.loads(result[key])
      result.pop(key)
  return result
