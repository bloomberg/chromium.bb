# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to convert json responses from test-results into data frames."""

try:
  import pandas
except ImportError:
  pandas = None


def BuildersDataFrame(data):
  def iter_rows():
    for master in data['masters']:
      for test_type, builders in master['tests'].iteritems():
        for builder_name in builders['builders']:
          yield master['name'], builder_name, test_type

  return pandas.DataFrame.from_records(
      iter_rows(), columns=('master', 'builder', 'test_type'))
