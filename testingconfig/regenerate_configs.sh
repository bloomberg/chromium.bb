#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerates the testingconfig JSON outputs.
#
# This assumes that you have depot_tools on your PATH, as lucicfg is contained
# in depot_tools.

script_dir="$(dirname "${BASH_SOURCE[*]}")/"

config_files="$(find "${script_dir}" -name '*config.star')"

for f in ${config_files}; do
  lucicfg generate "${f}"
done
