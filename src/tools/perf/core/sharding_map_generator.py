# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import json
import sys

import core.path_util

core.path_util.AddTelemetryToPath()



def generate_sharding_map(
    benchmarks_to_shard, timing_data, num_shards, debug):
  """Generate sharding map.

    Args:
      benchmarks_to_shard is a list all benchmarks to be sharded. Its
      structure is as follows:
      [{
         "name": "benchmark_1",
         "stories": [ "storyA", "storyB",...],
         "repeat": <number of pageset_repeat>
        },
        {
         "name": "benchmark_2",
         "stories": [ "storyA", "storyB",...],
         "repeat": <number of pageset_repeat>
        },
         ...
      ]

      The "stories" field contains a list of ordered story names. Notes that
      this should match the actual order of how the benchmark stories are
      executed for the sharding algorithm to be effective.

  """

  story_timing_ordered_dict = _gather_timing_data(
      benchmarks_to_shard, timing_data, True)

  all_stories = {}
  for b in benchmarks_to_shard:
    all_stories[b['name']] = b['stories']


  expected_time_per_shard = _get_expected_time_per_shard(
      story_timing_ordered_dict, num_shards)

  total_time = 0
  sharding_map = collections.OrderedDict()
  debug_map = collections.OrderedDict()
  min_shard_time = sys.maxint
  min_shard_index = None
  max_shard_time = 0
  max_shard_index = None
  num_stories = len(story_timing_ordered_dict)
  for i in range(num_shards):
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}
    debug_map[str(i)] = collections.OrderedDict()
    time_per_shard = 0
    stories_in_shard = []
    expected_total_time = expected_time_per_shard * (i + 1)
    last_diff = abs(total_time - expected_total_time)
    # Keep adding story to the current shard until the absolute difference
    # between the total time of shards so far and expected total time is
    # minimal.
    while (story_timing_ordered_dict and
           abs(total_time + story_timing_ordered_dict.items()[0][1] -
               expected_total_time) <= last_diff):
      (story, time) = story_timing_ordered_dict.popitem(last=False)
      total_time += time
      time_per_shard += time
      stories_in_shard.append(story)
      debug_map[str(i)][story] = time
      last_diff = abs(total_time - expected_total_time)
    _add_benchmarks_to_shard(sharding_map, i, stories_in_shard, all_stories)
    # Double time_per_shard to account for reference benchmark run.
    debug_map[str(i)]['expected_total_time'] = time_per_shard * 2
    if time_per_shard > max_shard_time:
      max_shard_time = time_per_shard
      max_shard_index = i
    if time_per_shard < min_shard_time:
      min_shard_time = time_per_shard
      min_shard_index = i

  if debug:
    with open(debug, 'w') as output_file:
      json.dump(debug_map, output_file, indent = 4, separators=(',', ': '))


  sharding_map['extra_infos'] = collections.OrderedDict([
      ('num_stories', num_stories),
      # Double all the time stats by 2 to account for reference build.
      ('predicted_min_shard_time', min_shard_time * 2),
      ('predicted_min_shard_index', min_shard_index),
      ('predicted_max_shard_time', max_shard_time * 2),
      ('predicted_max_shard_index', max_shard_index),
      ])
  return sharding_map


def _get_expected_time_per_shard(timing_data, num_shards):
  total_run_time = 0
  for story in timing_data:
    total_run_time += timing_data[story]
  return total_run_time / num_shards


def _add_benchmarks_to_shard(sharding_map, shard_index, stories_in_shard,
    all_stories):
  benchmarks = collections.OrderedDict()
  for story in stories_in_shard:
    (b, story) = story.split('/', 1)
    if b not in benchmarks:
      benchmarks[b] = []
    benchmarks[b].append(story)

  # Format the benchmark's stories by indices
  benchmarks_in_shard = collections.OrderedDict()
  for b in benchmarks:
    benchmarks_in_shard[b] = {}
    first_story = all_stories[b].index(benchmarks[b][0])
    last_story = all_stories[b].index(benchmarks[b][-1]) + 1
    if first_story != 0:
      benchmarks_in_shard[b]['begin'] = first_story
    if last_story != len(all_stories[b]):
      benchmarks_in_shard[b]['end'] = last_story
  sharding_map[str(shard_index)] = {'benchmarks': benchmarks_in_shard}


def _gather_timing_data(benchmarks_to_shard, timing_data, repeat):
  story_timing_ordered_dict = collections.OrderedDict()
  benchmarks_data_by_name = {}
  for b in benchmarks_to_shard:
    story_list = b['stories']
    benchmarks_data_by_name[b['name']] = b
    for story in story_list:
      story_timing_ordered_dict[b['name'] + '/' + story] = 0

  for run in timing_data:
    benchmark = run['name'].split('/', 1)[0]
    if run['name'] in story_timing_ordered_dict:
      if run['duration']:
        if repeat:
          story_timing_ordered_dict[run['name']] = (float(run['duration'])
              * benchmarks_data_by_name[benchmark]['repeat'])
        else:
          story_timing_ordered_dict[run['name']] += float(run['duration'])
  return story_timing_ordered_dict


def _generate_empty_sharding_map(num_shards):
  sharding_map = collections.OrderedDict()
  for i in range(0, num_shards):
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}
  return sharding_map


def test_sharding_map(
    sharding_map, benchmarks_to_shard, test_timing_data):
  story_timing_ordered_dict = _gather_timing_data(
      benchmarks_to_shard, test_timing_data, False)

  results = collections.OrderedDict()
  all_stories = {}
  for b in benchmarks_to_shard:
    all_stories[b['name']] = b['stories']

  sharding_map.pop('extra_infos', None)
  for shard in sharding_map:
    results[shard] = collections.OrderedDict()
    shard_total_time = 0
    for benchmark_name in sharding_map[shard]['benchmarks']:
      benchmark = sharding_map[shard]['benchmarks'][benchmark_name]
      begin = 0
      end = len(all_stories[benchmark_name])
      if 'begin' in benchmark:
        begin = benchmark['begin']
      if 'end' in benchmark:
        end = benchmark['end']
      benchmark_timing = 0
      for story in all_stories[benchmark_name][begin : end]:
        story_timing = story_timing_ordered_dict.get(
            benchmark_name + '/' + story, 0)
        results[shard][benchmark_name + '/' + story] = str(story_timing)
        benchmark_timing += story_timing
      shard_total_time += benchmark_timing
    results[shard]['full_time'] = shard_total_time
  return results
