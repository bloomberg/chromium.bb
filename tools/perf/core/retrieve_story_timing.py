#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import subprocess
import sys


QUERY_BY_BUILD_NUMBER = """
SELECT
  run.name AS name,
  SUM(run.times) AS duration
FROM
  [test-results-hrd:events.test_results]
WHERE
  buildbot_info.builder_name IN ({})
  AND buildbot_info.build_number = {}
GROUP BY
  name
ORDER BY
  name
"""


QUERY_LAST_RUNS = """
SELECT
  name,
  ROUND(AVG(time)) AS duration,
FROM (
  SELECT
    run.name AS name,
    start_time,
    AVG(run.times) AS time
  FROM
    [test-results-hrd:events.test_results]
  WHERE
    buildbot_info.builder_name IN ({configuration_names})
    AND run.time IS NOT NULL
    AND run.time != 0
    AND run.is_unexpected IS FALSE
    AND DATEDIFF(CURRENT_DATE(), DATE(start_time)) < {num_last_days}
  GROUP BY
    name,
    start_time
  ORDER BY
    start_time DESC)
GROUP BY
  name
ORDER BY
  name
"""


def _run_query(query):
  args = ["bq", "query", "--format=json", "--max_rows=100000", query]

  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode == 0:
    return json.loads(stdout)
  else:
    raise RuntimeError(
        'Error generating authentication token.\nStdout: %s\nStder:%s' %
        (stdout, stderr))


def FetchStoryTimingDataForSingleBuild(configurations, build_number):
  return _run_query(QUERY_BY_BUILD_NUMBER.format(
      configurations, build_number))


def FetchAverageStortyTimingData(configurations, num_last_days):
  return _run_query(QUERY_LAST_RUNS.format(
      configuration_names=configurations, num_last_days=num_last_days))


def main(args):
  """
    To run this script, you need to be able to run bigquery in your terminal.
    If this is the first time you run the script, do the following steps:

    1) Follow the steps at https://cloud.google.com/sdk/docs/ to download and
       unpack google-cloud-sdk in your home directory.
    2) Run `glcoud auth login`
    3) Run `gcloud config set project test-results-hrd`
       3a) If 'test-results-hrd' does not show up, contact seanmccullough@
           to be added as a user of the table
    4) Run this script!
  """
  parser = argparse.ArgumentParser(
      description='Retrieve story timing from bigquery.')
  parser.add_argument(
      '--output-file', action='store', required=True,
      help='The filename to send the bigquery results to.')
  parser.add_argument(
      '--configurations', '-c', action='append', required=True,
      help='The configuration(s) of the builder to query results from.')
  parser.add_argument(
      '--build-number', action='store',
      help='If specified, the build number to get timing data from.')
  opts = parser.parse_args(args)

  configurations = str(opts.configurations).strip('[]')
  if opts.build_number:
    data = FetchStoryTimingDataForSingleBuild(configurations,
        opts.build_number)
  else:
    data = FetchAverageStortyTimingData(configurations, num_last_days=5)

  with open(opts.output_file, 'w') as output_file:
    json.dump(data, output_file, indent = 4, separators=(',', ': '))

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
