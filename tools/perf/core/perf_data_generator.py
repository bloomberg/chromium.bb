#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=too-many-lines

"""Script to generate chromium.perf.json in
the src/testing/buildbot directory and benchmark.csv in the src/tools/perf
directory. Maintaining these files by hand is too unwieldy.
Note: chromium.perf.fyi.json is updated manuall for now until crbug.com/757933
is complete.
"""
import argparse
import collections
import csv
import filecmp
import json
import os
import re
import sys
import sets
import tempfile


from core import path_util
from core import undocumented_benchmarks as ub_module
path_util.AddTelemetryToPath()

from telemetry import benchmark as benchmark_module
from telemetry import decorators

from py_utils import discover


_UNSCHEDULED_TELEMETRY_BENCHMARKS = set([
  'experimental.startup.android.coldish',
  'experimental.startup.mobile',
  ])

# Additional compile targets to add to builders.
# On desktop builders, chromedriver is added as an additional compile target.
# The perf waterfall builds this target for each commit, and the resulting
# ChromeDriver is archived together with Chrome for use in bisecting.
# This can be used by Chrome test team, as well as by google3 teams for
# bisecting Chrome builds with their web tests. For questions or to report
# issues, please contact johnchen@chromium.org and stgao@chromium.org.
BUILDER_ADDITIONAL_COMPILE_TARGETS = {
    'Android Compile Perf': ['microdump_stackwalk', 'angle_perftests'],
    'Android arm64 Compile Perf': ['microdump_stackwalk', 'angle_perftests'],
    'Linux Builder Perf': ['chromedriver'],
    'Mac Builder Perf': ['chromedriver'],
    'Win Builder Perf': ['chromedriver'],
    'Win x64 Builder Perf': ['chromedriver'],
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
NEW_PERF_RECIPE_FYI_TESTERS = {
  'testers' : {
    'OBBS Mac 10.12 Perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'extra_args': [
            '--run-ref-build',
            '--test-shard-map-filename=mac1012_5_shard_map.json',
          ],
          'num_shards': 5
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
        }
      ],
      'platform': 'mac',
      'dimension': {
        'pool': 'chrome.tests.perf-fyi',
        'os': 'Mac-10.12',
        'gpu': '8086:0a2e'
      },
    },
    'One Buildbot Step Test Builder': {
      'tests': [
        {
          'isolate': 'telemetry_perf_tests_without_chrome',
          'extra_args': [
            '--xvfb',
            '--run-ref-build',
            '--test-shard-map-filename=benchmark_bot_map.json'
          ],
          'num_shards': 3
        },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        }
      ],
      'platform': 'linux',
      'dimension': {
        'gpu': 'none',
        'pool': 'chrome.tests.perf-fyi',
        'os': 'Linux',
      },
      'testing': True,
    },
    'android-pixel2_webview-perf': {
      'tests': [
        {
          'isolate': 'performance_webview_test_suite',
          'extra_args': [
            '--test-shard-map-filename=android_pixel2_webview_shard_map.json',
          ],
          'num_shards': 7
        }
      ],
      'platform': 'android-webview',
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
            '--test-shard-map-filename=android_pixel2_shard_map.json',
          ],
          'num_shards': 7
        }
      ],
      'platform': 'android',
      'dimension': {
        'pool': 'chrome.tests.perf-fyi',
        'os': 'Android',
        'device_type': 'walleye',
        'device_os': 'O',
        'device_os_flavor': 'google',
      },
    }
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
            '--test-shard-map-filename=android_go_shard_map.json',
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
    'android-nexus5x-perf': {
      'tests': [
        {
          'isolate': 'performance_test_suite',
          'num_shards': 16,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=android_nexus5x_16_shard_map.json',
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
              '--test-shard-map-filename=android_nexus5_16_shard_map.json',
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
        {
          'isolate': 'angle_perftests',
          'num_shards': 1,
          'telemetry': False,
          'extra_args': [
              '--shard-timeout=300'
          ],
        }
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
              '--test-shard-map-filename=android_nexus5x_webview_16_shard_map.json',
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
          'num_shards': 16,
          'extra_args': [
              '--test-shard-map-filename=android_nexus6_webview_shard_map.json',
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
          'num_shards': 5,
          'extra_args': [
              '--run-ref-build',
              '--test-shard-map-filename=win10_shard_map.json',
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
          'isolate': 'views_perftests',
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
              '--test-shard-map-filename=win7_shard_map.json',
          ],
        },
        # crbug.com/735679 enable performance_browser_tests
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
              '--test-shard-map-filename=win7_nvidia_shard_map.json',
              '--assert-gpu-compositing',
          ],
        },
        # crbug.com/735679 enable performance_browser_tests
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
       # crbug.com/868675
       # {
       #   'isolate': 'performance_test_suite',
       #   'num_shards': 26,
       #   'extra_args': [
       #       '--run-ref-build',
       #       '--test-shard-map-filename=mac_1012_low_end_26_shard_map.json',
       #       '--assert-gpu-compositing',
       #   ],
       # },
        {
          'isolate': 'load_library_perf_tests',
          'num_shards': 1,
          'telemetry': False,
        },
        {
          'isolate': 'performance_browser_tests',
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
              '--test-shard-map-filename=linux_perf_shard_map.json',
              '--assert-gpu-compositing',
          ],
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
       # crbug.com/868675
       # {
       #   'isolate': 'performance_test_suite',
       #   'extra_args': [
       #     '--run-ref-build',
       #     '--test-shard-map-filename=mac_1013_high_end_26_shard_map.json',
       #       '--assert-gpu-compositing',
       #   ],
       #   'num_shards': 26
       # },
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


def current_benchmarks():
  benchmarks_dir = os.path.join(
      path_util.GetChromiumSrcDir(), 'tools', 'perf', 'benchmarks')
  top_level_dir = os.path.dirname(benchmarks_dir)

  all_benchmarks = []

  for b in discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark,
      index_by_class_name=True).values():
    if not b.Name() in _UNSCHEDULED_TELEMETRY_BENCHMARKS:
      all_benchmarks.append(b)

  return sorted(all_benchmarks, key=lambda b: b.Name())


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
  verify_all_tests_in_benchmark_csv(tests,
                                    get_all_waterfall_benchmarks_metadata())


