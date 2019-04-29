# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import copy
import cStringIO
import unittest
import tempfile
import json
import os

import mock

from core import perf_data_generator
from core.perf_data_generator import BenchmarkMetadata


class PerfDataGeneratorTest(unittest.TestCase):
  def setUp(self):
    # Test config can be big, so set maxDiff to None to see the full comparision
    # diff when assertEquals fails.
    self.maxDiff = None

  def test_get_scheduled_non_telemetry_benchmarks(self):
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    tmpfile.close()
    fake_perf_waterfall_file = tmpfile.name

    data = {
        'builder 1': {
          'isolated_scripts': [
            {'name': 'test_dancing'},
            {'name': 'test_singing'},
            {'name': 'performance_test_suite'},
          ],
          'scripts': [
            {'name': 'ninja_test'},
          ]
        },
        'builder 2': {
          'scripts': [
            {'name': 'gun_slinger'},
          ]
        }
      }
    try:
      with open(fake_perf_waterfall_file, 'w') as f:
        json.dump(data, f)
      self.assertEquals(
          perf_data_generator.get_scheduled_non_telemetry_benchmarks(
              fake_perf_waterfall_file),
          {'ninja_test', 'gun_slinger', 'test_dancing', 'test_singing'})
    finally:
      os.remove(fake_perf_waterfall_file)


  def testGenerateCPlusPlusTestSuite(self):
    swarming_dimensions = [
        {'os': 'SkyNet', 'pool': 'T-RIP'}
    ]
    test_config = {
        'platform': 'win',
        'dimension': swarming_dimensions,
    }
    test = {
        'isolate': 'angle_perftest',
        'telemetry': False,
        'num_shards': 1
    }
    returned_test = perf_data_generator.generate_performance_test(
        test_config, test)

    expected_generated_test = {
        'override_compile_targets': ['angle_perftest'],
        'isolate_name': 'angle_perftest',
        'args': ['--gtest-benchmark-name', 'angle_perftest'],
        'trigger_script': {
          'args': [
            '--multiple-dimension-script-verbose',
            'True'
          ],
          'requires_simultaneous_shard_dispatch': True,
          'script': '//testing/trigger_scripts/perf_device_trigger.py'
        },
        'merge': {
          'script': '//tools/perf/process_perf_results.py'
        },
        'swarming': {
          'ignore_task_failure': False,
          'can_use_on_swarming_builders': True,
          'expiration': 7200,
          'io_timeout': 1800,
          'hard_timeout': 36000,
          'dimension_sets': [[{'os': 'SkyNet', 'pool': 'T-RIP'}]],
          'shards': 1
        },
        'name': 'angle_perftest'
      }
    self.assertEquals(returned_test, expected_generated_test)

  def testGeneratePerformanceTestSuiteExact(self):
    swarming_dimensions = [
        {'os': 'SkyNet', 'pool': 'T-RIP'}
    ]
    test_config = {
        'platform': 'android-webview',
        'browser': 'bin/monochrome_64_32_bundle',
        'dimension': swarming_dimensions,
    }
    test = {
        'isolate': 'performance_test_suite',
        'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=shard_map.json',
          ],
        'num_shards': 26
    }
    returned_test = perf_data_generator.generate_performance_test(
        test_config, test)

    expected_generated_test = {
        'override_compile_targets': ['performance_test_suite'],
        'isolate_name': 'performance_test_suite',
        'args': ['-v', '--browser=exact', '--upload-results',
                 '--browser-executable=../../out/Release'
                 '/bin/monochrome_64_32_bundle',
                 '--device=android',
                 '--webview-embedder-apk=../../out/Release'
                 '/apks/SystemWebViewShell.apk',
                 '--run-ref-build',
                 '--test-shard-map-filename=shard_map.json'],
        'trigger_script': {
          'args': [
            '--multiple-dimension-script-verbose',
            'True'
          ],
          'requires_simultaneous_shard_dispatch': True,
          'script': '//testing/trigger_scripts/perf_device_trigger.py'
        },
        'merge': {
          'script': '//tools/perf/process_perf_results.py'
        },
        'swarming': {
          'ignore_task_failure': False,
          'can_use_on_swarming_builders': True,
          'expiration': 7200,
          'io_timeout': 1800,
          'hard_timeout': 36000,
          'dimension_sets': [[{'os': 'SkyNet', 'pool': 'T-RIP'}]],
          'shards': 26
        },
        'name': 'performance_test_suite'
      }
    self.assertEquals(returned_test, expected_generated_test)

  def testGeneratePerformanceTestSuiteWebview(self):
    swarming_dimensions = [
        {'os': 'SkyNet', 'pool': 'T-RIP'}
    ]
    test_config = {
        'platform': 'android-webview',
        'dimension': swarming_dimensions,
    }
    test = {
        'isolate': 'performance_test_suite',
        'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=shard_map.json',
          ],
        'num_shards': 26
    }
    returned_test = perf_data_generator.generate_performance_test(
        test_config, test)

    expected_generated_test = {
        'override_compile_targets': ['performance_test_suite'],
        'isolate_name': 'performance_test_suite',
        'args': ['-v', '--browser=android-webview', '--upload-results',
                 '--webview-embedder-apk=../../out/Release'
                 '/apks/SystemWebViewShell.apk',
                 '--run-ref-build',
                 '--test-shard-map-filename=shard_map.json'],
        'trigger_script': {
          'args': [
            '--multiple-dimension-script-verbose',
            'True'
          ],
          'requires_simultaneous_shard_dispatch': True,
          'script': '//testing/trigger_scripts/perf_device_trigger.py'
        },
        'merge': {
          'script': '//tools/perf/process_perf_results.py'
        },
        'swarming': {
          'ignore_task_failure': False,
          'can_use_on_swarming_builders': True,
          'expiration': 7200,
          'io_timeout': 1800,
          'hard_timeout': 36000,
          'dimension_sets': [[{'os': 'SkyNet', 'pool': 'T-RIP'}]],
          'shards': 26
        },
        'name': 'performance_test_suite'
      }
    self.assertEquals(returned_test, expected_generated_test)

  def testGeneratePerformanceTestSuite(self):
    swarming_dimensions = [
        {'os': 'SkyNet', 'pool': 'T-RIP'}
    ]
    test_config = {
        'platform': 'android',
        'dimension': swarming_dimensions,
    }
    test = {
        'isolate': 'performance_test_suite',
        'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=shard_map.json',
          ],
        'num_shards': 26
    }
    returned_test = perf_data_generator.generate_performance_test(
        test_config, test)

    expected_generated_test = {
        'override_compile_targets': ['performance_test_suite'],
        'isolate_name': 'performance_test_suite',
        'args': ['-v', '--browser=android-chromium', '--upload-results',
                 '--run-ref-build',
                 '--test-shard-map-filename=shard_map.json'],
        'trigger_script': {
          'args': [
            '--multiple-dimension-script-verbose',
            'True'
          ],
          'requires_simultaneous_shard_dispatch': True,
          'script': '//testing/trigger_scripts/perf_device_trigger.py'
        },
        'merge': {
          'script': '//tools/perf/process_perf_results.py'
        },
        'swarming': {
          'ignore_task_failure': False,
          'can_use_on_swarming_builders': True,
          'expiration': 7200,
          'io_timeout': 1800,
          'hard_timeout': 36000,
          'dimension_sets': [[{'os': 'SkyNet', 'pool': 'T-RIP'}]],
          'shards': 26
        },
        'name': 'performance_test_suite'
      }
    self.assertEquals(returned_test, expected_generated_test)


