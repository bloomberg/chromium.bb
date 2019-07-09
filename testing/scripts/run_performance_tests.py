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

If this is executed with a gtest perf test, the flag --non-telemetry
has to be passed in to the script so the script knows it is running
an executable and not the run_benchmark command.

This script obeys the --isolated-script-test-output flag and merges test results
from all the benchmarks into the one output.json file. The test results and perf
results are also put in separate directories per
benchmark. Two files will be present in each directory; perf_results.json, which
is the perf specific results (with unenforced format, could be histogram or
graph json), and test_results.json, which is a JSON test results
format file
https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md

TESTING:
To test changes to this script, please run
cd tools/perf
./run_tests ScriptsSmokeTest.testRunPerformanceTests
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
sys.path.append(PERF_DIR)
import generate_legacy_perf_dashboard_json

PERF_CORE_DIR = os.path.join(PERF_DIR, 'core')
sys.path.append(PERF_CORE_DIR)
import results_merger

# Add src/testing/ into sys.path for importing xvfb and test_env.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb
import test_env

# Unfortunately we need to copy these variables from ../test_env.py.
# Importing it and using its get_sandbox_env breaks test runs on Linux
# (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'
SHARD_MAPS_DIRECTORY = os.path.join(
    os.path.dirname(__file__), '..', '..', 'tools', 'perf', 'core',
    'shard_maps')


class OutputFilePaths(object):
  """Provide paths to where results outputs should be written.

  The process_perf_results.py merge script later will pull all of these
  together, so that's why they aren't in the standard locations. Also,
  note that because of the OBBS (One Build Bot Step), Telemetry
  has multiple tests running on a single shard, so we need to prefix
  these locations with a directory named by the benchmark name.
  """

  def __init__(self, isolated_out_dir, perf_test_name):
    self.benchmark_path = os.path.join(isolated_out_dir, perf_test_name)

  def SetUp(self):
    os.makedirs(self.benchmark_path)
    return self

  @property
  def perf_results(self):
    return os.path.join(self.benchmark_path, 'perf_results.json')

  @property
  def test_results(self):
    return os.path.join(self.benchmark_path, 'test_results.json')

  @property
  def logs(self):
    return os.path.join(self.benchmark_path, 'benchmark_log.txt')

  @property
  def csv_perf_results(self):
    """Path for csv perf results.

    Note that the chrome.perf waterfall uses the json histogram perf results
    exclusively. csv_perf_results are implemented here in case a user script
    passes --output-format=csv.
    """
    return os.path.join(self.benchmark_path, 'perf_results.csv')


def print_duration(step, start):
  print 'Duration of %s: %d seconds' % (step, time.time() - start)


def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


class GtestCommandGenerator(object):
  def __init__(self, options):
    self._options = options

  def generate(self):
    """Generate the command to run to start the gtest perf test.

    Returns:
      list of strings, the executable and its arguments.
    """
    return ([self._get_executable()] +
            self._generate_filter_args() +
            self._generate_repeat_args() +
            self._generate_also_run_disabled_tests_args() +
            self._generate_output_args() +
            self._get_passthrough_args()
           )

  def _get_executable(self):
    executable = self._options.executable
    if IsWindows():
      return r'.\%s.exe' % executable
    else:
      return './%s' % executable

  def _get_passthrough_args(self):
    return self._options.passthrough_args

  def _generate_filter_args(self):
    if self._options.isolated_script_test_filter:
      filter_list = common.extract_filter_list(
        self._options.isolated_script_test_filter)
      return ['--gtest_filter=' + ':'.join(filter_list)]
    return []

  def _generate_repeat_args(self):
    # TODO(crbug.com/920002): Support --isolated-script-test-repeat.
    return []

  def _generate_also_run_disabled_tests_args(self):
    # TODO(crbug.com/920002): Support
    # --isolated-script-test-also-run-disabled-tests.
    return []

  def _generate_output_args(self):
    output_args = []
    # These flags are to make sure that test output perf metrics in the log.
    if not '--verbose' in self._options.passthrough_args:
      output_args.append('--verbose')
    if (not '--test-launcher-print-test-stdio=always'
        in self._options.passthrough_args):
      output_args.append('--test-launcher-print-test-stdio=always')
    return output_args


