#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge results from code coverage swarming runs.

This script merges code coverage profiles from multiple shards. It also merges
the test results of the shards.

It is functionally similar to merge_steps.py but it accepts the parameters
passed by swarming api.
"""

import argparse
import json
import logging
import os
import subprocess
import sys

import merge_lib as coverage_merger


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
  parser.add_argument('jsons_to_merge', nargs='*', help=argparse.SUPPRESS)

  # Custom arguments for this merge script.
  parser.add_argument(
      '--additional-merge-script', help='additional merge script to run')
  parser.add_argument(
      '--additional-merge-script-args',
      help='args for the additional merge script', action='append')
  parser.add_argument(
      '--profdata-dir', required=True, help='where to store the merged data')
  parser.add_argument(
      '--llvm-profdata', required=True, help='path to llvm-profdata executable')
  return parser


def main():
  desc = "Merge profraw files in <--task-output-dir> into a single profdata."
  parser = _MergeAPIArgumentParser(description=desc)
  params = parser.parse_args()

  logging.info('Merging %d test results', len(params.jsons_to_merge))

  failed = False

  if params.additional_merge_script:
    new_args = [
        '--build-properties', params.build_properties,
        '--summary-json', params.summary_json,
        '--task-output-dir', params.task_output_dir,
        '--output-json', params.output_json,
    ]
    if params.additional_merge_script_args:
      new_args += params.additional_merge_script_args

    new_args += params.jsons_to_merge

    rc = subprocess.call([
        sys.executable, params.additional_merge_script] + new_args)
    if rc != 0:
      failed = True
      logging.warning('Additional merge script %s exited with %s' % (
          params.additional_merge_script, p.returncode
      ))

  invalid_profiles = coverage_merger.merge_profiles(
      params.task_output_dir,
      os.path.join(params.profdata_dir, 'default.profdata'), '.profraw',
      params.llvm_profdata)
  if invalid_profiles:
    with open(os.path.join(params.profdata_dir, 'invalid_profiles.json'),
              'w') as f:
      json.dump(invalid_profiles, f)

  return 1 if (failed or bool(invalid_profiles)) else 0


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
