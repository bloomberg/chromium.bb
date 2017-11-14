#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs several telemetry benchmarks.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format detailed here:
https://www.chromium.org/developers/the-json-test-results-format

Optional argument:

  --isolated-script-test-filter-file=[FILENAME]

points to a file containing newline-separated test names, to run just
that subset of tests. This gets remapped to the command line argument
--file-list.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.

It currently runs several benchmarks. The benchmarks it will execute are
determined by the --bot and sharding map location (see sharding_map_path()).

The results of running the benchmark are put in separate directories per
benchmark. Two files will be present in each directory; perf_results.json, which
is the perf specific results (with unenforced format, could be histogram,
legacy, or chartjson), and test_results.json, which is a JSON test results
format file
(https://www.chromium.org/developers/the-json-test-results-format)

This script was derived from run_telemetry_benchmark_as_googletest, and calls
into that script.
"""

import argparse
import json
import os
import shutil
import sys
import tempfile
import traceback

import common

import run_telemetry_benchmark_as_googletest


def sharding_map_path():
  return os.path.join(
      os.path.dirname(__file__), '..', '..', 'tools', 'perf', 'core',
      'benchmark_sharding_map.json')

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-output', required=True)
  parser.add_argument(
      '--isolated-script-test-chartjson-output', required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)
  parser.add_argument(
      '--isolated-script-test-filter-file', type=str, required=False)
  parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')
  parser.add_argument('--output-format', action='append')
  parser.add_argument('--builder', required=True)
  parser.add_argument('--bot', required=True,
                      help='Bot ID to use to determine which tests to run. Will'
                           ' use //tools/perf/core/benchmark_sharding_map.json'
                           ' with this as a key to determine which benchmarks'
                           ' to execute')

  args, rest_args = parser.parse_known_args()
  for output_format in args.output_format:
    rest_args.append('--output-format=' + output_format)
  isolated_out_dir = os.path.dirname(args.isolated_script_test_output)

  with open(sharding_map_path()) as f:
    sharding_map = json.load(f)
  sharding = sharding_map[args.builder][args.bot]['benchmarks']
  return_code = 0

  for benchmark in sharding:
    # Insert benchmark name as first argument to run_benchmark call
    per_benchmark_args = rest_args[:1] + [benchmark] + rest_args[1:]
    # We don't care exactly what these are. In particular, the perf results
    # could be any format (chartjson, legacy, histogram). We just pass these
    # through, and expose these as results for this task.
    rc, perf_results, json_test_results = (
        run_telemetry_benchmark_as_googletest.run_benchmark(
            args, per_benchmark_args))

    return_code = return_code or rc
    benchmark_path = os.path.join(isolated_out_dir, benchmark)
    os.makedirs(benchmark_path)
    with open(os.path.join(benchmark_path, 'perf_results.json'), 'w') as f:
      json.dump(perf_results, f)
    with open(os.path.join(benchmark_path, 'test_results.json'), 'w') as f:
      json.dump(json_test_results, f)

  return return_code


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
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