def write_legacy_test_results(return_code, output_filepath):
  # TODO(crbug.com/920002): Fix to output
  # https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md
  valid = (return_code == 0)
  failures = [] if valid else ['(entire test suite)']
  output_json = {
      'valid': valid,
      'failures': failures,
  }
  with open(output_filepath, 'w') as fh:
    json.dump(output_json, fh)


def execute_gtest_perf_test(command_generator, output_paths, use_xvfb=False):
  env = os.environ.copy()
  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH
  env['CHROME_HEADLESS'] = '1'

  return_code = 0
  try:
    command = command_generator.generate()
    if use_xvfb:
      # When running with xvfb, we currently output both to stdout and to the
      # file. It would be better to only output to the file to keep the logs
      # clean.
      return_code = xvfb.run_executable(
          command, env, stdoutfile=output_paths.logs)
    else:
      with open(output_paths.logs, 'w') as handle:
        return_code = test_env.run_command_output_to_handle(
            command, handle, env=env)
    # Get the correct json format from the stdout to write to the perf
    # results file.
    results_processor = generate_legacy_perf_dashboard_json.\
        LegacyResultsProcessor()
    graph_json_string = results_processor.GenerateJsonResults(
        output_paths.logs)
    with open(output_paths.perf_results, 'w') as fh:
      fh.write(graph_json_string)
  except Exception:
    traceback.print_exc()
    return_code = 1
  write_legacy_test_results(return_code, output_paths.test_results)
  return return_code


class _TelemetryFilterArgument(object):
  def __init__(self, filter_string):
    self.benchmark, self.story = filter_string.split('/')


class TelemetryCommandGenerator(object):
  def __init__(self, benchmark, options,
               story_selection_config=None, is_reference=False):
    self.benchmark = benchmark
    self._options = options
    self._story_selection_config = story_selection_config
    self._is_reference = is_reference

  def generate(self, output_dir):
    """Generate the command to run to start the benchmark.

    Args:
      output_dir: The directory to configure the command to put output files
        into.

    Returns:
      list of strings, the executable and its arguments.
    """
    return ([sys.executable, self._options.executable] +
            [self.benchmark] +
            self._generate_filter_args() +
            self._generate_also_run_disabled_tests_args() +
            self._generate_output_args(output_dir) +
            self._generate_story_selection_args() +
            # passthrough args must be before reference args and repeat args:
            # crbug.com/928928, crbug.com/894254#c78
            self._get_passthrough_args() +
            self._generate_repeat_args() +
            self._generate_reference_build_args()
           )

  def _get_passthrough_args(self):
    return self._options.passthrough_args

  def _generate_filter_args(self):
    if self._options.isolated_script_test_filter:
      filter_list = common.extract_filter_list(
          self._options.isolated_script_test_filter)
      filter_arguments = [_TelemetryFilterArgument(f) for f in filter_list]
      applicable_stories = [
          f.story for f in filter_arguments if f.benchmark == self.benchmark]
      # Need to convert this to a valid regex.
      filter_regex = '(' + '|'.join(applicable_stories) + ')'
      return ['--story-filter=' + filter_regex]
    return []

  def _generate_repeat_args(self):
    if self._options.isolated_script_test_repeat:
      return ['--pageset-repeat=' + str(
          self._options.isolated_script_test_repeat)]
    return []

  def _generate_also_run_disabled_tests_args(self):
    if self._options.isolated_script_test_also_run_disabled_tests:
      return ['--also-run-disabled-tests']
    return []

  def _generate_output_args(self, output_dir):
    return ['--output-format=json-test-results',
            '--output-format=histograms',
            '--output-dir=' + output_dir]

  def _generate_story_selection_args(self):
    """Returns arguments that limit the stories to be run inside the benchmark.
    """
    selection_args = []
    if self._story_selection_config:
      if 'begin' in self._story_selection_config:
        selection_args.append('--story-shard-begin-index=%d' % (
            self._story_selection_config['begin']))
      if 'end' in self._story_selection_config:
        selection_args.append('--story-shard-end-index=%d' % (
            self._story_selection_config['end']))
      if not self._story_selection_config.get('abridged', True):
        selection_args.append('--run-full-story-set')
    return selection_args

  def _generate_reference_build_args(self):
    if self._is_reference:
      return ['--browser=reference',
              '--max-failures=5']
    return []


