#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys

from core import upload_results_to_perf_dashboard
from core import results_merger
from os import listdir
from os.path import isfile, join


RESULTS_URL = 'https://chromeperf.appspot.com'

def _upload_perf_results(json_to_upload, name, configuration_name,
    build_properties):
  """Upload the contents of result JSON(s) to the perf dashboard."""
  build_properties = json.loads(build_properties)
  args = [
      '--build-dir', '/b/c/b/obbs_fyi/src/out',
      '--buildername', build_properties['buildername'],
      '--buildnumber', build_properties['buildnumber'],
      '--name', name,
      '--configuration-name', configuration_name,
      '--results-file', json_to_upload,
      '--results-url', RESULTS_URL,
      '--got-revision-cp', build_properties['got_revision_cp'],
      '--got-v8-revision', build_properties['got_v8_revision'],
      '--got-webrtc-revision', build_properties['got_webrtc_revision'],
      '--chromium-checkout-dir', '/b/c/b/obbs_fyi',
      '--send-as-histograms',
  ]
  upload_results_to_perf_dashboard.main(args)


def _merge_json_output(output_json, jsons_to_merge):
  """Merges the contents of one or more results JSONs.

  Args:
    output_json: A path to a JSON file to which the merged results should be
      written.
    jsons_to_merge: A list of JSON files that should be merged.
  """
  merged_results = results_merger.merge_test_results(jsons_to_merge)

  with open(output_json, 'w') as f:
    json.dump(merged_results, f)

  return 0


def _process_perf_results(output_json, configuration_name,
                          build_properties, task_output_dir):
  """Process one or more perf JSON results.

  Consists of merging the json-test-format output and uploading the perf test
  output (chartjson and histogram).

  Each directory in the task_output_dir represents one benchmark
  that was run. Within this directory, there is a subdirectory with the name
  of the benchmark that was run. In that subdirectory, there is a
  perftest-output.json file containing the performance results in histogram
  or dashboard json format and an output.json file containing the json test
  results for the benchmark.
  """
  directory_list = [
      f for f in listdir(task_output_dir)
      if not isfile(join(task_output_dir, f))
  ]
  benchmark_directory_list = []
  for directory in directory_list:
    benchmark_directory_list += [
      join(task_output_dir, directory, f)
      for f in listdir(join(task_output_dir, directory))
    ]

  test_results_list = []
  for directory in benchmark_directory_list:
    with open(join(directory, 'test_results.json')) as json_data:
      test_results_list.append(json.load(json_data))
  _merge_json_output(output_json, test_results_list)

  for directory in benchmark_directory_list:
    _upload_perf_results(join(directory, 'perf_results.json'),
        directory, configuration_name, build_properties)

  return 0


def main():
  """ See collect_task.collect_task for more on the merge script API. """
  parser = argparse.ArgumentParser()
  # configuration-name (previously perf-id) is the name of bot the tests run on
  # For example, buildbot-test is the name of the obbs_fyi bot
  # configuration-name and results-url are set in the json file which is going
  # away tools/perf/core/chromium.perf.fyi.extras.json
  parser.add_argument('--configuration-name', help=argparse.SUPPRESS)

  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('-o', '--output-json', required=True,
                      help=argparse.SUPPRESS)
  parser.add_argument('json_files', nargs='*', help=argparse.SUPPRESS)

  args = parser.parse_args()

  return _process_perf_results(
      args.output_json, args.configuration_name,
      args.build_properties, args.task_output_dir)


if __name__ == '__main__':
  sys.exit(main())
