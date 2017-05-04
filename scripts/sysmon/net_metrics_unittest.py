# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_metrics."""

from __future__ import absolute_import
from __future__ import print_function
from __future__ import unicode_literals

from chromite.lib import cros_test_lib
from chromite.scripts.sysmon import net_metrics


class TestNetMetrics(cros_test_lib.TestCase):
  """Tests for network metrics."""

  def test_collect_net_info(self):
    """Tests that collecting the git hash doesn't have type conflicts."""
    # This has the side-effect of checking the types are correct.
    net_metrics.collect_net_info()
