#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs telemetry benchmarks and gtest perf tests.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format detailed here:
https://www.chromium.org/developers/the-json-test-results-format

Optional argument:

  --isolated-script-test-filter=[TEST_NAMES]

is a double-colon-separated ("::") list of test names, to run just that subset
of tests. This list is forwarded to the run_telemetry_benchmark_as_googletest
script.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.

It currently runs several benchmarks. The benchmarks it will execute are
based on the shard it is running on and the sharding_map_path.

If this is executed with a non-telemetry perf test, the flag --non-telemetry
has to be passed in to the script so the script knows it is running
an executable and not the run_benchmark command.

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
import time
import tempfile
import traceback

import common

CHROMIUM_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
PERF_DIR = os.path.join(CHROMIUM_SRC_DIR, 'tools', 'perf')
# Add src/tools/perf where generate_legacy_perf_dashboard_json.py lives
sys.path.append(PERF_DIR)

import generate_legacy_perf_dashboard_json

# Add src/testing/ into sys.path for importing xvfb and test_env.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb
import test_env

# Unfortunately we need to copy these variables from ../test_env.py.
# Importing it and using its get_sandbox_env breaks test runs on Linux
# (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def get_sharding_map_path(args):
  return os.path.join(
      os.path.dirname(__file__), '..', '..', 'tools', 'perf', 'core',
      'shard_maps', args.test_shard_map_filename)

def write_results(
    perf_test_name, perf_results, json_test_results, benchmark_log,
    isolated_out_dir, encoded):
  benchmark_path = os.path.join(isolated_out_dir, perf_test_name)

  os.makedirs(benchmark_path)
  with open(os.path.join(benchmark_path, 'perf_results.json'), 'w') as f:
    # non telemetry perf results are already json encoded
    if encoded:
      f.write(perf_results)
    else:
      json.dump(perf_results, f)
  with open(os.path.join(benchmark_path, 'test_results.json'), 'w') as f:
    json.dump(json_test_results, f)

  with open(os.path.join(benchmark_path, 'benchmark_log.txt'), 'w') as f:
    f.write(benchmark_log)


def print_duration(step, start):
  print 'Duration of %s: %d seconds' % (step, time.time() - start)


def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def execute_gtest_perf_test(args, rest_args):
  env = os.environ.copy()
  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH

  rc = 0
  try:
    executable = rest_args[0]
    extra_flags = []
    if len(rest_args) > 1:
      extra_flags = rest_args[1:]

    # These flags are to make sure that test output perf metrics in the log.
    if not '--verbose' in extra_flags:
      extra_flags.append('--verbose')
    if not '--test-launcher-print-test-stdio=always' in extra_flags:
      extra_flags.append('--test-launcher-print-test-stdio=always')
    if args.isolated_script_test_filter:
      filter_list = common.extract_filter_list(
        args.isolated_script_test_filter)
      extra_flags.append('--gtest_filter=' + ':'.join(filter_list))

    if IsWindows():
      executable = '.\%s.exe' % executable
    else:
      executable = './%s' % executable
    with common.temporary_file() as tempfile_path:
      env['CHROME_HEADLESS'] = '1'
      cmd = [executable] + extra_flags

      if args.xvfb:
        rc = xvfb.run_executable(cmd, env, stdoutfile=tempfile_path)
      else:
        rc = test_env.run_command_with_output(cmd, env=env,
                                              stdoutfile=tempfile_path)

      # Now get the correct json format from the stdout to write to the perf
      # results file
      results_processor = (
          generate_legacy_perf_dashboard_json.LegacyResultsProcessor())
      charts = results_processor.GenerateJsonResults(tempfile_path)
  except Exception:
    traceback.print_exc()
    rc = 1

  valid = (rc == 0)
  failures = [] if valid else ['(entire test suite)']
  output_json = {
      'valid': valid,
      'failures': failures,
    }
  return rc, charts, output_json


def execute_telemetry_benchmark(
    benchmark, isolated_out_dir, args, rest_args, is_reference, stories=None):
  start = time.time()
  # While we are between chartjson and histogram set we need
  # to determine which output format to look for or see if it was
  # already passed in in which case that format applies to all benchmarks
  # in this run.
  is_histograms = append_output_format(args, rest_args)
  # Insert benchmark name as first argument to run_benchmark call
  # which is the first argument in the rest_args.  Also need to append
  # output format and smoke test mode.
  per_benchmark_args = (rest_args[:1] + [benchmark] + rest_args[1:])
  benchmark_name = benchmark
  if is_reference:
    # Need to parse out the browser to replace browser flag with
    # reference build so we run it reference build as well
    browser_index = 0
    for arg in per_benchmark_args:
      if "browser" in arg:
        break
      browser_index = browser_index + 1
    per_benchmark_args[browser_index] = '--browser=reference'
    # Now we need to add in the rest of the reference build args
    per_benchmark_args.append('--max-failures=5')
    per_benchmark_args.append('--output-trace-tag=_ref')
    benchmark_name = benchmark + '.reference'

  # If we are only running a subset of stories, add in the begin and end
  # index.
  if stories:
    if 'begin' in stories.keys():
      per_benchmark_args.append(
          ('--story-shard-begin-index=%d' % stories['begin']))
    if 'end' in stories.keys():
      per_benchmark_args.append(
          ('--story-shard-end-index=%d' % stories['end']))

  # We don't care exactly what these are. In particular, the perf results
  # could be any format (chartjson, legacy, histogram). We just pass these
  # through, and expose these as results for this task.
  rc, perf_results, json_test_results, benchmark_log = (
      execute_telemetry_benchmark_helper(
          args, per_benchmark_args, is_histograms))

  write_results(
      benchmark_name, perf_results, json_test_results, benchmark_log,
      isolated_out_dir, False)

  print_duration('executing benchmark %s' % benchmark_name, start)
  return rc


