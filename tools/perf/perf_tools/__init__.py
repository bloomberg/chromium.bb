# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library for Chrome GPU test code."""

import os
import sys


def _Init():
  telemetry_path = os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, 'telemetry')
  absolute_telemetry_path = os.path.abspath(telemetry_path)
  sys.path.append(absolute_telemetry_path)
  telemetry_tools_path = os.path.join(os.path.dirname(__file__),
                                      os.pardir, os.pardir, 'telemetry_tools')
  absolute_telemetry_tools_path = os.path.abspath(telemetry_tools_path)
  sys.path.append(absolute_telemetry_tools_path)


_Init()

from telemetry import RemoveAllStalePycFiles
RemoveAllStalePycFiles(os.path.dirname(__file__))
