#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=too-many-lines

"""Script to generate chromium.perf.json in
the src/testing/buildbot directory and benchmark.csv in the src/tools/perf
directory. Maintaining these files by hand is too unwieldy.
Note: chromium.perf.fyi.json is updated manually for now until crbug.com/757933
is complete.
"""
import argparse
import collections
import csv
import filecmp
import json
import os
import shutil
import sys
import tempfile
import textwrap

from core import benchmark_finders
from core import benchmark_utils
from core import bot_platforms
from core import path_util
from core import undocumented_benchmarks as ub_module
path_util.AddTelemetryToPath()

from telemetry import decorators


# Additional compile targets to add to builders.
# On desktop builders, chromedriver is added as an additional compile target.
# The perf waterfall builds this target for each commit, and the resulting
# ChromeDriver is archived together with Chrome for use in bisecting.
# This can be used by Chrome test team, as well as by google3 teams for
# bisecting Chrome builds with their web tests. For questions or to report
# issues, please contact johnchen@chromium.org.
BUILDER_ADDITIONAL_COMPILE_TARGETS = {
    'android-builder-perf': [
        'microdump_stackwalk', 'angle_perftests', 'chrome_apk'
    ],
    'android_arm64-builder-perf': [
        'microdump_stackwalk', 'angle_perftests', 'chrome_apk'
    ],
    'linux-builder-perf': ['chromedriver'],
    'mac-builder-perf': ['chromedriver'],
    'win32-builder-perf': ['chromedriver'],
    'win64-builder-perf': ['chromedriver'],
}


# To add a new isolate, add an entry to the 'tests' section.  Supported
# values in this json are:
# isolate: the name of the isolate you are trigger
# test_suite: name of the test suite if different than the isolate
#     that you want to show up as the test name
# extra_args: args that need to be passed to the script target
#     of the isolate you are running.
# shards: shard indices that you want the isolate to run on.  If omitted
#     will run on all shards.
# telemetry: boolean indicating if this is a telemetry test.  If omitted
#     assumed to be true.

# TODO(crbug.com/902089): automatically generate --test-shard-map-filename
# arguments once we track all the perf FYI builders to core/bot_platforms.py
NEW_PERF_RECIPE_FYI_TESTERS = {
  'testers' : {
    'android-pixel2_webview-perf': {
      'tests': [
        {
          'isolate': 'performance_webview_test_suite',
          'extra_args': [
            '--test-shard-map-filename=android-pixel2_webview-perf_map.json',
          ],
          'num_shards': 7
        }
      ],
      'platform': 'android-webview-google',
      'dimension': {
        'pool': 'chrome.tests.perf-webview-fyi',
        'os': 'Android',
        'device_type': 'walleye',
        'device_os': 'O',
        'device_os_flavor': 'google',
      },
    },
    'android-pixel2-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=android-pixel2-perf_map.json',
          ],
          'num_shards': 7
        }
      ],
      'platform': 'android-chrome',
      'dimension': {
        'pool': 'chrome.tests.perf-fyi',
        'os': 'Android',
        'device_type': 'walleye',
        'device_os': 'O',
        'device_os_flavor': 'google',
      },
    },
  }
}

