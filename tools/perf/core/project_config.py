# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))
TOP_LEVEL_DIR = os.path.normpath(os.path.join(THIS_DIR, os.pardir))
TELEMETRY_DIR = os.path.normpath(os.path.join(
    TOP_LEVEL_DIR, os.pardir, 'telemetry'))

sys.path.append(TELEMETRY_DIR)
from telemetry import benchmark_runner

binary_dependencies_file = os.path.join(THIS_DIR, 'binary_dependencies.json')

config = benchmark_runner.ProjectConfig(
    top_level_dir=TOP_LEVEL_DIR,
    benchmark_dirs=[os.path.join(TOP_LEVEL_DIR, 'benchmarks')],
    client_config=binary_dependencies_file)

config.telemetry_dir = TELEMETRY_DIR

