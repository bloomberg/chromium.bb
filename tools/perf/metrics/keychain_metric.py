# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from metrics import Metric
from telemetry.value import histogram_util
from telemetry.value import scalar


class KeychainMetric(Metric):
  """KeychainMetric gathers keychain statistics from the browser object.

  This includes the number of times that the keychain was accessed.
  """

  DISPLAY_NAME = 'OSX_Keychain_Access'
  HISTOGRAM_NAME = 'OSX.Keychain.Access'

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    """Adds a browser argument that allows for the collection of keychain
    metrics. Has no effect on non-Mac platforms."""
    if sys.platform != 'darwin':
      return

    options.AppendExtraBrowserArgs(['--enable-stats-collection-bindings'])

  def AddResults(self, tab, results):
    """Adds the number of times that the keychain was accessed to |results|.
    Has no effect on non-Mac platforms."""
    if sys.platform != 'darwin':
      return

    access_count = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM, KeychainMetric.HISTOGRAM_NAME, tab)
    results.AddValue(scalar.ScalarValue(
        results.current_page, KeychainMetric.DISPLAY_NAME, 'count',
        access_count))
