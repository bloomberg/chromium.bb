# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides the web interface for displaying an overview of jobs."""

import datetime
import json

import webapp2

from dashboard.pinpoint.models import job


_MAX_JOBS_TO_FETCH = 10000


# TODO: Generalize the Jobs handler to allow the user to choose what fields to
# include and how many Jobs to fertch.

class Stats(webapp2.RequestHandler):
  """Shows an overview of recent anomalies for perf sheriffing."""

  def get(self):
    self.response.out.write(json.dumps(_GetJobs()))


def _GetJobs():
  created_limit = datetime.datetime.now() - datetime.timedelta(days=28)
  query = job.Job.query(job.Job.created >= created_limit)
  jobs = query.fetch(limit=_MAX_JOBS_TO_FETCH)

  job_infos = []
  for j in jobs:
    job_infos.append({
        'comparison_mode': j.comparison_mode,
        'completed': j.completed,
        'created': j.created.isoformat(),
        'difference_count': j.difference_count,
        'failed': j.failed,
        'updated': j.updated.isoformat(),
    })

  return job_infos