# These configurations are taken from chromium_perf.py in
# build/scripts/slave/recipe_modules/chromium_tests and must be kept in sync
# to generate the correct json for each tester
NEW_PERF_RECIPE_MIGRATED_TESTERS = {
  'testers' : {
    'android-go-perf': {
      'tests': [
        {
          'name': 'performance_test_suite',
          'isolate': 'performance_test_suite',
          'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=android-go-perf_map.json',
          ],
          'num_shards': 19
        }
      ],
      'platform': 'android',
      'dimension': {
        'device_os': 'O',
        'device_type': 'gobo',
        'device_os_flavor': 'google',
        'pool': 'chrome.tests.perf',
        'os': 'Android',
      },
    },
    'android-go_webview-perf': {
      'tests': [
        {
          'isolate': 'performance_webview_test_suite',
          'extra_args': [
              '--test-shard-map-filename=android-go_webview-perf_map.json',
          ],
          'num_shards': 25
        }
      ],
      'platform': 'android-webview-google',
      'dimension': {
        'pool': 'chrome.tests.perf-webview',
        'os': 'Android',
        'device_type': 'gobo',
        'device_os': 'O',
        'device_os_flavor': 'google',
      },
    },
    'android-nexus5x-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 16,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=android-nexus5x-perf_map.json',
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'components_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'tracing_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'gpu_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'angle_perftests',
          'num_shards': 1,
          'telemetry': False,
          'extra_args': [
              '--shard-timeout=300'
          ],
        },
        {
          'isolate': 'base_perftests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'android',
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Android',
        'device_type': 'bullhead',
        'device_os': 'MMB29Q',
        'device_os_flavor': 'google',
      },
    },
    'Android Nexus5 Perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 16,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=android_nexus5_perf_map.json',
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'tracing_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'components_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'gpu_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
      ],
      'platform': 'android',
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Android',
        'device_type': 'hammerhead',
        'device_os': 'KOT49H',
        'device_os_flavor': 'google',
      },
    },
    'Android Nexus5X WebView Perf': {
      'tests': [
        {
          'isolate': 'performance_webview_test_suite',
          'num_shards': 16,
          'extra_args': [
              '--test-shard-map-filename=android_nexus5x_webview_perf_map.json',
              '--assert-gpu-compositing',
          ],
        }
      ],
      'platform': 'android-webview',
      'dimension': {
        'pool': 'chrome.tests.perf-webview',
        'os': 'Android',
        'device_type': 'bullhead',
        'device_os': 'MOB30K',
        'device_os_flavor': 'aosp',
      },
    },
    'Android Nexus6 WebView Perf': {
      'tests': [
        {
          'isolate': 'performance_webview_test_suite',
          'num_shards': 8,
          'extra_args': [
              '--test-shard-map-filename=android_nexus6_webview_perf_map.json',
              '--assert-gpu-compositing',
          ],
        }
      ],
      'platform': 'android-webview',
      'dimension': {
        'pool': 'chrome.tests.perf-webview',
        'os': 'Android',
        'device_type': 'shamu',
        'device_os': 'MOB30K',
        'device_os_flavor': 'aosp',
      },
    },
    'win-10-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 26,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=win-10-perf_map.json',
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'angle_perftests',
          'num_shards': 1,
          'telemetry': False,
          'extra_args': [
              '--shard-timeout=300'
          ],
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'components_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'views_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'base_perftests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'win',
      'target_bits': 64,
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Windows-10',
        'gpu': '8086:5912'
      },
    },
    'Win 7 Perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 5,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=win_7_perf_map.json',
          ],
        },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'components_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'win',
      'target_bits': 32,
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Windows-2008ServerR2-SP1',
        'gpu': '102b:0532'
      },
    },
    'Win 7 Nvidia GPU Perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 5,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=win_7_nvidia_gpu_perf_map.json',
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'angle_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'test_suite': 'passthrough_command_buffer_perftests',
          'isolate': 'command_buffer_perftests',
          'num_shards': 1,
          'telemetry': False,
          'extra_args': [
              '--use-cmd-decoder=passthrough',
              '--use-angle=gl-null',
          ],
        },
        {
          'test_suite': 'validating_command_buffer_perftests',
          'isolate': 'command_buffer_perftests',
          'num_shards': 1,
          'telemetry': False,
          'extra_args': [
              '--use-cmd-decoder=validating',
              '--use-stub',
          ],
        },
      ],
      'platform': 'win',
      'target_bits': 64,
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Windows-2008ServerR2-SP1',
        'gpu': '10de:1cb3'
      },
    },
    'mac-10_12_laptop_low_end-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 26,
          'extra_args': [
              '--run-ref-build',
              ('--test-shard-map-filename='
               'mac-10_12_laptop_low_end-perf_map.json'),
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'performance_browser_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'mac',
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Mac-10.12',
        'gpu': '8086:1626'
      },
    },
    'linux-perf': {
      'tests': [
        # Add views_perftests, crbug.com/811766
        {
          'isolate': 'performance_test_suite',
          'num_shards': 26,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=linux-perf_map.json',
              '--assert-gpu-compositing',
          ],
        },
        {
          'isolate': 'performance_browser_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'net_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'tracing_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'base_perftests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'linux',
      'dimension': {
        'gpu': '10de:1cb3',
        'os': 'Ubuntu-14.04',
        'pool': 'chrome.tests.perf',
      },
    },
    'mac-10_13_laptop_high_end-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=mac-10_13_laptop_high_end-perf_map.json',
              '--assert-gpu-compositing',
          ],
          'num_shards': 26
        },
        {
          'isolate': 'performance_browser_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'net_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'views_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'media_perftests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'base_perftests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'mac',
      'dimension': {
        'pool': 'chrome.tests.perf',
        'os': 'Mac-10.13',
        'gpu': '1002:6821'
      },
    },
  }
}


