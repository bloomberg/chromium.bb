#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for running gpu integration tests on Fuchsia devices."""

import os
import shutil
import sys

import fuchsia_util
from gpu_tests import path_util


def main():
  gpu_script = [
      os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test', 'gpu',
                   'run_gpu_integration_test.py')
  ]
  return fuchsia_util.RunTestOnFuchsiaDevice(gpu_script)


if __name__ == '__main__':
  sys.exit(main())