def execute_telemetry_benchmark_helper(args, rest_args, histogram_results):
  """Run benchmark with args.

  Args:
    args: the option object resulted from parsing commandline args required for
      IsolatedScriptTest contract (see
      https://cs.chromium.org/chromium/build/scripts/slave/recipe_modules/chromium_tests/steps.py?rcl=d31f256fb860701e6dc02544f2beffe4e17c9b92&l=1639).
    rest_args: the args (list of strings) for running Telemetry benchmark.
    histogram_results: a boolean describes whether to output histograms format
      for the benchmark.

  Returns: a tuple of (rc, perf_results, json_test_results, benchmark_log)
    rc: the return code of benchmark
    perf_results: json object contains the perf test results
    json_test_results: json object contains the Pass/Fail data of the benchmark.
    benchmark_log: string contains the stdout/stderr of the benchmark run.
  """
  # TODO(crbug.com/920002): These arguments cannot go into
  # run_performance_tests.py because
  # run_gtest_perf_tests.py does not yet support them. Note that ideally
  # we would use common.BaseIsolatedScriptArgsAdapter, but this will take
  # a good deal of refactoring to accomplish.
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-repeat', type=int, required=False)
  parser.add_argument(
      '--isolated-script-test-launcher-retry-limit', type=int, required=False,
      choices=[0])  # Telemetry does not support retries. crbug.com/894254#c21
  parser.add_argument(
      '--isolated-script-test-also-run-disabled-tests',
      default=False, action='store_true', required=False)
  # Parse leftover args not already parsed in run_performance_tests.py or in
  # main().
  args, rest_args = parser.parse_known_args(args=rest_args, namespace=args)

  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'

  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH
  tempfile_dir = tempfile.mkdtemp('telemetry')
  benchmark_log = ''
  stdoutfile = os.path.join(tempfile_dir, 'benchmark_log.txt')
  valid = True
  num_failures = 0
  perf_results = None
  json_test_results = None

  results = None
  cmd_args = rest_args
  if args.isolated_script_test_filter:
    filter_list = common.extract_filter_list(args.isolated_script_test_filter)
    # Need to convert this to a valid regex.
    filter_regex = '(' + '|'.join(filter_list) + ')'
    cmd_args.append('--story-filter=' + filter_regex)
  if args.isolated_script_test_repeat:
    cmd_args.append('--pageset-repeat=' + str(args.isolated_script_test_repeat))
  if args.isolated_script_test_also_run_disabled_tests:
    cmd_args.append('--also-run-disabled-tests')
  cmd_args.append('--output-dir=' + tempfile_dir)
  cmd_args.append('--output-format=json-test-results')
  cmd = [sys.executable] + cmd_args
  rc = 1  # Set default returncode in case there is an exception.
  try:
    if args.xvfb:
      rc = xvfb.run_executable(cmd, env=env, stdoutfile=stdoutfile)
    else:
      rc = test_env.run_command_with_output(cmd, env=env, stdoutfile=stdoutfile)

    with open(stdoutfile) as f:
      benchmark_log = f.read()

    # If we have also output chartjson read it in and return it.
    # results-chart.json is the file name output by telemetry when the
    # chartjson output format is included
    tempfile_name = None
    if histogram_results:
      tempfile_name = os.path.join(tempfile_dir, 'histograms.json')
    else:
      tempfile_name = os.path.join(tempfile_dir, 'results-chart.json')

    if tempfile_name is not None:
      with open(tempfile_name) as f:
        perf_results = json.load(f)

    # test-results.json is the file name output by telemetry when the
    # json-test-results format is included
    tempfile_name = os.path.join(tempfile_dir, 'test-results.json')
    with open(tempfile_name) as f:
      json_test_results = json.load(f)
    num_failures = json_test_results['num_failures_by_type'].get('FAIL', 0)
    valid = bool(rc == 0 or num_failures != 0)

  except Exception:
    traceback.print_exc()
    if results:
      print 'results, which possibly caused exception: %s' % json.dumps(
          results, indent=2)
    valid = False
  finally:
    # Add ignore_errors=True because otherwise rmtree may fail due to leaky
    # processes of tests are still holding opened handles to files under
    # |tempfile_dir|. For example, see crbug.com/865896
    shutil.rmtree(tempfile_dir, ignore_errors=True)

  if not valid and num_failures == 0:
    if rc == 0:
      rc = 1  # Signal an abnormal exit.

  return rc, perf_results, json_test_results, benchmark_log