def add_builder(waterfall, name, additional_compile_targets=None):
  waterfall['builders'][name] = added = {}
  if additional_compile_targets:
    added['additional_compile_targets'] = additional_compile_targets

  return waterfall

def get_waterfall_builder_config():
  builders = {'builders':{}}

  for builder, targets in BUILDER_ADDITIONAL_COMPILE_TARGETS.items():
    builders = add_builder(
        builders, builder, additional_compile_targets=targets)

  return builders


def update_all_tests(waterfall, file_path):
  tests = {}
  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See //tools/perf/generate_perf_data to make changes'] = {}

  # Add in builders
  for name, config in waterfall['builders'].iteritems():
    tests[name] = config

  # Add in tests
  generate_telemetry_tests(NEW_PERF_RECIPE_MIGRATED_TESTERS, tests)
  with open(file_path, 'w') as fp:
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')

def merge_dicts(*dict_args):
    result = {}
    for dictionary in dict_args:
      result.update(dictionary)
    return result


class BenchmarkMetadata(object):
  def __init__(self, emails, component='', documentation_url='', tags=''):
    self.emails = emails
    self.component = component
    self.documentation_url = documentation_url
    self.tags = tags


NON_TELEMETRY_BENCHMARKS = {
    'angle_perftests': BenchmarkMetadata(
        'jmadill@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU>ANGLE'),
    'base_perftests': BenchmarkMetadata(
        'skyostil@chromium.org, gab@chromium.org',
        'Internals>SequenceManager',
        ('https://chromium.googlesource.com/chromium/src/+/HEAD/base/' +
         'README.md#performance-testing')),
    'validating_command_buffer_perftests': BenchmarkMetadata(
        'piman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU'),
    'passthrough_command_buffer_perftests': BenchmarkMetadata(
        'piman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU>ANGLE'),
    'net_perftests': BenchmarkMetadata(
        'xunjieli@chromium.org'),
    'gpu_perftests': BenchmarkMetadata(
        'reveman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU'),
    'tracing_perftests': BenchmarkMetadata(
        'kkraynov@chromium.org, primiano@chromium.org'),
    'load_library_perf_tests': BenchmarkMetadata(
        'xhwang@chromium.org, crouleau@chromium.org',
        'Internals>Media>Encrypted'),
    'performance_browser_tests': BenchmarkMetadata(
        'miu@chromium.org', 'Internals>Media>ScreenCapture'),
    'media_perftests': BenchmarkMetadata(
        'crouleau@chromium.org, dalecurtis@chromium.org',
        'Internals>Media'),
    'views_perftests': BenchmarkMetadata(
        'tapted@chromium.org', 'Internals>Views'),
    'components_perftests': BenchmarkMetadata('csharrison@chromium.org')
}