# not_scheduled means this test is not scheduled on any of the chromium.perf
# waterfalls. Right now, all the below benchmarks are scheduled, but some other
# benchmarks are not scheduled, because they're disabled on all platforms.
BenchmarkMetadata = collections.namedtuple(
    'BenchmarkMetadata', 'emails component documentation_url not_scheduled')
NON_TELEMETRY_BENCHMARKS = {
    'angle_perftests': BenchmarkMetadata(
        'jmadill@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU>ANGLE', None, False),
    'validating_command_buffer_perftests': BenchmarkMetadata(
        'piman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU', None, False),
    'passthrough_command_buffer_perftests': BenchmarkMetadata(
        'piman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU>ANGLE', None, False),
    'net_perftests': BenchmarkMetadata(
        'xunjieli@chromium.org', None, None, False),
    'gpu_perftests': BenchmarkMetadata(
        'reveman@chromium.org, chrome-gpu-perf-owners@chromium.org',
        'Internals>GPU', None, False),
    'tracing_perftests': BenchmarkMetadata(
        'kkraynov@chromium.org, primiano@chromium.org', None, None, False),
    'load_library_perf_tests': BenchmarkMetadata(
        'xhwang@chromium.org, crouleau@chromium.org',
        'Internals>Media>Encrypted', None, False),
    'media_perftests': BenchmarkMetadata(
        'crouleau@chromium.org', None, None, False),
    'performance_browser_tests': BenchmarkMetadata(
        'miu@chromium.org', None, None, False),
    'views_perftests': BenchmarkMetadata(
        'tapted@chromium.org', 'Internals>Views', None, False),
    'components_perftests': BenchmarkMetadata(
        'csharrison@chromium.org', None, None, False)
}


# If you change this dictionary, run tools/perf/generate_perf_data
NON_WATERFALL_BENCHMARKS = {
    'sizes (mac)': BenchmarkMetadata('tapted@chromium.org', None, None, False),
    'sizes (win)': BenchmarkMetadata('grt@chromium.org', None, None, False),
    'sizes (linux)': BenchmarkMetadata(
        'thestig@chromium.org', None, None, False),
    'resource_sizes': BenchmarkMetadata(
        'agrieve@chromium.org, rnephew@chromium.org, perezju@chromium.org',
        None, None, False),
    'supersize_archive': BenchmarkMetadata(
        'agrieve@chromium.org', None, None, False),
}


# Returns a dictionary mapping waterfall benchmark name to benchmark owner
# metadata
def get_all_waterfall_benchmarks_metadata():
  return get_all_benchmarks_metadata(NON_TELEMETRY_BENCHMARKS)


