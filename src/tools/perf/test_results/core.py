# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import hashlib
import sys

from test_results import api
from test_results import frames


def CheckDependencies():
  """Check that module dependencies are satisfied, otherwise exit with error."""
  if frames.pandas is None:
    sys.exit('ERROR: This tool requires pandas to run, try: pip install pandas')


def GetBuilders():
  """Get the builders data frame and keep a cached copy."""
  def make_frame():
    data = api.GetBuilders()
    return frames.BuildersDataFrame(data)

  return frames.GetWithCache(
      'builders.pkl', make_frame, expires_after=datetime.timedelta(hours=12))


def GetTestResults(master, builder, test_type):
  """Get a test results data frame and keep a cached copy."""
  def make_frame():
    data = api.GetTestResults(master, builder, test_type)
    return frames.TestResultsDataFrame(data)

  basename = hashlib.md5('/'.join([master, builder, test_type])).hexdigest()
  return frames.GetWithCache(
      basename + '.pkl', make_frame, expires_after=datetime.timedelta(hours=3))