def append_output_format(args, rest_args):
  # We need to determine if the output format is already passed in
  # or if we need to define it for this benchmark
  perf_output_specified = False
  is_histograms = False
  if args.output_format:
    for output_format in args.output_format:
      if 'histograms' in output_format:
        perf_output_specified = True
        is_histograms = True
      if 'chartjson' in output_format:
        perf_output_specified = True
      rest_args.append('--output-format=' + output_format)
  # When crbug.com/744736 is resolved we no longer have to check
  # the type of format per benchmark and can rely on it being passed
  # in as an arg as all benchmarks will output the same format.
  if not perf_output_specified:
    rest_args.append('--output-format=histograms')
    is_histograms = True
  return is_histograms


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-output', required=True)
  # These two flags are passed in from the swarming recipe
  # but will no longer be needed when we migrate to this new recipe.
  # For now we need to recognize them so they don't get passed
  # through to telemetry.
  parser.add_argument(
      '--isolated-script-test-chartjson-output', required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)

  parser.add_argument(
      '--isolated-script-test-filter', type=str, required=False)
  parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')
  parser.add_argument('--non-telemetry',
                      help='Type of perf test', type=bool, default=False)
  parser.add_argument('--gtest-benchmark-name',
                      help='Name of the gtest benchmark', type=str,
                      required=False)

  parser.add_argument('--benchmarks',
                      help='Comma separated list of benchmark names'
                      ' to run in lieu of indexing into our benchmark bot maps',
                      required=False)
  # Some executions may have a different sharding scheme and/or set of tests.
  # These files must live in src/tools/perf/core/shard_maps
  parser.add_argument('--test-shard-map-filename', type=str, required=False)
  parser.add_argument('--output-format', action='append')
  parser.add_argument('--run-ref-build',
                      help='Run test on reference browser', action='store_true')

  args, rest_args = parser.parse_known_args()
  isolated_out_dir = os.path.dirname(args.isolated_script_test_output)
  return_code = 0

  if args.non_telemetry:
    benchmark_name = args.gtest_benchmark_name
    # Fallback to use the name of the executable if flag isn't set.
    # TODO(crbug.com/870899): remove fallback logic and raise parser error if
    # -non-telemetry is set but --gtest-benchmark-name is not set once pinpoint
    # is converted to always pass --gtest-benchmark-name flag.
    if not benchmark_name:
      benchmark_name = rest_args[0]
    return_code, charts, output_json = execute_gtest_perf_test(
        args, rest_args)

    write_results(benchmark_name, charts, output_json,
                  benchmark_log='Not available for C++ perf test',
                  isolated_out_dir=isolated_out_dir, encoded=True)
  else:
    # If the user has supplied a list of benchmark names, execute those instead
    # of the entire suite of benchmarks.
    if args.benchmarks:
      benchmarks = args.benchmarks.split(',')
      for benchmark in benchmarks:
        return_code = (execute_telemetry_benchmark(
            benchmark, isolated_out_dir, args, rest_args, False) or return_code)
    else:
      # First determine what shard we are running on to know how to
      # index into the bot map to get list of benchmarks to run.
      total_shards = None
      shard_index = None

      env = os.environ.copy()
      if 'GTEST_TOTAL_SHARDS' in env:
        total_shards = env['GTEST_TOTAL_SHARDS']
      if 'GTEST_SHARD_INDEX' in env:
        shard_index = env['GTEST_SHARD_INDEX']

      if not (total_shards or shard_index):
        raise Exception('Shard indicators must be present for perf tests')

      sharding_map_path = get_sharding_map_path(args)

      # Copy sharding map file to isolated_out_dir so that the collect script
      # can collect it later.
      shutil.copyfile(
          sharding_map_path,
          os.path.join(isolated_out_dir, 'benchmarks_shard_map.json'))

      with open(sharding_map_path) as f:
        sharding_map = json.load(f)
      sharding = sharding_map[shard_index]['benchmarks']

      for benchmark, stories in sharding.iteritems():
        # Need to run the benchmark twice on browser and reference build
        return_code = (execute_telemetry_benchmark(
            benchmark, isolated_out_dir, args, rest_args,
            False, stories=stories) or return_code)
        # We ignore the return code of the reference build since we do not
        # monitor it.
        if args.run_ref_build:
          execute_telemetry_benchmark(
              benchmark, isolated_out_dir, args, rest_args, True,
              stories=stories)

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
