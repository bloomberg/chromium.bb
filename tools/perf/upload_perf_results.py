#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys


def UploadJson(output_json, jsons_to_upload, debug=True):
  """Upload the contents of one or more results JSONs to the perf dashboard.

  Args:
    jsons_to_upload: A list of paths to JSON files that should be uploaded.
  """
  shard_output_list = []
  shard_perf_results_list = []
  for file_name in jsons_to_upload:
    with open(file_name) as f:
      shard_output_list.append(json.load(f))

    # Temporary name change to grab the perftest files instead of the json
    # test results files. Will be removed as soon as we pass in the correct
    # paths from the infra side.
    file_name = file_name.replace('output.json', 'perftest-output.json')

    with open(file_name) as f:
      shard_perf_results_list.append(json.load(f))

  if debug:
    print "Outputing the shard_results_list:"
    print shard_perf_results_list

  merged_results = shard_output_list[0]

  with open(output_json, 'w') as f:
      json.dump(merged_results, f)

  return 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', '--output-json', required=True)
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--verbose', help=argparse.SUPPRESS)
  parser.add_argument('jsons_to_merge', nargs='*')

  args = parser.parse_args()
  return UploadJson(args.output_json, args.jsons_to_merge)


if __name__ == '__main__':
  sys.exit(main())
