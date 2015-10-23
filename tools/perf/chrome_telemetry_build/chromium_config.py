# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

CHROMIUM_SRC_DIR = os.path.join(
    os.path.dirname(__file__), os.path.pardir, os.path.pardir, os.path.pardir)

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'tools', 'telemetry'))

from telemetry import project_config


CLIENT_CONFIG_PATH = os.path.join(
    os.path.dirname(__file__), 'binary_dependencies.json')


class ChromiumConfig(project_config.ProjectConfig):
  def __init__(self, top_level_dir=None, benchmark_dirs=None,
               client_config=CLIENT_CONFIG_PATH,
               default_chrome_root=CHROMIUM_SRC_DIR):

    perf_dir = os.path.join(CHROMIUM_SRC_DIR, 'tools', 'perf')
    if not benchmark_dirs:
      benchmark_dirs = [os.path.join(perf_dir, 'benchmarks')]
      logging.info('No benchmark directories specified. Defaulting to %s',
                   benchmark_dirs)
    if not top_level_dir:
      top_level_dir = perf_dir
      logging.info('No top level directory specified. Defaulting to %s',
                   top_level_dir)

    super(ChromiumConfig, self).__init__(
        top_level_dir=top_level_dir, benchmark_dirs=benchmark_dirs,
        client_config=client_config, default_chrome_root=default_chrome_root)