def execute_telemetry_benchmark(
    command_generator, output_paths, use_xvfb=False):
  start = time.time()

  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'
  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH

  return_code = 1
  temp_dir = tempfile.mkdtemp('telemetry')
  try:
    command = command_generator.generate(temp_dir)
    if use_xvfb:
      # When running with xvfb, we currently output both to stdout and to the
      # file. It would be better to only output to the file to keep the logs
      # clean.
      return_code = xvfb.run_executable(
          command, env=env, stdoutfile=output_paths.logs)
    else:
      with open(output_paths.logs, 'w') as handle:
        return_code = test_env.run_command_output_to_handle(
            command, handle, env=env)
    expected_results_filename = os.path.join(temp_dir, 'test-results.json')
    if os.path.exists(expected_results_filename):
      shutil.move(expected_results_filename, output_paths.test_results)
    else:
      common.write_interrupted_test_results_to(output_paths.test_results, start)
    expected_perf_filename = os.path.join(temp_dir, 'histograms.json')
    shutil.move(expected_perf_filename, output_paths.perf_results)

    csv_file_path = os.path.join(temp_dir, 'results.csv')
    if os.path.isfile(csv_file_path):
      shutil.move(csv_file_path, output_paths.csv_perf_results)
  except Exception:
    print ('The following exception may have prevented the code from '
           'outputing structured test results and perf results output:')
    print traceback.format_exc()
  finally:
    # Add ignore_errors=True because otherwise rmtree may fail due to leaky
    # processes of tests are still holding opened handles to files under
    # |tempfile_dir|. For example, see crbug.com/865896
    shutil.rmtree(temp_dir, ignore_errors=True)

  print_duration('executing benchmark %s' % command_generator.benchmark, start)

  if return_code:
    return return_code
  return 0

