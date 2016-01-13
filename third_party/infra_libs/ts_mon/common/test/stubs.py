# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock

from infra_libs.ts_mon.common import interface
from infra_libs.ts_mon.common import metric_store
from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.common import targets


class MockState(interface.State):  # pragma: no cover

  def __init__(self, store_ctor=None):
    if store_ctor is None:
      store_ctor = metric_store.InProcessMetricStore

    self.global_monitor = None
    self.target = None
    self.flush_mode = None
    self.flush_thread = None
    self.metrics = {}
    self.store = store_ctor(self)


def MockMonitor():  # pragma: no cover
  return mock.MagicMock(monitors.Monitor)


def MockTarget():  # pragma: no cover
  return mock.MagicMock(targets.Target)
