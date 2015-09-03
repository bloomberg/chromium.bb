# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'telemetry'))
from telemetry import benchmark_runner

top_level_dir = os.path.dirname(os.path.realpath(
    os.path.join(__file__, os.pardir)))

config = benchmark_runner.ProjectConfig(
    top_level_dir=top_level_dir,
    benchmark_dirs=[os.path.join(top_level_dir, 'benchmarks')])

config.telemetry_dir = os.path.realpath(os.path.join(
    top_level_dir, os.pardir, 'telemetry'))

