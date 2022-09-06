# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import datetime
import logging

from google.appengine.api.taskqueue import TaskRetryOptions
from google.appengine.ext import deferred

from dashboard.common import layered_cache
from dashboard.common import utils
from dashboard.pinpoint.models import errors
from dashboard.pinpoint.models import job as job_module

_JOB_CACHE_KEY = 'pinpoint_refresh_jobs_%s'
_JOB_MAX_RETRIES = 5
_JOB_FROZEN_THRESHOLD = datetime.timedelta(hours=1)

if utils.IsRunningFlask():
  from flask import make_response

  def RefreshJobsHandler():
    _FindAndRestartJobs()
    return make_response('', 200)
else:
  import webapp2

  class RefreshJobs(webapp2.RequestHandler):

    def get(self):
      _FindAndRestartJobs()


def _FindFrozenJobs():
  jobs = job_module.Job.query(job_module.Job.running == True).fetch()
  now = datetime.datetime.utcnow()

  def _IsFrozen(j):
    time_elapsed = now - j.updated
    return time_elapsed >= _JOB_FROZEN_THRESHOLD

  results = [j for j in jobs if _IsFrozen(j)]
  logging.debug('frozen jobs = %r', [j.job_id for j in results])
  return results


def _FindAndRestartJobs():
  jobs = _FindFrozenJobs()
  opts = TaskRetryOptions(task_retry_limit=1)

  for j in jobs:
    deferred.defer(_ProcessFrozenJob, j.job_id, _retry_options=opts)


def _ProcessFrozenJob(job_id):
  job = job_module.JobFromId(job_id)
  key = _JOB_CACHE_KEY % job_id
  info = layered_cache.Get(key) or {'retries': 0}

  retries = info.setdefault('retries', 0)
  if retries >= _JOB_MAX_RETRIES:
    info['retries'] += 1
    layered_cache.Set(key, info, days_to_keep=30)
    job.Fail(errors.JobRetryFailed())
    logging.error('Retry #%d: failed retrying job %s', retries, job_id)
    return

  info['retries'] += 1
  layered_cache.Set(key, info, days_to_keep=30)

  logging.info('Restarting job %s', job_id)
  job._Schedule()
  job.put()