def get_all_benchmarks_metadata(metadata):
  benchmark_list = current_benchmarks()

  for benchmark in benchmark_list:
    emails = decorators.GetEmails(benchmark)
    if emails:
      emails = ', '.join(emails)
    metadata[benchmark.Name()] = BenchmarkMetadata(
        emails, decorators.GetComponent(benchmark),
        decorators.GetDocumentationLink(benchmark), False)
  return metadata

# With migration to new recipe tests are now listed in the shard maps
# that live in tools/perf/core.  We need to verify off of that list.
def get_tests_in_performance_test_suite():
  tests = sets.Set()
  add_benchmarks_from_sharding_map(
      tests, "shard_maps/linux_perf_shard_map.json")
  add_benchmarks_from_sharding_map(
      tests, "shard_maps/pixel2_7_shard_map.json")
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


def verify_all_tests_in_benchmark_csv(tests, benchmark_metadata):
  benchmark_names = sets.Set(benchmark_metadata)
  test_names = get_tests_in_performance_test_suite()

  for t in tests:
    scripts = []
    if 'isolated_scripts' in tests[t]:
      scripts = tests[t]['isolated_scripts']
    elif 'scripts' in tests[t]:
      scripts = tests[t]['scripts']
    else:
      assert(t in BUILDER_ADDITIONAL_COMPILE_TARGETS
             or t.startswith('AAAAA')), 'Unknown test data %s' % t
    for s in scripts:
      name = s['name']
      name = re.sub('\\.reference$', '', name)
      # TODO(eyaich): Determine new way to generate ownership based
      # on the benchmark bot map instead of on the generated tests
      # for new perf recipe.
      if (name is 'performance_test_suite'
          or name is 'performance_webview_test_suite'):
        continue
      test_names.add(name)


  # Disabled tests are filtered out of the waterfall json. Add them back here.
  for name, data in benchmark_metadata.items():
    if data.not_scheduled:
      test_names.add(name)

  error_messages = []
  for test in benchmark_names - test_names:
    error_messages.append('Remove ' + test + ' from NON_TELEMETRY_BENCHMARKS')
  for test in test_names - benchmark_names:
    error_messages.append('Add ' + test + ' to NON_TELEMETRY_BENCHMARKS')

  assert benchmark_names == test_names, ('Please update '
      'NON_TELEMETRY_BENCHMARKS as below:\n' + '\n'.join(error_messages))

  _verify_benchmark_owners(benchmark_metadata)


