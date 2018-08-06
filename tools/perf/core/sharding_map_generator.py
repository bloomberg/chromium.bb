# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate files to control which bots run which tests.

The files are JSON dictionaries which map shard index to a list of benchmarks
and benchmarks indexed by story. E.g.

{
  "0": {
    "benchmarks": [
      "power.desktop": {},
      "rendering.mobile": {
        "begin": 0,
        "end": 104
      },
      ...
    ],
  }
}

This will be used to manually shard tests to certain bots, to more efficiently
execute all our tests.

Example usage:

Generating sharding maps:
tools/perf/core/retrieve_story_timing.py --output-file all_desktop_perf.json
    -c 'linux-perf' -c 'Win 10 High-DPI Perf' -c 'Win 10 Perf' -c 'Win 7 Perf'
    -c 'Win 7 Intel GPU Perf' -c 'Win 7 Nvidia GPU Perf' -c 'Mac 10.12 Perf'
    -c 'Mac Pro 10.11 Perf' -c 'Mac Air 10.11 Perf'
    -c 'mac-10_12_laptop_low_end-perf'
tools/perf/generate_perf_sharding
    --output-file tools/perf/core/desktop_sharding_map.json
    --num-shards 26 --timing-data all_desktop_perf.json
rm all_desktop_perf.json

tools/perf/core/retrieve_story_timing.py --output-file all_mobile_perf.json
    -c 'Android Nexus 5X Perf' -c 'Android Nexus5 Perf' -c 'Android One Perf'
    -c 'Android Nexus5X WebView Perf' -c 'Android Nexus6 WebView Perf'
tools/perf/generate_perf_sharding
    --output-file tools/perf/core/mobile_sharding_map.json
    --num-shards 39 --timing-data all_mobile_perf.json
rm all_mobile_perf.json

Generating and testing sharding maps:
tools/perf/core/retrieve_story_timing.py --output-file all_desktop_perf.json
    -c 'linux-perf' -c 'Win 10 High-DPI Perf' -c 'Win 10 Perf' -c 'Win 7 Perf'
    -c 'Win 7 Intel GPU Perf' -c 'Win 7 Nvidia GPU Perf' -c 'Mac 10.12 Perf'
    -c 'Mac Pro 10.11 Perf' -c 'Mac Air 10.11 Perf'
    -c 'mac-10_12_laptop_low_end-perf'
tools/perf/core/retrieve_story_timing.py --output-file win_10_test_data.json
    -c 'Win 10 High-DPI Perf' --build-number 1924
tools/perf/generate_perf_sharding
    --output-file tools/perf/core/desktop_sharding_map.json
    --num-shards 26 --timing-data all_desktop_perf.json
    --test-data win_10_test_data.json
rm all_desktop_perf.json
rm win_10_test_data.json

"""

import argparse
import collections
import json
import sys

import core.path_util

core.path_util.AddTelemetryToPath()


def main(options, benchmarks_data):
  """
    benchmarks_data is a dictionary of all benchmarks to be sharded. Its
    structure is as follows:
    {
      "benchmark_1": {
         "stories": [ "storyA", "storyB",...],
         "repeat": <number of pageset_repeat> },
      "benchmark_2": {
         ...
      },...
    }

    The "stories" field contains a list of ordered story names. Notes that
    this should match the actual order of how the benchmark stories are
    executed for the sharding algorithm to be effective.
  """
  story_timing_ordered_dict = _load_timing_data_from_file(
      benchmarks_data, options.timing_data, True)

  all_stories = {}
  for b in benchmarks_data:
    all_stories[b] = benchmarks_data[b]['stories']

  sharding_map = generate_sharding_map(story_timing_ordered_dict,
      all_stories, options.num_shards, options.debug)

  with open(options.output_file, 'w') as output_file:
    json.dump(sharding_map, output_file, indent = 4, separators=(',', ': '))

  if options.test_data:
    story_timing_ordered_dict = _load_timing_data_from_file(
        benchmarks_data, options.test_data, False)
    test_results = test_sharding_map(
        options.output_file, story_timing_ordered_dict, all_stories)
    if options.test_data_output:
      with open(options.test_data_output, 'w') as output_file:
        json.dump(test_results, output_file, indent = 4, separators=(',', ': '))

    else:
      print test_results


def get_parser():
  parser = argparse.ArgumentParser(
      description='Generate perf test sharding map.')
  parser.add_argument(
      '--output-file', action='store', required=True,
      help='The filename to write the sharding map to.')
  parser.add_argument(
      '--num-shards', action='store', required=True, type=int,
      help='The number of shards to write to.')
  parser.add_argument(
      '--timing-data', action='store', required=True,
      help='The file to read timing data from.')
  # This json file should contain a list of dicts containing
  #    the name and duration of the stories. For example:
  #   [ { "duration": "98.039", "name": "webrtc/multiple_connections"},
  #     { "duration": "85.118", "name": "webrtc/pause_connections"} ]

  parser.add_argument(
      '--test-data', action='store',
      help='If specified, test the generated sharding map with this data.')
  parser.add_argument(
      '--test-data-output', action='store',
      help='If specified with --test-data, file \
          to output the tested timing data to.')
  parser.add_argument(
      '--debug', action='store',
      help='If specified, the filename to write extra timing data to.')
  parser.add_argument(
      '--benchmarks',
      help='Comma separated list of benchmark names to generate a map for',
      required=False)
  return parser


def generate_sharding_map(
    story_timing_ordered_dict, all_stories, num_shards, debug):

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


def _load_timing_data_from_file(benchmarks_data, timing_data_file, repeat):
  story_timing_ordered_dict = _init_timing_dict_for_benchmarks(benchmarks_data)
  with open(timing_data_file, 'r') as timing_data_file:
    story_timing = json.load(timing_data_file)
    for run in story_timing:
      benchmark = run['name'].split('/', 1)[0]
      if run['name'] in story_timing_ordered_dict:
        if run['duration']:
          if repeat:
            story_timing_ordered_dict[run['name']] = (float(run['duration'])
                * benchmarks_data[benchmark]['repeat'])
          else:
            story_timing_ordered_dict[run['name']] += float(run['duration'])
  return story_timing_ordered_dict


def _init_timing_dict_for_benchmarks(benchmarks_data):
  timing_data = collections.OrderedDict()
  for b in benchmarks_data:
    story_list = benchmarks_data[b]['stories']
    for story in story_list:
      timing_data[b + '/' + story] = 0
  return timing_data


def _generate_empty_sharding_map(num_shards):
  sharding_map = collections.OrderedDict()
  for i in range(0, num_shards):
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}
  return sharding_map


def test_sharding_map(sharding_map_file, timing_data, all_stories):
  results = collections.OrderedDict()

  with open(sharding_map_file) as f:
    sharding_map = json.load(f, object_pairs_hook=collections.OrderedDict)
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
          story_timing = timing_data.get(benchmark_name + "/" + story, 0)
          results[shard][benchmark_name + "/" + story] = str(story_timing)
          benchmark_timing += story_timing
        shard_total_time += benchmark_timing
      results[shard]['full_time'] = shard_total_time
  return results
