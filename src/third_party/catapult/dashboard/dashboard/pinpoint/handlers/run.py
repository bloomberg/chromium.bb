# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import webapp2

from dashboard.pinpoint.models import job


class Run(webapp2.RequestHandler):
  """Handler that runs a Pinpoint job."""

  def post(self, job_id):
    job.JobFromId(job_id).Run()