# Verify that all benchmarks have owners except those on the whitelist.
def _verify_benchmark_owners(benchmark_metadata):
  unowned_benchmarks = set()
  for benchmark_name in benchmark_metadata:
    if benchmark_metadata[benchmark_name].emails is None:
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
      ['See //tools/perf/generate_perf_data.py to make changes'],
      ['Benchmark name', 'Individual owners', 'Component', 'Documentation']
  ]

  csv_data = []
  all_benchmarks = NON_TELEMETRY_BENCHMARKS
  all_benchmarks.update(NON_WATERFALL_BENCHMARKS)
  benchmark_metadata = get_all_benchmarks_metadata(all_benchmarks)
  _verify_benchmark_owners(benchmark_metadata)

  undocumented_benchmarks = set()
  for benchmark_name in benchmark_metadata:
    if not benchmark_metadata[benchmark_name].documentation_url:
      undocumented_benchmarks.add(benchmark_name)
    csv_data.append([
        benchmark_name,
        benchmark_metadata[benchmark_name].emails,
        benchmark_metadata[benchmark_name].component,
        benchmark_metadata[benchmark_name].documentation_url,
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
          'These benchmarks are already documented. Please remove them from'
          'the UNDOCUMENTED_BENCHMARKS list in undocumented_benchmarks.py: %s' %
          (','.join(b for b in ub_module.UNDOCUMENTED_BENCHMARKS -
                    undocumented_benchmarks)))

    raise ValueError(error_message)

  csv_data = sorted(csv_data, key=lambda b: b[0])
  csv_data = header_data + csv_data

  with open(file_path, 'wb') as f:
    writer = csv.writer(f, lineterminator="\n")
    writer.writerows(csv_data)


def validate_tests(waterfall, waterfall_file, benchmark_file):
  up_to_date = True

  waterfall_tempfile = tempfile.NamedTemporaryFile(delete=False).name
  benchmark_tempfile = tempfile.NamedTemporaryFile(delete=False).name

  try:
    update_all_tests(waterfall, waterfall_tempfile)
    up_to_date &= filecmp.cmp(waterfall_file, waterfall_tempfile)

    update_benchmark_csv(benchmark_tempfile)
    up_to_date &= filecmp.cmp(benchmark_file, benchmark_tempfile)
  finally:
    os.remove(waterfall_tempfile)
    os.remove(benchmark_tempfile)

  return up_to_date

def add_common_test_properties(test_entry, tester_config):
  test_entry['trigger_script'] = {
      'script': '//testing/trigger_scripts/perf_device_trigger.py',
      'args': [
          '--multiple-dimension-script-verbose',
          'True'
      ],
  }

  if tester_config['platform'] == 'win':
    service_account_path = (
        'C:\\creds\\service_accounts\\'
        'service-account-chromium-perf-histograms.json')
  else:
    service_account_path = (
        '/creds/service_accounts/service-account-chromium-perf-histograms.json')
  test_entry['merge'] = {
      'script': '//tools/perf/process_perf_results.py',
      'args': [
        '--service-account-file',
        service_account_path
      ],
  }

def generate_telemetry_args(tester_config):
  # First determine the browser that you need based on the tester
  browser_name = ''
  # For trybot testing we always use the reference build
  if tester_config.get('testing', False):
    browser_name = 'reference'
  elif tester_config['platform'] == 'android':
    browser_name = 'android-chromium'
  elif tester_config['platform'] == 'android-webview':
    browser_name = 'android-webview'
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

  if browser_name == 'android-webview':
    test_args.append(
        '--webview-embedder-apk=../../out/Release/apks/SystemWebViewShell.apk')

  return test_args

def generate_non_telemetry_args(test_name):
  # --gtest-benchmark-name so the benchmark name is consistent with the test
  # step's name. This is not always the same as the test binary's name (see
  # crbug.com/870692).
  # --non-telemetry tells run_performance_tests.py that this test needs
  #   to be executed differently
  # --migrated-test tells run_performance_test_wrapper that this has
  #   non-telemetry test has been migrated to the new recipe.
  return [
    '--gtest-benchmark-name', test_name,
    '--non-telemetry=true',
    '--migrated-test=true'
  ]

def generate_performance_test(tester_config, test):
  isolate_name = test['isolate']

  # Check to see if the name is different than the isolate
  test_suite = isolate_name
  if test.get('test_suite', False):
    test_suite = test['test_suite']

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
  add_common_test_properties(result, tester_config)
  shards = test.get('num_shards')
  result['swarming'] = {
    # Always say this is true regardless of whether the tester
    # supports swarming. It doesn't hurt.
    'can_use_on_swarming_builders': True,
    'expiration': 6 * 60 * 60, # 6 hours timeout
    'hard_timeout': 7 * 60 * 60, # 7 hours timeout for full suite
    'ignore_task_failure': False,
    'io_timeout': 30 * 60, # 30 minutes
    'dimension_sets': [
      tester_config['dimension']
    ],
    'upload_test_results': True,
    'shards': shards,
  }
  return result


def load_and_update_fyi_json(fyi_waterfall_file):
  tests = {}
  with open(fyi_waterfall_file) as fp_r:
    tests = json.load(fp_r)
  with open(fyi_waterfall_file, 'w') as fp:
    # We have loaded what is there, we want to update or add
    # what we have listed here
    generate_telemetry_tests(NEW_PERF_RECIPE_FYI_TESTERS, tests)
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')


def generate_telemetry_tests(testers, tests):
  for tester, tester_config in testers['testers'].iteritems():
    isolated_scripts = []
    for test in tester_config['tests']:
      isolated_scripts.append(generate_performance_test(tester_config, test))
    tests[tester] = {
      'isolated_scripts': sorted(isolated_scripts, key=lambda x: x['name'])
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

  waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.json')
  fyi_waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.fyi.json')

  benchmark_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'tools', 'perf', 'benchmark.csv')

  if options.validate_only:
    if validate_tests(get_waterfall_builder_config(),
                      waterfall_file, benchmark_file):
      print 'All the perf JSON config files are up-to-date. \\o/'
      return 0
    else:
      print ('The perf JSON config files are not up-to-date. Please run %s '
             'without --validate-only flag to update the perf JSON '
             'configs and benchmark.csv.') % sys.argv[0]
      return 1
  else:
    load_and_update_fyi_json(fyi_waterfall_file)
    update_all_tests(get_waterfall_builder_config(), waterfall_file)
    update_benchmark_csv(benchmark_file)
  return 0
