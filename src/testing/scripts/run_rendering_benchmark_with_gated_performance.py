#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs telemetry benchmarks on representative story tag.

This script is a wrapper around run_performance_tests.py to capture the
values of performance metrics and compare them with the acceptable limits
in order to prevent regressions.

Arguments used for this script are the same as run_performance_tests.py.

The name and some functionalities of this script should be adjusted for
use with other benchmarks.
"""

import argparse
import csv
import json
import os
import sys
import time

import common
import run_performance_tests

BENCHMARK = 'rendering.desktop'
# AVG_ERROR_MARGIN determines how much more the value of frame times can be
# compared to the recorded value
AVG_ERROR_MARGIN = 2.0
# CI stands for comfidence intrevals. "ci_095"s recorded in the data is the
# reocrded range between upper and lower CIs. CI_ERROR_MARGIN is the maximum
# acceptable ratio of calculated ci_095 to the recorded ones.
CI_ERROR_MARGIN = 2.0

class ResultRecorder(object):
  def __init__(self):
    self.fails = 0
    self.tests = 0
    self.start_time = time.time()
    self.output = {}
    self.return_code = 0

  def setTests(self, output, testNum):
    self.output = output
    self.tests = testNum
    self.fails = 0
    if 'FAIL' in output['num_failures_by_type']:
      self.fails = output['num_failures_by_type']['FAIL']

  def addFailure(self, name):
    self.output['tests'][BENCHMARK][name]['actual'] = 'FAIL'
    self.output['tests'][BENCHMARK][name]['is_unexpected'] = True
    self.fails += 1

  def getOutput(self, return_code):
    self.output['seconds_since_epoch'] = time.time() - self.start_time
    self.output['num_failures_by_type']['PASS'] = self.tests - self.fails
    if self.fails > 0:
      self.output['num_failures_by_type']['FAIL'] = self.fails
    if return_code == 1:
      self.output["interrupted"] = True

    if self.fails == 0:
      print "[  PASSED  ] " + str(self.tests) + " tests."
    else:
      print "[  FAILED  ] " + str(self.fails) + "/" + str(self.tests)+ " tests."
      self.return_code = 1

    return (self.output, self.return_code)

  def setAllToFail(self):
    self.fails = self.tests

def main():
  overall_return_code = 0

  # Linux does not have it's own specific representatives
  # and uses the representatives chosen for winodws.
  if sys.platform == 'win32':
    platform = 'win'
    story_tag = 'representative_win_desktop'
  elif sys.platform == 'darwin':
    platform = 'mac'
    story_tag = 'representative_mac_desktop'
  else:
    return 1

  options = parse_arguments()
  args = sys.argv
  args.extend(['--story-tag-filter', story_tag])

  overall_return_code = run_performance_tests.main(args)
  result_recorder = ResultRecorder()

  # The values used as the upper limit are the 99th percentile of the
  # avg and ci_095 frame_times recorded by dashboard in the past 200 revisions.
  # If the value measured here would be higher than this value at least by
  # 2ms [AVG_ERROR_MARGIN], that would be considered a failure.
  # crbug.com/953895
  with open(
    os.path.join(os.path.dirname(__file__),
    'representative_perf_test_data',
    'representatives_frame_times_upper_limit.json')
  ) as bound_data:
    upper_limit_data = json.load(bound_data)

  out_dir_path = os.path.dirname(options.isolated_script_test_output)
  test_count = len(upper_limit_data[platform])

  output_path = os.path.join(out_dir_path, BENCHMARK, 'test_results.json')

  with open(output_path, 'r+') as resultsFile:
    initialOut = json.load(resultsFile)
    result_recorder.setTests(initialOut, test_count)

    results_path = os.path.join(out_dir_path, BENCHMARK, 'perf_results.csv')
    marked_stories = set()
    with open(results_path) as csv_file:
      reader = csv.DictReader(csv_file)
      for row in reader:
        # For now only frame_times is used for testing representatives'
        # performance.
        if row['name'] != 'frame_times':
          continue
        story_name = row['stories']
        if (story_name in marked_stories or story_name not in
          upper_limit_data[platform]):
          continue
        marked_stories.add(story_name)

        if row['avg'] == '' or row['count'] == 0:
          print "No values for " + story_name
          result_recorder.addFailure(story_name)
        elif (float(row['ci_095']) >
          upper_limit_data[platform][story_name]['ci_095'] * CI_ERROR_MARGIN):
          print "Noisy data on frame_times for " + story_name + ".\n"
          result_recorder.addFailure(story_name)
        elif (float(row['avg']) >
          upper_limit_data[platform][story_name]['avg'] + AVG_ERROR_MARGIN):
          print (story_name + ": average frame_times is higher than 99th " +
            "percentile of the past 200 recorded frame_times(" +
            row['avg'] + ")" + ".\n")
          result_recorder.addFailure(story_name)

    (
      finalOut,
      overall_return_code
    ) = result_recorder.getOutput(overall_return_code)

    # Clearing the result of run_benchmark and write the gated perf results
    resultsFile.seek(0)
    resultsFile.truncate(0)
    json.dump(finalOut, resultsFile, indent=4)

    with open(options.isolated_script_test_output, 'w') as outputFile:
      json.dump(finalOut, outputFile, indent=4)

  return overall_return_code

def parse_arguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('executable', help='The name of the executable to run.')
  parser.add_argument(
      '--isolated-script-test-output', required=True)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)
  return parser.parse_known_args()[0]

def main_compile_targets(args):
  json.dump([], args.output)

if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())