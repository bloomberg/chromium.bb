#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=too-many-lines

"""Script to generate chromium.perf.json and chromium.perf.fyi.json in
the src/testing/buildbot directory and benchmark.csv in the src/tools/perf
directory. Maintaining these files by hand is too unwieldy.
"""
import argparse
import collections
import csv
import json
import os
import re
import sys
import sets

from core import path_util
path_util.AddTelemetryToPath()

from telemetry import benchmark as benchmark_module
from telemetry.core import discover
from telemetry.util import bot_utils
from telemetry import decorators


SCRIPT_TESTS = [
  {
    'args': [
      'gpu_perftests',
      '--adb-path',
      'src/third_party/catapult/devil/bin/deps/linux2/x86_64/bin/adb',
    ],
    'name': 'gpu_perftests',
    'script': 'gtest_perf_test.py',
    'testers': {
      'chromium.perf': [
        {
          'name': 'Android Nexus5 Perf',
          'shards': [2]
        },
        {
          'name': 'Android Nexus7v2 Perf',
          'shards': [2]
        }
        # crbug.com/663762
        #{
        #  'name': 'Android Nexus9 Perf',
        #  'shards': [2]
        #}
      ],
    }
  },
  {
    'args': [
      'cc_perftests',
      '--adb-path',
      'src/third_party/catapult/devil/bin/deps/linux2/x86_64/bin/adb',
    ],
    'name': 'cc_perftests',
    'script': 'gtest_perf_test.py',
    'testers': {
      'chromium.perf': [
        # crbug.com/698831
        # {
        #   'name': 'Android Nexus5 Perf',
        #   'shards': [2]
        # },
        # {
        #   'name': 'Android Nexus6 Perf',
        #   'shards': [2]
        # },
        # {
        #   'name': 'Android Nexus7v2 Perf',
        #   'shards': [2]
        # },
        {
          'name': 'Android Nexus9 Perf',
          'shards': [2]
        },
      ],
    }
  },
  {
    'args': [
      'tracing_perftests',
      '--adb-path',
      'src/third_party/catapult/devil/bin/deps/linux2/x86_64/bin/adb',
    ],
    'name': 'tracing_perftests',
    'script': 'gtest_perf_test.py',
    'testers': {
      'chromium.perf': [
        {
          'name': 'Android Nexus5 Perf',
          'shards': [2]
        },
        {
          'name': 'Android Nexus6 Perf',
          'shards': [2]
        },
        {
          'name': 'Android Nexus7v2 Perf',
          'shards': [2]
        },
        {
          'name': 'Android Nexus9 Perf',
          'shards': [2]
        },
      ]
    }
  },
]


def add_builder(waterfall, name, additional_compile_targets=None):
  waterfall['builders'][name] = added = {}
  if additional_compile_targets:
    added['additional_compile_targets'] = additional_compile_targets

  return waterfall

def add_tester(waterfall, name, perf_id, platform, target_bits=64,
              num_host_shards=1, num_device_shards=1, swarming=None):
  del perf_id # this will be needed
  waterfall['testers'][name] = {
    'platform': platform,
    'num_device_shards': num_device_shards,
    'num_host_shards': num_host_shards,
    'target_bits': target_bits,
  }

  if swarming:
    waterfall['testers'][name]['swarming_dimensions'] = swarming
    waterfall['testers'][name]['swarming'] = True

  return waterfall


