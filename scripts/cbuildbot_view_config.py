# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for dumping build config contents."""

from __future__ import print_function

from chromite.cbuildbot import generate_chromeos_config


def main(_argv):
  config = generate_chromeos_config.GetConfig()
  print(config.SaveConfigToString())