def parse_arguments(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('executable', help='The name of the executable to run.')
  parser.add_argument(
      '--isolated-script-test-output', required=True)
  # The following two flags may be passed in sometimes by Pinpoint
  # or by the recipe, but they don't do anything. crbug.com/927482.
  parser.add_argument(
      '--isolated-script-test-chartjson-output', required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)

  parser.add_argument(
      '--isolated-script-test-filter', type=str, required=False)

  # Note that the following three arguments are only supported by Telemetry
  # tests right now. See crbug.com/920002.
  parser.add_argument(
      '--isolated-script-test-repeat', type=int, required=False)
  parser.add_argument(
      '--isolated-script-test-launcher-retry-limit', type=int, required=False,
      choices=[0])  # Telemetry does not support retries. crbug.com/894254#c21
  parser.add_argument(
      '--isolated-script-test-also-run-disabled-tests',
      default=False, action='store_true', required=False)
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
  parser.add_argument('--run-ref-build',
                      help='Run test on reference browser', action='store_true')
  parser.add_argument('--passthrough-arg',
                      help='Arguments to pass directly through to the test '
                      'executable.', action='append',
                      dest='passthrough_args',
                      default=[])
  options, leftover_args = parser.parse_known_args(args)
  options.passthrough_args.extend(leftover_args)
  return options


def main(sys_args):
  args = sys_args[1:]  # Skip program name.
  options = parse_arguments(args)
  isolated_out_dir = os.path.dirname(options.isolated_script_test_output)
  overall_return_code = 0
  # This is a list of test results files to be merged into a standard
  # output.json file for use by infrastructure including FindIt.
  # This list should not contain reference build runs
  # since we do not monitor those. Also, merging test reference build results
  # with standard build results may not work properly.
  test_results_files = []

  print('Running a series of performance test subprocesses. Logs, performance\n'
        'results, and test results JSON will be saved in a subfolder of the\n'
        'isolated output directory. Inside the hash marks in the following\n'
        'lines is the name of the subfolder to find results in.\n')

  if options.non_telemetry:
    command_generator = GtestCommandGenerator(options)
    benchmark_name = options.gtest_benchmark_name
    # Fallback to use the name of the executable if flag isn't set.
    # TODO(crbug.com/870899): remove fallback logic and raise parser error if
    # --non-telemetry is set but --gtest-benchmark-name is not set once pinpoint
    # is converted to always pass --gtest-benchmark-name flag.
    if not benchmark_name:
      benchmark_name = options.executable
    output_paths = OutputFilePaths(isolated_out_dir, benchmark_name).SetUp()
    print('\n### {folder} ###'.format(folder=benchmark_name))
    overall_return_code = execute_gtest_perf_test(
        command_generator, output_paths, options.xvfb)
    test_results_files.append(output_paths.test_results)
  else:
    # If the user has supplied a list of benchmark names, execute those instead
    # of using the shard map.
    if options.benchmarks:
      benchmarks = options.benchmarks.split(',')
      for benchmark in benchmarks:
        output_paths = OutputFilePaths(isolated_out_dir, benchmark).SetUp()
        command_generator = TelemetryCommandGenerator(
            benchmark, options)
        print('\n### {folder} ###'.format(folder=benchmark))
        return_code = execute_telemetry_benchmark(
            command_generator, output_paths, options.xvfb)
        overall_return_code = return_code or overall_return_code
        test_results_files.append(output_paths.test_results)
      if options.run_ref_build:
        print ('Not running reference build. --run-ref-build argument is only '
               'supported for sharded benchmarks. It is simple to support '
               'this for unsharded --benchmarks if needed.')
    elif options.test_shard_map_filename:
      # First determine what shard we are running on to know how to
      # index into the bot map to get list of telemetry benchmarks to run.
      shard_index = None
      shard_map_path = os.path.join(SHARD_MAPS_DIRECTORY,
                                    options.test_shard_map_filename)
      # Copy sharding map file to isolated_out_dir so that the merge script
      # can collect it later.
      # TODO(crouleau): Move this step over to merge script
      # (process_perf_results.py).
      shutil.copyfile(
          shard_map_path,
          os.path.join(isolated_out_dir, 'benchmarks_shard_map.json'))
      with open(shard_map_path) as f:
        shard_map = json.load(f)
      env = os.environ.copy()
      if 'GTEST_SHARD_INDEX' in env:
        shard_index = env['GTEST_SHARD_INDEX']
      # TODO(crbug.com/972844): shard environment variables are not specified
      # for single-shard shard runs.
      if not shard_index:
        shard_map_has_multiple_shards = bool(shard_map.get('1', False))
        if not shard_map_has_multiple_shards:
          shard_index = '0'
      if not shard_index:
        raise Exception(
            'Sharded Telemetry perf tests must either specify --benchmarks '
            'list or have GTEST_SHARD_INDEX environment variable present.')
      benchmarks_and_configs = shard_map[shard_index]['benchmarks']

      for (benchmark, story_selection_config
           ) in benchmarks_and_configs.iteritems():
        # Need to run the benchmark on both latest browser and reference build.
        output_paths = OutputFilePaths(isolated_out_dir, benchmark).SetUp()
        command_generator = TelemetryCommandGenerator(
            benchmark, options, story_selection_config=story_selection_config)
        print('\n### {folder} ###'.format(folder=benchmark))
        return_code = execute_telemetry_benchmark(
            command_generator, output_paths, options.xvfb)
        overall_return_code = return_code or overall_return_code
        test_results_files.append(output_paths.test_results)
        if options.run_ref_build:
          reference_benchmark_foldername = benchmark + '.reference'
          reference_output_paths = OutputFilePaths(
              isolated_out_dir, reference_benchmark_foldername).SetUp()
          reference_command_generator = TelemetryCommandGenerator(
              benchmark, options,
              story_selection_config=story_selection_config, is_reference=True)
          print('\n### {folder} ###'.format(
              folder=reference_benchmark_foldername))
          # We intentionally ignore the return code and test results of the
          # reference build.
          execute_telemetry_benchmark(
              reference_command_generator, reference_output_paths,
              options.xvfb)
    else:
      raise Exception('Telemetry tests must provide either a shard map or a '
                      '--benchmarks list so that we know which stories to run.')

  test_results_list = []
  for test_results_file in test_results_files:
    if os.path.exists(test_results_file):
      with open(test_results_file, 'r') as fh:
        test_results_list.append(json.load(fh))
  merged_test_results = results_merger.merge_test_results(test_results_list)
  with open(options.isolated_script_test_output, 'w') as f:
    json.dump(merged_test_results, f)

  return overall_return_code


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
  sys.exit(main(sys.argv))
