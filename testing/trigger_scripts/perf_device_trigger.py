#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming triggering script.

This script does custom swarming triggering logic, to enable device affinity
for our bots, while lumping all trigger calls under one logical step.

This script receives multiple machine configurations on the command line in the
form of quoted strings. These strings are JSON dictionaries that represent
entries in the "dimensions" array of the "swarming" dictionary in the
src/testing/buildbot JSON files.

Scripts inheriting must have roughly the same command line interface as
swarming.py trigger. It modifies it in the following ways:
 * Intercepts the dump-json argument, and creates its own by combining the
   results from each trigger call.
 * Scans through the multiple-trigger-configs dictionaries. For any key found,
   deletes that dimension from the originally triggered task's dimensions. This
   is what allows the Swarming dimensions to be replaced.  Must contain the
   dimension "id" for the perf device affinity use case.
 * On a per-shard basis, adds the Swarming dimensions chosen from the
   multiple-trigger-configs list to the dimensions for the shard.

This script is normally called from the swarming recipe module in tools/build.

"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

import base_test_triggerer

class PerfDeviceTriggerer(base_test_triggerer.BaseTestTriggerer):
  def __init__(self):
    super(PerfDeviceTriggerer, self).__init__()

  def append_additional_args(self, args):
    return args

  def select_config_indices(self, args, verbose):
    # For perf we want to trigger a job for every valid config since
    # each config represents exactly one bot in the perf swarming pool,
    selected_configs = []
    for i in xrange(args.shards):
      selected_configs.append(i)
    return selected_configs


def main():
  triggerer = PerfDeviceTriggerer()
  # Setup args for common contract of base class
  parser = triggerer.setup_parser_contract(
      argparse.ArgumentParser(description=__doc__))
  args, remaining = parser.parse_known_args()
  return triggerer.trigger_tasks(args, remaining)

if __name__ == '__main__':
  sys.exit(main())

