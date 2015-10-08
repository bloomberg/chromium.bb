# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from core import path_util

sys.path.append(path_util.GetTelemetryDir())
from telemetry import project_config


CLIENT_CONFIG_PATH = os.path.join(
    path_util.GetPerfDir(), 'core', 'binary_dependencies.json')


class ChromiumConfig(project_config.ProjectConfig):
  def __init__(self, top_level_dir=None, benchmark_dirs=None,
               client_config=CLIENT_CONFIG_PATH,
               chromium_src_dir=path_util.GetChromiumSrcDir()):

    if not benchmark_dirs:
      benchmark_dirs = [path_util.GetPerfBenchmarksDir()]
      logging.info('No benchmark directories specified. Defaulting to %s',
                   benchmark_dirs)
    if not top_level_dir:
      top_level_dir = path_util.GetPerfDir()
      logging.info('No top level directory specified. Defaulting to %s',
                   top_level_dir)

    super(ChromiumConfig, self).__init__(
        top_level_dir=top_level_dir, benchmark_dirs=benchmark_dirs,
        client_config=client_config)

    self._chromium_src_dir = chromium_src_dir