# If you change this dictionary, run tools/perf/generate_perf_data
NON_WATERFALL_BENCHMARKS = {
    'sizes (mac)':
        BenchmarkMetadata('tapted@chromium.org'),
    'sizes (win)': BenchmarkMetadata('grt@chromium.org',
                                     'Internals>PlatformIntegration'),
    'sizes (linux)': BenchmarkMetadata(
        'thestig@chromium.org', 'thomasanderson@chromium.org',
        'Internals>PlatformIntegration'),
    'resource_sizes': BenchmarkMetadata(
        'agrieve@chromium.org, rnephew@chromium.org, perezju@chromium.org'),
    'supersize_archive': BenchmarkMetadata('agrieve@chromium.org'),
}


def _get_telemetry_perf_benchmarks_metadata():
  metadata = {}
  benchmark_list = benchmark_finders.GetAllPerfBenchmarks()

  for benchmark in benchmark_list:
    emails = decorators.GetEmails(benchmark)
    if emails:
      emails = ', '.join(emails)
    tags_set = benchmark_utils.GetStoryTags(benchmark())
    metadata[benchmark.Name()] = BenchmarkMetadata(
        emails, decorators.GetComponent(benchmark),
        decorators.GetDocumentationLink(benchmark),
        ','.join(tags_set))
  return metadata


TELEMETRY_PERF_BENCHMARKS = _get_telemetry_perf_benchmarks_metadata()

ALL_PERF_WATERFALL_BENCHMARKS_METADATA = merge_dicts(
    TELEMETRY_PERF_BENCHMARKS, NON_TELEMETRY_BENCHMARKS)


# With migration to new recipe tests are now listed in the shard maps
# that live in tools/perf/core.  We need to verify off of that list.
def get_telemetry_tests_in_performance_test_suite():
  tests = set()
  add_benchmarks_from_sharding_map(
      tests, "shard_maps/linux-perf_map.json")
  add_benchmarks_from_sharding_map(
      tests, "shard_maps/android-pixel2-perf_map.json")
  return tests


def add_benchmarks_from_sharding_map(tests, shard_map_name):
  path = os.path.join(os.path.dirname(__file__), shard_map_name)
  if os.path.exists(path):
    with open(path) as f:
      sharding_map = json.load(f)
    for shard, benchmarks in sharding_map.iteritems():
      if "extra_infos" in shard:
        continue
      for benchmark, _ in benchmarks['benchmarks'].iteritems():
        tests.add(benchmark)


def get_scheduled_non_telemetry_benchmarks(perf_waterfall_file):
  test_names = set()

  with open(perf_waterfall_file) as f:
    tests_by_builder = json.load(f)

  script_tests = []
  for tests in tests_by_builder.values():
    if 'isolated_scripts' in tests:
      script_tests += tests['isolated_scripts']
    if 'scripts' in tests:
      script_tests += tests['scripts']

  for s in script_tests:
    name = s['name']
    # TODO(eyaich): Determine new way to generate ownership based
    # on the benchmark bot map instead of on the generated tests
    # for new perf recipe.
    if not name in ('performance_test_suite',
                    'performance_webview_test_suite'):
      test_names.add(name)

  return test_names


def is_perf_benchmarks_scheduling_valid(
    perf_waterfall_file, outstream):
  """Validates that all existing benchmarks are properly scheduled.

  Return: True if all benchmarks are properly scheduled, False otherwise.
  """
  scheduled_telemetry_tests = get_telemetry_tests_in_performance_test_suite()
  scheduled_non_telemetry_tests = get_scheduled_non_telemetry_benchmarks(
      perf_waterfall_file)

  all_perf_telemetry_tests = set(TELEMETRY_PERF_BENCHMARKS)
  all_perf_non_telemetry_tests = set(NON_TELEMETRY_BENCHMARKS)

  error_messages = []

  for test_name in all_perf_telemetry_tests - scheduled_telemetry_tests:
    if not test_name.startswith('UNSCHEDULED_'):
      error_messages.append(
          'Telemetry benchmark %s exists but is not scheduled to run. Rename '
          'it to UNSCHEDULED_%s, then file a crbug against Telemetry and '
          'Chrome Client Infrastructure team to schedule the benchmark on the '
          'perf waterfall.' % (test_name, test_name))

  for test_name in scheduled_telemetry_tests - all_perf_telemetry_tests:
    error_messages.append(
        'Telemetry benchmark %s no longer exists but is scheduled. File a bug '
        'against Telemetry and/or Chrome Client Infrastructure team to remove '
        'the corresponding benchmark class and deschedule the benchmark on the '
        "perf waterfall. After that, you can safely remove the benchmark's "
        'dependency code, e.g: stories, WPR archives, metrics, etc.' %
        test_name)

  for test_name in all_perf_non_telemetry_tests - scheduled_non_telemetry_tests:
    error_messages.append(
        'Benchmark %s is tracked but not scheduled on any perf waterfall '
        'builders. Either schedule or remove it from NON_TELEMETRY_BENCHMARKS.'
        % test_name)

  for test_name in scheduled_non_telemetry_tests - all_perf_non_telemetry_tests:
    error_messages.append(
        'Benchmark %s is scheduled on perf waterfall but not tracked. Please '
        'add an entry for it in '
        'perf.core.perf_data_generator.NON_TELEMETRY_BENCHMARKS.' % test_name)

  for message in error_messages:
    print >> outstream, '*', textwrap.fill(message, 70), '\n'

  return not error_messages


