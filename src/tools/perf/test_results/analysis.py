# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch


def FilterBy(df, **kwargs):
  """Filter out a data frame by applying patterns to columns.

  Args:
    df: A data frame to filter.
    **kwargs: Remaining keyword arguments are interpreted as column=pattern
      specifications. The pattern may contain shell-style wildcards, only rows
      whose value in the specified column matches the pattern will be kept in
      the result. If the pattern is None, no filtering is done.

  Returns:
    The filtered data frame (a view on the original data frame).
  """
  for column, pattern in kwargs.iteritems():
    if pattern is not None:
      df = df[df[column].str.match(fnmatch.translate(pattern), case=False)]
  return df