class TestIsPerfBenchmarksSchedulingValid(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self.original_NON_TELEMETRY_BENCHMARKS = copy.deepcopy(
        perf_data_generator.NON_TELEMETRY_BENCHMARKS)
    self.original_TELEMETRY_PERF_BENCHMARKS = copy.deepcopy(
        perf_data_generator.NON_TELEMETRY_BENCHMARKS)
    self.test_stream = cStringIO.StringIO()
    self.mock_get_telemetry_benchmarks = mock.patch(
        'core.perf_data_generator.get_telemetry_tests_in_performance_test_suite'
        )
    self.get_telemetry_benchmarks = (
        self.mock_get_telemetry_benchmarks.start())
    self.mock_get_non_telemetry_benchmarks = mock.patch(
        'core.perf_data_generator.get_scheduled_non_telemetry_benchmarks')
    self.get_non_telemetry_benchmarks = (
        self.mock_get_non_telemetry_benchmarks.start())

  def tearDown(self):
    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = (
        self.original_TELEMETRY_PERF_BENCHMARKS)
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = (
        self.original_NON_TELEMETRY_BENCHMARKS)
    self.mock_get_telemetry_benchmarks.stop()
    self.mock_get_non_telemetry_benchmarks.stop()

  def test_returnTrue(self):
    self.get_telemetry_benchmarks.return_value = {'t_foo', 't_bar'}
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = {
        't_foo': BenchmarkMetadata('t@foo.com'),
        't_bar': BenchmarkMetadata('t@bar.com'),
    }
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, True)
    self.assertEquals(self.test_stream.getvalue(), '')

  def test_UnscheduledTelemetryBenchmarks(
      self):
    self.get_telemetry_benchmarks.return_value = {'t_bar'}
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = {
        'darth.vader': BenchmarkMetadata('death@star.com'),
        't_bar': BenchmarkMetadata('t@bar.com'),
    }
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, False)
    self.assertIn('Telemetry benchmark darth.vader exists but is not scheduled',
        self.test_stream.getvalue())

  def test_UnscheduledCppBenchmarks(self):
    self.get_telemetry_benchmarks.return_value = {'t_bar'}
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = {
        't_bar': BenchmarkMetadata('t@bar.com'),
    }
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
        'toyota': BenchmarkMetadata('baz@foo.com'),
    }
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, False)
    self.assertIn('Benchmark toyota is tracked but not scheduled',
        self.test_stream.getvalue())

  def test_UntrackedTelemetryBenchmarks(
      self):
    self.get_telemetry_benchmarks.return_value = {'t_bar', 'darth.vader'}
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = {
        't_bar': BenchmarkMetadata('t@bar.com'),
    }
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, False)
    self.assertIn('Telemetry benchmark darth.vader no longer exists',
        self.test_stream.getvalue())

  def test_UntrackedCppBenchmarks(self):
    self.get_telemetry_benchmarks.return_value = {'t_bar'}
    self.get_non_telemetry_benchmarks.return_value = {'honda', 'tesla'}

    perf_data_generator.TELEMETRY_PERF_BENCHMARKS = {
        't_bar': BenchmarkMetadata('t@bar.com'),
    }
    perf_data_generator.NON_TELEMETRY_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, False)
    self.assertIn(
        'Benchmark tesla is scheduled on perf waterfall but not tracked',
        self.test_stream.getvalue())
