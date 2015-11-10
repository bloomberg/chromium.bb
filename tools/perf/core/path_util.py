# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def GetChromiumSrcDir():
  return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))))

def GetTelemetryDir():
  return os.path.join(GetChromiumSrcDir(), 'tools', 'telemetry')

def GetPerfDir():
  return os.path.join(GetChromiumSrcDir(), 'tools', 'perf')

def GetPerfStorySetsDir():
  return os.path.join(GetPerfDir(), 'page_sets')

def GetPerfBenchmarksDir():
  return os.path.join(GetPerfDir(), 'benchmarks')