def get_fyi_waterfall_config():
  waterfall = {'builders':{}, 'testers': {}}
  waterfall = add_tester(
    waterfall, 'Win 10 Low-End Perf Tests',
    'win-10-low-end', 'win',
    swarming=[
      {
       'gpu': '1002:9874',
       'os': 'Windows-10-10586',
       'pool': 'Chrome-perf-fyi',
       'device_ids': [
           'build171-b4', 'build186-b4', 'build202-b4', 'build203-b4',
           'build204-b4', 'build205-b4', 'build206-b4', 'build207-b4',
           'build208-b4', 'build209-b4', 'build210-b4', 'build211-b4',
           'build212-b4', 'build213-b4', 'build214-b4', 'build215-b4',
           'build216-b4', 'build217-b4', 'build218-b4', 'build219-b4',
           'build220-b4', 'build221-b4']
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 10 4 Core Low-End Perf Tests',
    'win-10-4-core-low-end', 'win',
    swarming=[
      {
       'gpu': '8086:22b1',
       'os': 'Windows-10-10586',
       'pool': 'Chrome-perf-fyi',
       'device_ids': [
           'build136-b1', 'build137-b1', 'build138-b1', 'build139-b1',
           'build140-b1', 'build141-b1', 'build142-b1', 'build143-b1',
           'build144-b1', 'build145-b1', 'build146-b1', 'build147-b1',
           'build148-b1', 'build149-b1', 'build150-b1', 'build151-b1',
           'build152-b1', 'build153-b1', 'build154-b1', 'build155-b1',
           'build47-b4', 'build48-b4'],
       'perf_tests': [
         ('cc_perftests', 0),
         ('gpu_perftests', 0),
         ('load_library_perf_tests', 0),
         ('angle_perftests', 1),
         ('performance_browser_tests', 1),
         ('tracing_perftests', 1)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Android Swarming N5X Tester',
    'fyi-android-swarming-n5x', 'android',
    swarming=[
      {
       'os': 'Android',
       'android_devices': '1',
       'pool': 'Chrome-perf-fyi',
       'device_ids': [
           'build245-m4--device1', 'build245-m4--device2',
           'build245-m4--device3', 'build245-m4--device4',
           'build245-m4--device5', 'build245-m4--device6',
           'build245-m4--device7', 'build248-m4--device1',
           'build248-m4--device2', 'build248-m4--device3',
           'build248-m4--device4', 'build248-m4--device5',
           'build248-m4--device6', 'build248-m4--device7',
           'build249-m4--device1', 'build249-m4--device2',
           'build249-m4--device3', 'build249-m4--device4',
           'build249-m4--device5', 'build249-m4--device6',
           'build249-m4--device7'
        ]
      }
    ])
  return waterfall


def get_waterfall_config():
  waterfall = {'builders':{}, 'testers': {}}

  waterfall = add_builder(
      waterfall, 'Android Compile', additional_compile_targets=[
          'microdump_stackwalk'
      ])
  waterfall = add_builder(
      waterfall, 'Android arm64 Compile', additional_compile_targets=[
          'microdump_stackwalk'
      ])

  # These configurations are taken from chromium_perf.py in
  # build/scripts/slave/recipe_modules/chromium_tests and must be kept in sync
  # to generate the correct json for each tester
  waterfall = add_tester(
    waterfall, 'Android Nexus5 Perf', 'android-nexus5',
    'android', target_bits=32, num_device_shards=7, num_host_shards=3)
  waterfall = add_tester(
    waterfall, 'Android Nexus5X Perf', 'android-nexus5X',
    'android', target_bits=32, num_device_shards=7, num_host_shards=3)
  waterfall = add_tester(
    waterfall, 'Android Nexus6 Perf', 'android-nexus6',
    'android', target_bits=32, num_device_shards=7, num_host_shards=3)
  waterfall = add_tester(
    waterfall, 'Android Nexus7v2 Perf', 'android-nexus7v2',
   'android', target_bits=32, num_device_shards=7, num_host_shards=3)
  waterfall = add_tester(
    waterfall, 'Android One Perf', 'android-one',
    'android', target_bits=32, num_device_shards=7, num_host_shards=3)

  waterfall = add_tester(
    waterfall, 'Android Nexus5X Perf', 'android-nexus5X', 'android',
    swarming=[
      {
       'os': 'Android',
       'android_devices': '1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build73-b1--device1', 'build73-b1--device2', 'build73-b1--device3',
           'build73-b1--device4', 'build73-b1--device5', 'build73-b1--device6',
           'build73-b1--device7',
           'build74-b1--device1', 'build74-b1--device2', 'build74-b1--device3',
           'build74-b1--device4', 'build74-b1--device5', 'build74-b1--device6',
           'build74-b1--device7',
           'build75-b1--device1', 'build75-b1--device2', 'build75-b1--device3',
           'build75-b1--device4', 'build75-b1--device5', 'build75-b1--device6',
           'build75-b1--device7',
          ]
      }
    ])

  waterfall = add_tester(
    waterfall, 'Win Zenbook Perf', 'win-zenbook', 'win',
    swarming=[
      {
       'gpu': '8086:161e',
       'os': 'Windows-10-10240',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build30-b1', 'build31-b1',
           'build32-b1', 'build33-b1', 'build34-b1'
          ]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 10 High-DPI Perf', 'win-high-dpi', 'win',
    swarming=[
      {
       'gpu': '8086:1616',
       'os': 'Windows-10-10240',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build117-b1', 'build118-b1',
           'build119-b1', 'build120-b1',
           'build180-b4' # Added in https://crbug.com/695613
          ]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 10 Perf', 'chromium-rel-win10', 'win',
    swarming=[
      {
       'gpu': '102b:0534',
       'os': 'Windows-10-10240',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build132-m1', 'build133-m1',
           'build134-m1', 'build135-m1', 'build136-m1'
          ],
       'perf_tests': [
         ('media_perftests', 2)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 8 Perf', 'chromium-rel-win8-dual', 'win',
    swarming=[
      {
       'gpu': '102b:0532',
       'os': 'Windows-2012ServerR2-SP0',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build143-m1', 'build144-m1',
           'build145-m1', 'build146-m1', 'build147-m1'
          ],
       'perf_tests': [
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2),
         ('media_perftests', 3)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 7 Perf', 'chromium-rel-win7-dual',
    'win', target_bits=32,
    swarming=[
      {
       'gpu': '102b:0532',
       'os': 'Windows-2008ServerR2-SP1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build185-m1', 'build186-m1',
           'build187-m1', 'build188-m1', 'build189-m1'
          ],
       'perf_tests': [
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2),
         ('media_perftests', 3)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 7 x64 Perf',
    'chromium-rel-win7-x64-dual', 'win',
    swarming=[
      {
       'gpu': '102b:0532',
       'os': 'Windows-2008ServerR2-SP1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build138-m1', 'build139-m1',
           'build140-m1', 'build141-m1', 'build142-m1'
          ],
       'perf_tests': [
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 7 ATI GPU Perf',
    'chromium-rel-win7-gpu-ati', 'win',
    swarming=[
      {
       'gpu': '1002:6613',
       'os': 'Windows-2008ServerR2-SP1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build101-m1', 'build102-m1',
           'build103-m1', 'build104-m1', 'build105-m1'
          ],
       'perf_tests': [
         ('angle_perftests', 2),
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2),
         ('media_perftests', 3)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 7 Intel GPU Perf',
    'chromium-rel-win7-gpu-intel', 'win',
    swarming=[
      {
       'gpu': '8086:041a',
       'os': 'Windows-2008ServerR2-SP1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build164-m1', 'build165-m1',
           'build166-m1', 'build167-m1', 'build168-m1'
          ],
       'perf_tests': [
         ('angle_perftests', 2),
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Win 7 Nvidia GPU Perf',
    'chromium-rel-win7-gpu-nvidia', 'win',
    swarming=[
      {
       'gpu': '10de:104a',
       'os': 'Windows-2008ServerR2-SP1',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build92-m1', 'build93-m1',
           'build94-m1', 'build95-m1', 'build96-m1'
          ],
       'perf_tests': [
         ('angle_perftests', 2),
         ('load_library_perf_tests', 2),
         ('performance_browser_tests', 2),
         ('media_perftests', 3)]
      }
    ])

  waterfall = add_tester(
    waterfall, 'Mac 10.11 Perf', 'chromium-rel-mac11',
    'mac',
    swarming=[
      {
       'gpu': '8086:0166',
       'os': 'Mac-10.11',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build102-b1', 'build103-b1',
           'build104-b1', 'build105-b1', 'build106-b1'
          ],
       'perf_tests': [
         ('media_perftests', 3)]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Mac 10.12 Perf', 'chromium-rel-mac12',
    'mac',
    swarming=[
      {
       'os': 'Mac-10.12',
       'gpu': '8086:0a2e',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build158-m1', 'build159-m1', 'build160-m1',
           'build161-m1', 'build162-m1']
      }
    ])
  waterfall = add_tester(
    waterfall, 'Mac Retina Perf',
    'chromium-rel-mac-retina', 'mac',
    swarming=[
      {
       'gpu': '8086:0d26',
       'os': 'Mac-10.11',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build4-b1', 'build5-b1', 'build6-b1', 'build7-b1', 'build8-b1'
          ]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Mac Pro 10.11 Perf',
    'chromium-rel-mac11-pro', 'mac',
    swarming=[
      {
       'gpu': '1002:6821',
       'os': 'Mac-10.11',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build128-b1', 'build129-b1',
           'build130-b1', 'build131-b1', 'build132-b1'
          ]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Mac Air 10.11 Perf',
    'chromium-rel-mac11-air', 'mac',
    swarming=[
      {
       'gpu': '8086:1626',
       'os': 'Mac-10.11',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build123-b1', 'build124-b1',
           'build125-b1', 'build126-b1', 'build127-b1'
          ]
      }
    ])
  waterfall = add_tester(
    waterfall, 'Mac Mini 8GB 10.12 Perf',
    'chromium-rel-mac12-mini-8gb', 'mac',
    swarming=[
      {
       'gpu': '8086:0a26',
       'os': 'Mac-10.12',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build24-b1', 'build25-b1',
           'build26-b1', 'build27-b1', 'build28-b1'
          ]
      }
    ])

  waterfall = add_tester(
    waterfall, 'Linux Perf', 'linux-release', 'linux',
    swarming=[
      {
       'gpu': '102b:0534',
       'os': 'Ubuntu-14.04',
       'pool': 'Chrome-perf',
       'device_ids': [
           'build148-m1', 'build149-m1',
           'build150-m1', 'build151-m1', 'build152-m1'
          ],
       'perf_tests': [
         # crbug.com/698831
         # ('cc_perftests', 2),
         # crbug.com/709274
         # ('load_library_perf_tests', 2),
         ('tracing_perftests', 2),
         ('media_perftests', 3)]
      }
    ])

  return waterfall


def generate_isolate_script_entry(swarming_dimensions, test_args,
    isolate_name, step_name, ignore_task_failure,
    override_compile_targets=None,
    swarming_timeout=None):
  result = {
    'args': test_args,
    'isolate_name': isolate_name,
    'name': step_name,
  }
  if override_compile_targets:
    result['override_compile_targets'] = override_compile_targets
  if swarming_dimensions:
    result['swarming'] = {
      # Always say this is true regardless of whether the tester
      # supports swarming. It doesn't hurt.
      'can_use_on_swarming_builders': True,
      'expiration': 10 * 60 * 60, # 10 hour timeout for now (crbug.com/699312)
      'hard_timeout': swarming_timeout if swarming_timeout else 7200,
      'ignore_task_failure': ignore_task_failure,
      'io_timeout': 3600,
      'dimension_sets': swarming_dimensions,
    }
  return result


def generate_telemetry_test(swarming_dimensions, benchmark_name, browser):
  # The step name must end in 'test' or 'tests' in order for the
  # results to automatically show up on the flakiness dashboard.
  # (At least, this was true some time ago.) Continue to use this
  # naming convention for the time being to minimize changes.

  test_args = [
    benchmark_name,
    '-v',
    '--upload-results',
    '--output-format=chartjson',
    '--browser=%s' % browser
  ]
  # When this is enabled on more than just windows machines we will need
  # --device=android

  ignore_task_failure = False
  step_name = benchmark_name
  if browser == 'reference':
    test_args.append('--output-trace-tag=_ref')
    step_name += '.reference'
    # We ignore the failures on reference builds since there is little we can do
    # to fix them except waiting for the reference build to update.
    ignore_task_failure = True

  return generate_isolate_script_entry(
      swarming_dimensions, test_args, 'telemetry_perf_tests',
      step_name, ignore_task_failure=ignore_task_failure,
      override_compile_targets=['telemetry_perf_tests'],
      swarming_timeout=BENCHMARK_SWARMING_TIMEOUTS.get(benchmark_name))


def script_test_enabled_on_tester(master, test, tester_name, shard):
  for enabled_tester in test['testers'].get(master, []):
    if enabled_tester['name'] == tester_name:
      if shard in enabled_tester['shards']:
        return True
  return False


def generate_script_tests(master, tester_name, shard):
  script_tests = []
  for test in SCRIPT_TESTS:
    if script_test_enabled_on_tester(master, test, tester_name, shard):
      script = {
        'args': test['args'],
        'name': test['name'],
        'script': test['script']
      }
      script_tests.append(script)
  return script_tests


def get_swarming_dimension(dimension, device_affinity):
  complete_dimension = {
    'id': dimension['device_ids'][device_affinity],
    'os': dimension['os'],
    'pool': dimension['pool'],
  }
  if 'gpu' in dimension:
    complete_dimension['gpu'] = dimension['gpu']
  if 'android_devices' in dimension:
    complete_dimension['android_devices'] = dimension['android_devices']
  return complete_dimension


def generate_cplusplus_isolate_script_test(dimension):
  return [
    generate_isolate_script_entry(
        [get_swarming_dimension(dimension, shard)], [], name, name,
        ignore_task_failure=False)
    for name, shard in dimension['perf_tests']
  ]


def ShouldBenchmarkBeScheduled(benchmark, platform):
  disabled_tags = decorators.GetDisabledAttributes(benchmark)
  enabled_tags = decorators.GetEnabledAttributes(benchmark)

  # Don't run benchmarks which are disabled on all platforms.
  if 'all' in disabled_tags:
    return False

  # If we're not on android, don't run mobile benchmarks.
  if platform != 'android' and 'android' in enabled_tags:
    return False

  # If we're on android, don't run benchmarks disabled on mobile
  if platform == 'android' and 'android' in disabled_tags:
    return False

  return True

def generate_telemetry_tests(tester_config, benchmarks, benchmark_sharding_map,
                             benchmark_ref_build_blacklist):
  isolated_scripts = []
  # First determine the browser that you need based on the tester
  browser_name = ''
  if tester_config['platform'] == 'android':
    browser_name = 'android-chromium'
  elif (tester_config['platform'] == 'win'
    and tester_config['target_bits'] == 64):
    browser_name = 'release_x64'
  else:
    browser_name ='release'

  num_shards = len(tester_config['swarming_dimensions'][0]['device_ids'])
  current_shard = 0
  for benchmark in benchmarks:
    if not ShouldBenchmarkBeScheduled(benchmark, tester_config['platform']):
      continue

    # First figure out swarming dimensions this test needs to be triggered on.
    # For each set of dimensions it is only triggered on one of the devices
    swarming_dimensions = []
    for dimension in tester_config['swarming_dimensions']:
      sharding_map = benchmark_sharding_map.get(str(num_shards), None)
      if not sharding_map:
        raise Exception('Invalid number of shards, generate new sharding map')
      device_affinity = sharding_map.get(benchmark.Name(), None)
      if device_affinity is None:
        raise Exception('Device affinity for benchmark %s not found'
          % benchmark.Name())
      swarming_dimensions.append(
          get_swarming_dimension(dimension, device_affinity))

    test = generate_telemetry_test(
      swarming_dimensions, benchmark.Name(), browser_name)
    isolated_scripts.append(test)
    # Now create another executable for this benchmark on the reference browser
    # if it is not blacklisted from running on the reference browser.
    if benchmark.Name() not in benchmark_ref_build_blacklist:
      reference_test = generate_telemetry_test(
        swarming_dimensions, benchmark.Name(),'reference')
      isolated_scripts.append(reference_test)
      if current_shard == (num_shards - 1):
        current_shard = 0
      else:
        current_shard += 1

  return isolated_scripts


# List of benchmarks that are to never be run on a waterfall.
BENCHMARK_NAME_BLACKLIST = [
    'multipage_skpicture_printer',
    'multipage_skpicture_printer_ct',
    'rasterize_and_record_micro_ct',
    'repaint_ct',
    'multipage_skpicture_printer',
    'multipage_skpicture_printer_ct',
    'skpicture_printer',
    'skpicture_printer_ct',
]


# Overrides the default 2 hour timeout for swarming tasks.
BENCHMARK_SWARMING_TIMEOUTS = {
    'loading.mobile': 14400, # 4 hours
    'system_health.memory_mobile': 10800, # 4 hours
}


# List of benchmarks that are to never be run with reference builds.
BENCHMARK_REF_BUILD_BLACKLIST = [
  'power.idle_platform',
]



def current_benchmarks():
  benchmarks_dir = os.path.join(src_dir(), 'tools', 'perf', 'benchmarks')
  top_level_dir = os.path.dirname(benchmarks_dir)

  all_benchmarks = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark,
      index_by_class_name=True).values()
  # Remove all blacklisted benchmarks
  for blacklisted in BENCHMARK_NAME_BLACKLIST:
    for benchmark in all_benchmarks:
      if benchmark.Name() == blacklisted:
        all_benchmarks.remove(benchmark)
        break

  return sorted(all_benchmarks, key=lambda b: b.Name())


# Returns a sorted list of (benchmark, avg_runtime) pairs for every
# benchmark in the all_benchmarks list where avg_runtime is in seconds.  Also
# returns a list of benchmarks whose run time have not been seen before
def get_sorted_benchmark_list_by_time(all_benchmarks):
  runtime_list = []
  benchmark_avgs = {}
  new_benchmarks = []
  timing_file_path = os.path.join(src_dir(), 'tools', 'perf', 'core',
      'desktop_benchmark_avg_times.json')
  # Load in the avg times as calculated on Nov 1st, 2016
  with open(timing_file_path) as f:
    benchmark_avgs = json.load(f)

  for benchmark in all_benchmarks:
    benchmark_avg_time = benchmark_avgs.get(benchmark.Name(), None)
    if benchmark_avg_time is None:
      # Assume that this is a new benchmark that was added after 11/1/16 when
      # we generated the benchmarks. Use the old affinity algorithm after
      # we have given the rest the same distribution, add it to the
      # new benchmarks list.
      new_benchmarks.append(benchmark)
    else:
      # Need to multiple the seconds by 2 since we will be generating two tests
      # for each benchmark to be run on the same shard for the reference build
      runtime_list.append((benchmark, benchmark_avg_time * 2.0))

  # Return a reverse sorted list by runtime
  runtime_list.sort(key=lambda tup: tup[1], reverse=True)
  return runtime_list, new_benchmarks


# Returns a map of benchmark name to shard it is on.
def shard_benchmarks(num_shards, all_benchmarks):
  benchmark_to_shard_dict = {}
  shard_execution_times = [0] * num_shards
  sorted_benchmark_list, new_benchmarks = get_sorted_benchmark_list_by_time(
    all_benchmarks)
  # Iterate over in reverse order and add them to the current smallest bucket.
  for benchmark in sorted_benchmark_list:
    # Find current smallest bucket
    min_index = shard_execution_times.index(min(shard_execution_times))
    benchmark_to_shard_dict[benchmark[0].Name()] = min_index
    shard_execution_times[min_index] += benchmark[1]
  # For all the benchmarks that didn't have avg run times, use the default
  # device affinity algorithm
  for benchmark in new_benchmarks:
     device_affinity = bot_utils.GetDeviceAffinity(num_shards, benchmark.Name())
     benchmark_to_shard_dict[benchmark.Name()] = device_affinity
  return benchmark_to_shard_dict


def generate_all_tests(waterfall):
  tests = {}

  all_benchmarks = current_benchmarks()
  # Get benchmark sharding according to common sharding configurations
  # Currently we only have bots sharded 5 directions and 1 direction
  benchmark_sharding_map = {}
  benchmark_sharding_map['22'] = shard_benchmarks(22, all_benchmarks)
  benchmark_sharding_map['5'] = shard_benchmarks(5, all_benchmarks)
  benchmark_sharding_map['1'] = shard_benchmarks(1, all_benchmarks)
  benchmark_sharding_map['21'] = shard_benchmarks(21, all_benchmarks)

  for name, config in waterfall['testers'].iteritems():
    benchmark_list = all_benchmarks
    if config.get('swarming', False):
      # Our current configuration only ever has one set of swarming dimensions
      # Make sure this still holds true
      if len(config['swarming_dimensions']) > 1:
        raise Exception('Invalid assumption on number of swarming dimensions')
      # Generate benchmarks
      sharding_map = benchmark_sharding_map
      isolated_scripts = generate_telemetry_tests(
          config, benchmark_list, sharding_map,
          BENCHMARK_REF_BUILD_BLACKLIST)
      # Generate swarmed non-telemetry tests if present
      if config['swarming_dimensions'][0].get('perf_tests', False):
        isolated_scripts += generate_cplusplus_isolate_script_test(
          config['swarming_dimensions'][0])
      tests[name] = {
        'isolated_scripts': sorted(isolated_scripts, key=lambda x: x['name'])
      }
    else:
      # scripts are only currently run in addition to the main waterfall.  They
      # are currently the only thing generated in the perf json file.
      # TODO eyaich: will need to handle the sharding differently when we have
      # swarmed bots on the main waterfall.
      for shard in range(0, config['num_host_shards']):
        tester_name = '%s (%d)' % (name, shard + 1)
        scripts = generate_script_tests(waterfall['name'], name, shard + 1)
        if scripts:
          tests[tester_name] = {
            'scripts': sorted(scripts, key=lambda x: x['name'])
          }

  for name, config in waterfall['builders'].iteritems():
    tests[name] = config

  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See //tools/perf/generate_perf_data.py to make changes'] = {}
  return tests


def get_json_config_file_for_waterfall(waterfall):
  filename = '%s.json' % waterfall['name']
  buildbot_dir = os.path.join(src_dir(), 'testing', 'buildbot')
  return os.path.join(buildbot_dir, filename)


def tests_are_up_to_date(waterfalls):
  up_to_date = True
  all_tests = {}
  for w in waterfalls:
    tests = generate_all_tests(w)
    tests_data = json.dumps(tests, indent=2, separators=(',', ': '),
                            sort_keys=True)
    config_file = get_json_config_file_for_waterfall(w)
    with open(config_file, 'r') as fp:
      config_data = fp.read().strip()
    all_tests.update(tests)
    up_to_date &= tests_data == config_data
  verify_all_tests_in_benchmark_csv(all_tests,
                                    get_all_waterfall_benchmarks_metadata())
  return up_to_date


def update_all_tests(waterfalls):
  all_tests = {}
  for w in waterfalls:
    tests = generate_all_tests(w)
    config_file = get_json_config_file_for_waterfall(w)
    with open(config_file, 'w') as fp:
      json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
      fp.write('\n')
    all_tests.update(tests)
  verify_all_tests_in_benchmark_csv(all_tests,
                                    get_all_waterfall_benchmarks_metadata())


def src_dir():
  file_path = os.path.abspath(__file__)
  return os.path.dirname(os.path.dirname(
      os.path.dirname(os.path.dirname(file_path))))


# not_scheduled means this test is not scheduled on any of the chromium.perf
# waterfalls. Right now, all the below benchmarks are scheduled, but some other
# benchmarks are not scheduled, because they're disabled on all platforms.
BenchmarkMetadata = collections.namedtuple(
    'BenchmarkMetadata', 'emails component not_scheduled')
NON_TELEMETRY_BENCHMARKS = {
    'angle_perftests': BenchmarkMetadata('jmadill@chromium.org', None, False),
    'cc_perftests': BenchmarkMetadata('enne@chromium.org', None, False),
    'gpu_perftests': BenchmarkMetadata('reveman@chromium.org', None, False),
    'tracing_perftests': BenchmarkMetadata(
        'kkraynov@chromium.org, primiano@chromium.org', None, False),
    'load_library_perf_tests': BenchmarkMetadata(None, None, False),
    'media_perftests': BenchmarkMetadata('crouleau@chromium.org', None, False),
    'performance_browser_tests': BenchmarkMetadata(
        'hubbe@chromium.org, justinlin@chromium.org, miu@chromium.org', None,
        False)
}


# If you change this dictionary, run tools/perf/generate_perf_data
NON_WATERFALL_BENCHMARKS = {
    'sizes (mac)': BenchmarkMetadata('tapted@chromium.org', None, False),
    'sizes (win)': BenchmarkMetadata('grt@chromium.org', None, False),
    'sizes (linux)': BenchmarkMetadata('thestig@chromium.org', None, False),
    'resource_sizes': BenchmarkMetadata(
        'agrieve@chromium.org, rnephew@chromium.org, perezju@chromium.org',
        None, False)
}


# Returns a dictionary mapping waterfall benchmark name to benchmark owner
# metadata
def get_all_waterfall_benchmarks_metadata():
  return get_all_benchmarks_metadata(NON_TELEMETRY_BENCHMARKS)


def get_all_benchmarks_metadata(metadata):
  benchmark_list = current_benchmarks()

  for benchmark in benchmark_list:
    disabled = 'all' in decorators.GetDisabledAttributes(benchmark)

    emails = decorators.GetEmails(benchmark)
    if emails:
      emails = ', '.join(emails)
    metadata[benchmark.Name()] = BenchmarkMetadata(
        emails, decorators.GetComponent(benchmark), disabled)
  return metadata


def verify_all_tests_in_benchmark_csv(tests, benchmark_metadata):
  benchmark_names = sets.Set(benchmark_metadata)
  test_names = sets.Set()
  for t in tests:
    scripts = []
    if 'isolated_scripts' in tests[t]:
      scripts = tests[t]['isolated_scripts']
    elif 'scripts' in tests[t]:
      scripts = tests[t]['scripts']
    else:
      assert('Android Compile' == t
        or 'Android arm64 Compile' == t
        or t.startswith('AAAAA')), 'Unknown test data %s' % t
    for s in scripts:
      name = s['name']
      name = re.sub('\\.reference$', '', name)
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


def update_benchmark_csv():
  """Updates go/chrome-benchmarks.

  Updates telemetry/perf/benchmark.csv containing the current benchmark names,
  owners, and components.
  """
  header_data = [['AUTOGENERATED FILE DO NOT EDIT'],
      ['See //tools/perf/generate_perf_data.py to make changes'],
      ['Benchmark name', 'Individual owners', 'Component']
  ]

  csv_data = []
  all_benchmarks = NON_TELEMETRY_BENCHMARKS
  all_benchmarks.update(NON_WATERFALL_BENCHMARKS)
  benchmark_metadata = get_all_benchmarks_metadata(all_benchmarks)
  for benchmark_name in benchmark_metadata:
    csv_data.append([
        benchmark_name,
        benchmark_metadata[benchmark_name].emails,
        benchmark_metadata[benchmark_name].component
    ])

  csv_data = sorted(csv_data, key=lambda b: b[0])
  csv_data = header_data + csv_data

  perf_dir = os.path.join(src_dir(), 'tools', 'perf')
  benchmark_file = os.path.join(perf_dir, 'benchmark.csv')
  with open(benchmark_file, 'wb') as f:
    writer = csv.writer(f, lineterminator="\n")
    writer.writerows(csv_data)


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

  waterfall = get_waterfall_config()
  waterfall['name'] = 'chromium.perf'
  fyi_waterfall = get_fyi_waterfall_config()
  fyi_waterfall['name'] = 'chromium.perf.fyi'

  if options.validate_only:
    if tests_are_up_to_date([fyi_waterfall, waterfall]):
      print 'All the perf JSON config files are up-to-date. \\o/'
      return 0
    else:
      print ('The perf JSON config files are not up-to-date. Please run %s '
             'without --validate-only flag to update the perf JSON '
             'configs and benchmark.csv.') % sys.argv[0]
      return 1
  else:
    update_all_tests([fyi_waterfall, waterfall])
    update_benchmark_csv()
  return 0
