# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock

from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.common import targets


def MockMonitor():  # pragma: no cover
  return mock.MagicMock(monitors.Monitor)


def MockTarget():  # pragma: no cover
  return mock.MagicMock(targets.Target)