# Verify that all benchmarks have owners except those on the whitelist.
def _verify_benchmark_owners(benchmark_metadatas):
  unowned_benchmarks = set()
  for benchmark_name in benchmark_metadatas:
    if benchmark_metadatas[benchmark_name].emails is None:
      unowned_benchmarks.add(benchmark_name)

  assert not unowned_benchmarks, (
      'All benchmarks must have owners. Please add owners for the following '
      'benchmarks:\n%s' % '\n'.join(unowned_benchmarks))


def update_benchmark_csv(file_path):
  """Updates go/chrome-benchmarks.

  Updates telemetry/perf/benchmark.csv containing the current benchmark names,
  owners, and components. Requires that all benchmarks have owners.
  """
  header_data = [['AUTOGENERATED FILE DO NOT EDIT'],
      ['See https://bit.ly/update-benchmarks-info to make changes'],
      ['Benchmark name', 'Individual owners', 'Component', 'Documentation',
       'Tags']
  ]

  csv_data = []
  benchmark_metadatas = merge_dicts(
      NON_TELEMETRY_BENCHMARKS, TELEMETRY_PERF_BENCHMARKS,
      NON_WATERFALL_BENCHMARKS)
  _verify_benchmark_owners(benchmark_metadatas)

  undocumented_benchmarks = set()
  for benchmark_name in benchmark_metadatas:
    if not benchmark_metadatas[benchmark_name].documentation_url:
      undocumented_benchmarks.add(benchmark_name)
    csv_data.append([
        benchmark_name,
        benchmark_metadatas[benchmark_name].emails,
        benchmark_metadatas[benchmark_name].component,
        benchmark_metadatas[benchmark_name].documentation_url,
        benchmark_metadatas[benchmark_name].tags,
    ])
  if undocumented_benchmarks != ub_module.UNDOCUMENTED_BENCHMARKS:
    error_message = (
      'The list of known undocumented benchmarks does not reflect the actual '
      'ones.\n')
    if undocumented_benchmarks - ub_module.UNDOCUMENTED_BENCHMARKS:
      error_message += (
          'New undocumented benchmarks found. Please document them before '
          'enabling on perf waterfall: %s' % (
            ','.join(b for b in undocumented_benchmarks -
                     ub_module.UNDOCUMENTED_BENCHMARKS)))
    if ub_module.UNDOCUMENTED_BENCHMARKS - undocumented_benchmarks:
      error_message += (
          'These benchmarks are already documented. Please remove them from '
          'the UNDOCUMENTED_BENCHMARKS list in undocumented_benchmarks.py: %s' %
          (','.join(b for b in ub_module.UNDOCUMENTED_BENCHMARKS -
                    undocumented_benchmarks)))

    raise ValueError(error_message)

  csv_data = sorted(csv_data, key=lambda b: b[0])
  csv_data = header_data + csv_data

  with open(file_path, 'wb') as f:
    writer = csv.writer(f, lineterminator="\n")
    writer.writerows(csv_data)


