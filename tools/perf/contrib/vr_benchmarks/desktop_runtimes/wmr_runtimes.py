# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.vr_benchmarks.desktop_runtimes import base_runtime


# pylint: disable=abstract-method
class _WMRRuntimeBase(base_runtime.DesktopRuntimeBase):
  """Base class for all WMR runtimes."""

  def GetFeatureName(self):
    return 'WindowsMixedReality'


class WMRRuntimeReal(_WMRRuntimeBase):
  """Class for using the real Windows Mixed Reality runtime for desktop tests.
  """


class WMRRuntimeMock(_WMRRuntimeBase):
  """Class for using the mock Windows Mixed Reality runtime for desktop tests.
  """
