# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.vr_benchmarks.desktop_runtimes import base_runtime


# pylint: disable=abstract-method
class _OpenVRRuntimeBase(base_runtime.DesktopRuntimeBase):
  """Base class for all OpenVR runtimes."""

  def GetFeatureName(self):
    return 'OpenVR'


class OpenVRRuntimeReal(_OpenVRRuntimeBase):
  """Class for using the real OpenVR runtime for desktop tests."""


class OpenVRRuntimeMock(_OpenVRRuntimeBase):
  """Class for using the mock OpenVR runtime for desktop tests."""