def update_labs_docs_md(filepath):
  configs = collections.defaultdict(list)
  for tester in bot_platforms.ALL_PLATFORMS:
    if not tester.is_fyi:
      configs[tester.platform].append(tester)

  with open(filepath, 'w') as f:
    f.write("""
[comment]: # (AUTOGENERATED FILE DO NOT EDIT)
[comment]: # (See //tools/perf/generate_perf_data to make changes)

# Platforms tested in the Performance Lab

""")
    for platform, testers in sorted(configs.iteritems()):
      f.write('## %s\n\n' % platform.title())
      testers.sort()
      for tester in testers:
        f.write(' * [{0.name}]({0.buildbot_url}): {0.description}.\n'.format(
            tester))
      f.write('\n')


def validate_tests(waterfall, waterfall_file, fyi_waterfall_file,
                   benchmark_file, labs_docs_file):
  up_to_date = True

  waterfall_tempfile = tempfile.NamedTemporaryFile(delete=False).name
  fyi_waterfall_tempfile = tempfile.NamedTemporaryFile(delete=False).name
  benchmark_tempfile = tempfile.NamedTemporaryFile(delete=False).name
  labs_docs_tempfile = tempfile.NamedTemporaryFile(delete=False).name

  try:
    update_all_tests(waterfall, waterfall_tempfile)
    up_to_date &= filecmp.cmp(waterfall_file, waterfall_tempfile)

    shutil.copy(fyi_waterfall_file, fyi_waterfall_tempfile)
    load_and_update_fyi_json(fyi_waterfall_file)
    up_to_date &= filecmp.cmp(fyi_waterfall_tempfile, fyi_waterfall_file)

    update_benchmark_csv(benchmark_tempfile)
    up_to_date &= filecmp.cmp(benchmark_file, benchmark_tempfile)

    update_labs_docs_md(labs_docs_tempfile)
    up_to_date &= filecmp.cmp(labs_docs_file, labs_docs_tempfile)
  finally:
    os.remove(waterfall_tempfile)
    os.remove(fyi_waterfall_tempfile)
    os.remove(benchmark_tempfile)
    os.remove(labs_docs_tempfile)

  return up_to_date

def add_common_test_properties(test_entry):
  test_entry['trigger_script'] = {
      'requires_simultaneous_shard_dispatch': True,
      'script': '//testing/trigger_scripts/perf_device_trigger.py',
      'args': [
          '--multiple-dimension-script-verbose',
          'True'
      ],
  }

  test_entry['merge'] = {
      'script': '//tools/perf/process_perf_results.py',
  }

def generate_telemetry_args(tester_config):
  # First determine the browser that you need based on the tester
  browser_name = ''
  # For trybot testing we always use the reference build
  if tester_config.get('testing', False):
    browser_name = 'reference'
  elif tester_config['platform'] == 'android':
    browser_name = 'android-chromium'
  elif tester_config['platform'].startswith('android-'):
    browser_name = tester_config['platform']
  elif (tester_config['platform'] == 'win'
    and tester_config['target_bits'] == 64):
    browser_name = 'release_x64'
  else:
    browser_name ='release'

  test_args = [
    '-v',
    '--browser=%s' % browser_name,
    '--upload-results'
  ]

  if browser_name.startswith('android-webview'):
    test_args.append(
        '--webview-embedder-apk=../../out/Release/apks/SystemWebViewShell.apk')

  return test_args


def generate_non_telemetry_args(test_name):
  # --gtest-benchmark-name so the benchmark name is consistent with the test
  # step's name. This is not always the same as the test binary's name (see
  # crbug.com/870692).
  return [
    '--gtest-benchmark-name', test_name,
  ]


