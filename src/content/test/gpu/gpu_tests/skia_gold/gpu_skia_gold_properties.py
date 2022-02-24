# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU implementation of //testing/skia_gold_common/skia_gold_properties.py."""

import subprocess
import sys

import gpu_path_util

from skia_gold_common import skia_gold_properties


class GpuSkiaGoldProperties(skia_gold_properties.SkiaGoldProperties):
  @staticmethod
  def _GetGitOriginMainHeadSha1():
    try:
      return subprocess.check_output(
          ['git', 'rev-parse', 'origin/main'],
          shell=_IsWin(),
          cwd=gpu_path_util.CHROMIUM_SRC_DIR).strip()
    except subprocess.CalledProcessError:
      return None


def _IsWin():
  return sys.platform == 'win32'
