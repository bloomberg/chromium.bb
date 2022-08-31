# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides the web interface for displaying a results2 file."""
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import json
import logging

from dashboard.pinpoint.models import job as job_module
from dashboard.pinpoint.models import results2
from dashboard.common import utils

if utils.IsRunningFlask():
  from flask import make_response
else:
  import webapp2

if utils.IsRunningFlask():

  def Results2Handler(job_id):
    try:
      job = job_module.JobFromId(job_id)
      if not job:
        raise results2.Results2Error('Error: Unknown job %s' % job_id)

      if not job.completed:
        return make_response(json.dumps({'status': 'job-incomplete'}))

      url = results2.GetCachedResults2(job)
      if url:
        return make_response(
            json.dumps({
                'status': 'complete',
                'url': url,
                'updated': job.updated.isoformat(),
            }))

      if results2.ScheduleResults2Generation(job):
        return make_response(json.dumps({'status': 'pending'}))

      return make_response(json.dumps({'status': 'failed'}))

    except results2.Results2Error as e:
      return make_response(str(e), 400)
else:

  class Results2(webapp2.RequestHandler):
    """Shows an overview of recent anomalies for perf sheriffing."""

    def get(self, job_id):
      try:
        job = job_module.JobFromId(job_id)
        if not job:
          raise results2.Results2Error('Error: Unknown job %s' % job_id)

        if not job.completed:
          self.response.out.write(json.dumps({'status': 'job-incomplete'}))
          return

        url = results2.GetCachedResults2(job)
        if url:
          self.response.out.write(
              json.dumps({
                  'status': 'complete',
                  'url': url,
                  'updated': job.updated.isoformat(),
              }))
          return

        if results2.ScheduleResults2Generation(job):
          self.response.out.write(json.dumps({'status': 'pending'}))
          return

        self.response.out.write(json.dumps({'status': 'failed'}))

      except results2.Results2Error as e:
        self.response.set_status(400)
        self.response.out.write(str(e))


if utils.IsRunningFlask():

  def Results2GeneratorHandler(job_id):
    try:
      job = job_module.JobFromId(job_id)
      if not job:
        logging.debug('No job [%s]', job_id)
        raise results2.Results2Error('Error: Unknown job %s' % job_id)
      results2.GenerateResults2(job)
      return make_response('', 200)
    except results2.Results2Error as e:
      return make_response(str(e), 400)
else:

  class Results2Generator(webapp2.RequestHandler):
    """Creates a results2 file and streams it to cloud storage."""

    def post(self, job_id):
      try:
        job = job_module.JobFromId(job_id)
        if not job:
          logging.debug('No job [%s]', job_id)
          raise results2.Results2Error('Error: Unknown job %s' % job_id)
        results2.GenerateResults2(job)
      except results2.Results2Error as e:
        self.response.out.write(str(e))
