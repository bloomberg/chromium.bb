# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os
import unittest

from core import sharding_map_generator


class TestShardingMapGenerator(unittest.TestCase):

  def _generate_test_data(self, times):
    timing_data = []
    benchmarks_data = []
    for i, _ in enumerate(times):
      b = {
        'name': 'benchmark_' + str(i),
        'stories': [],
        'repeat': 1,
      }
      benchmarks_data.append(b)
      story_times = times[i]
      for j, _ in enumerate(story_times):
        benchmark_name = 'benchmark_' + str(i)
        story_name = 'story_' + str(j)
        b['stories'].append(story_name)
        timing_data.append({
           'name': benchmark_name + '/' + story_name,
           'duration': story_times[j]
        })
    return benchmarks_data, timing_data

  def testGenerateAndTestShardingMap(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[60, 56, 57], [66, 54, 80, 4], [2, 8, 7, 37, 2]])
    timing_data_for_testing = copy.deepcopy(timing_data)
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None)
    results = sharding_map_generator.test_sharding_map(sharding_map,
        benchmarks_data, timing_data_for_testing)
    self.assertEqual(results['0']['full_time'], 116)
    self.assertEqual(results['1']['full_time'], 177)
    self.assertEqual(results['2']['full_time'], 140)

  def testGeneratePerfSharding(self):
    test_data_dir = os.path.join(os.path.dirname(__file__), 'test_data')
    with open(os.path.join(test_data_dir, 'benchmarks_to_shard.json')) as f:
      benchmarks_to_shard = json.load(f)

    with open(os.path.join(test_data_dir, 'test_timing_data.json')) as f:
      timing_data = json.load(f)

    with open(
        os.path.join(test_data_dir, 'test_timing_data_1_build.json')) as f:
      timing_data_single_build = json.load(f)

    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_to_shard, timing_data, num_shards=5, debug=False)

    results = sharding_map_generator.test_sharding_map(
        sharding_map, benchmarks_to_shard, timing_data_single_build)

    shards_timing = []
    for shard in results:
      shards_timing.append(results[shard]['full_time'])
    self.assertTrue(max(shards_timing) - min(shards_timing) < 300)
