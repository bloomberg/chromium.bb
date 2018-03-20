#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import shutil
import sys
import tempfile

from core import oauth_api
from core import upload_results_to_perf_dashboard
from core import results_merger
from os import listdir
from os.path import isfile, join, basename


RESULTS_URL = 'https://chromeperf.appspot.com'

def _upload_perf_results(json_to_upload, name, configuration_name,
    build_properties, oauth_file, tmp_dir):
  """Upload the contents of result JSON(s) to the perf dashboard."""
  build_properties = json.loads(build_properties)
  if not configuration_name:
    # we are deprecating perf-id crbug.com/817823
    configuration_name = build_properties['buildername']
  args = [
      '--tmp-dir', tmp_dir,
      '--buildername', build_properties['buildername'],
      '--buildnumber', build_properties['buildnumber'],
      '--name', name,
      '--configuration-name', configuration_name,
      '--results-file', json_to_upload,
      '--results-url', RESULTS_URL,
      '--got-revision-cp', build_properties['got_revision_cp'],
      '--got-v8-revision', build_properties['got_v8_revision'],
      '--got-webrtc-revision', build_properties['got_webrtc_revision'],
      '--oauth-token-file', oauth_file,
  ]
  if _is_histogram(json_to_upload):
    args.append('--send-as-histograms')

  upload_results_to_perf_dashboard.main(args)


def _is_histogram(json_file):
  with open(json_file) as f:
    data = json.load(f)
    return isinstance(data, list)
  return False


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
                          service_account_file,
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

  # We need to keep track of disabled benchmarks so we don't try to
  # upload the results.
  test_results_list = []
  tmpfile_dir = tempfile.mkdtemp('resultscache')
  try:
    with oauth_api.with_access_token(service_account_file) as oauth_file:
      for directory in benchmark_directory_list:
        # Obtain the test name we are running
        benchmark_name = basename(directory)
        disabled = False
        with open(join(directory, 'test_results.json')) as json_data:
          json_results = json.load(json_data)
          if not json_results:
            # Output is null meaning the test didn't produce any results.
            # Want to output an error and continue loading the rest of the
            # test results.
            print 'No results produced for %s, skipping upload' % directory
            continue
          if json_results.get('version') == 3:
            # Non-telemetry tests don't have written json results but
            # if they are executing then they are enabled and will generate
            # chartjson results.
            if not bool(json_results.get('tests')):
              disabled = True
          if not '.reference' in directory:
            # We don't need to upload reference build data to the
            # flakiness dashboard since we don't monitor the ref build
            test_results_list.append(json_results)
        if disabled:
          # We don't upload disabled benchmarks
          print 'Benchmark %s disabled' % benchmark_name
          continue
        print 'Uploading perf results from %s benchmark' % benchmark_name
        _upload_perf_results(join(directory, 'perf_results.json'),
            benchmark_name, configuration_name, build_properties,
            oauth_file, tmpfile_dir)
      _merge_json_output(output_json, test_results_list)
  finally:
    shutil.rmtree(tmpfile_dir)
  return 0


def main():
  """ See collect_task.collect_task for more on the merge script API. """
  parser = argparse.ArgumentParser()
  # configuration-name (previously perf-id) is the name of bot the tests run on
  # For example, buildbot-test is the name of the obbs_fyi bot
  # configuration-name and results-url are set in the json file which is going
  # away tools/perf/core/chromium.perf.fyi.extras.json
  parser.add_argument('--configuration-name', help=argparse.SUPPRESS)
  parser.add_argument('--service-account-file', help=argparse.SUPPRESS)

  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('-o', '--output-json', required=True,
                      help=argparse.SUPPRESS)
  parser.add_argument('json_files', nargs='*', help=argparse.SUPPRESS)

  args = parser.parse_args()

  return _process_perf_results(
      args.output_json, args.configuration_name,
      args.service_account_file,
      args.build_properties, args.task_output_dir)


if __name__ == '__main__':
  sys.exit(main())
