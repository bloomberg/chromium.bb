# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from gpu_tests import path_util

path_util.SetupTypPath()
path_util.SetupTelemetryPaths()

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'testing'))