def generate_performance_test(tester_config, test):
  isolate_name = test['isolate']

  test_suite = test.get('test_suite', isolate_name)

  if test.get('telemetry', True):
    test_args = generate_telemetry_args(tester_config)
  else:
    test_args = generate_non_telemetry_args(test_name=test_suite)
  # Append any additional args specific to an isolate
  test_args += test.get('extra_args', [])

  result = {
    'args': test_args,
    'isolate_name': isolate_name,
    'name': test_suite,
    'override_compile_targets': [
      isolate_name
    ]
  }
  # For now we either get shards from the number of devices specified
  # or a test entry needs to specify the num shards if it supports
  # soft device affinity.
  add_common_test_properties(result)
  shards = test.get('num_shards')
  result['swarming'] = {
    # Always say this is true regardless of whether the tester
    # supports swarming. It doesn't hurt.
    'can_use_on_swarming_builders': True,
    'expiration': 2 * 60 * 60, # 2 hours pending max
    # TODO(crbug.com/865538): once we have plenty of windows hardwares,
    # to shards perf benchmarks on Win builders, reduce this hard timeout limit
    # to ~2 hrs.
    'hard_timeout': 10 * 60 * 60, # 10 hours timeout for full suite
    'ignore_task_failure': False,
    'io_timeout': 30 * 60, # 30 minutes
    'dimension_sets': [
      tester_config['dimension']
    ],
    'shards': shards,
  }
  return result


def load_and_update_fyi_json(fyi_waterfall_file):
  tests = {}

  with open(fyi_waterfall_file) as fp_r:
    tests = json.load(fp_r)
    tests['AAAAA1 SEMI-AUTOGENERATED FILE. EDIT CAREFULLY'] = {}
    tests['AAAAA2 Run ./tools/perf/generate_perf_data --validate to see '] = {}
    tests['AAAAA3 if your changes will stick. '] = {}
  with open(fyi_waterfall_file, 'w') as fp:
    # We have loaded what is there, we want to update or add
    # what we have listed here
    generate_telemetry_tests(NEW_PERF_RECIPE_FYI_TESTERS, tests)
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')


def generate_telemetry_tests(testers, tests):
  for tester, tester_config in testers['testers'].iteritems():
    telemetry_tests = []
    gtest_tests = []
    for test in tester_config['tests']:
      generated_script = generate_performance_test(tester_config, test)
      if test.get('telemetry', True):
        telemetry_tests.append(generated_script)
      else:
        gtest_tests.append(generated_script)
    telemetry_tests.sort(key=lambda x: x['name'])
    gtest_tests.sort(key=lambda x: x['name'])
    tests[tester] = {
      # Put Telemetry tests as the end since they tend to run longer to avoid
      # starving gtests (see crbug.com/873389).
      'isolated_scripts': gtest_tests + telemetry_tests
    }


def main(args):
  parser = argparse.ArgumentParser(
      description=('Generate perf test\' json config and benchmark.csv. '
                   'This needs to be done anytime you add/remove any existing'
                   'benchmarks in tools/perf/benchmarks.'))
  parser.add_argument(
      '--validate-only', action='store_true', default=False,
      help=('Validate whether the perf json generated will be the same as the '
            'existing configs. This does not change the contain of existing '
            'configs'))
  options = parser.parse_args(args)

  perf_waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.json')
  fyi_waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.fyi.json')

  benchmark_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'tools', 'perf', 'benchmark.csv')

  labs_docs_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'docs', 'speed', 'perf_lab_platforms.md')

  return_code = 0

  if options.validate_only:
    if validate_tests(get_waterfall_builder_config(),
                      perf_waterfall_file, fyi_waterfall_file, benchmark_file,
                      labs_docs_file):
      print 'All the perf config files are up-to-date. \\o/'
      return 0
    else:
      print ('Not all perf config files are up-to-date. Please run %s '
             'to update them.') % sys.argv[0]
      return 1
  else:
    load_and_update_fyi_json(fyi_waterfall_file)
    update_all_tests(get_waterfall_builder_config(), perf_waterfall_file)
    update_benchmark_csv(benchmark_file)
    update_labs_docs_md(labs_docs_file)
    if not is_perf_benchmarks_scheduling_valid(
        perf_waterfall_file, outstream=sys.stderr):
      return_code = 1

  return return_code
