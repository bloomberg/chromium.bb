#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script merges code coverage profiles from multiple shards.

It is functionally identical to merge_steps.py but it accepts the parameters
passed by swarming api.
"""

import argparse
import json
import logging
import os
import sys

import merge_lib as merger


def _MergeAPIArgumentParser(*args, **kwargs):
  """Parameters passed to this merge script, as per:
  https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/swarming/resources/merge_api.py
  """
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument(
      '-o', '--output-json', required=True, help=argparse.SUPPRESS)
  parser.add_argument(
      '--profdata-dir', required=True, help='where to store the merged data')
  parser.add_argument(
      '--llvm-profdata', required=True, help='path to llvm-profdata executable')
  parser.add_argument('jsons_to_merge', nargs='*', help=argparse.SUPPRESS)
  return parser


def main():
  desc = "Merge profraw files in <--task-output-dir> into a single profdata."
  parser = _MergeAPIArgumentParser(description=desc)
  params = parser.parse_args()
  invalid_profiles = merger.merge_profiles(
      params.task_output_dir,
      os.path.join(params.profdata_dir, 'default.profdata'), '.profraw',
      params.llvm_profdata)
  if invalid_profiles:
    with open(os.path.join(params.profdata_dir, 'invalid_profiles.json'),
              'w') as f:
      json.dump(invalid_profiles, f)

  # TODO(crbug.com/921300) This script doesn't know how to merge test results,
  # and the correct solution should be taking other merge script as inputs to
  # perform the merge.
  # However, to work around the issue that fuzzer test steps are red, following
  # logic directly copy paste the output json if there is only one shard, and
  # this strategy should work for test targets that only have one shard, such
  # as fuzzer targets and simple gtests targets.
  if len(params.jsons_to_merge) == 1:
    with open(params.jsons_to_merge[0]) as f_read:
      with open(params.output_json, 'w') as f_write:
        f_write.write(f_read.read())


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
