# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate files to control which bots run which tests.

The files are JSON dictionaries which map shard index to a list of benchmarks
and benchmarks indexed by story. E.g.

{
  "0": {
    "benchmarks": [
      "battor.steady_state": {},
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
from collections import OrderedDict
import core.path_util
import json
import optparse

core.path_util.AddTelemetryToPath()

from telemetry.internal.browser import browser_options


def main(args, benchmarks):
  story_timing_dict = _load_timing_data_from_file(benchmarks, args.timing_data)

  all_stories = {}
  for b in benchmarks:
    all_stories[b.Name()] = _get_stories_for_benchmark(b)

  sharding_map = generate_sharding_map(story_timing_dict,
      all_stories, args.num_shards, args.debug)

  with open(args.output_file, 'w') as output_file:
    json.dump(sharding_map, output_file, indent = 4, separators=(',', ': '))

  if args.test_data:
    print test_sharding_map(args.output_file, args.test_data)


def get_args():
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
  #   [ { "avg_duration": "98.039", "name": "webrtc/multiple_connections"},
  #     { "avg_duration": "85.118", "name": "webrtc/pause_connections"} ]

  parser.add_argument(
      '--test-data', action='store',
      help='If specified, test the generated sharding map with this data.')
  parser.add_argument(
      '--debug', action='store',
      help='If specified, the filename to write extra timing data to.')
  return parser


def generate_sharding_map(story_timing_dict, all_stories, num_shards, debug):
  max_time_per_shard = _get_max_time_per_shard(story_timing_dict, num_shards)

  total_time = 0
  sharding_map = OrderedDict()
  debug_map = OrderedDict()
  for i in range(num_shards):
    sharding_map[str(i)] = {'benchmarks': OrderedDict()}
    debug_map[str(i)] = OrderedDict()
    time_per_shard = 0
    stories_in_shard = []
    while total_time < max_time_per_shard * (i + 1) and len(
        story_timing_dict) > 0:
      (story, time) = story_timing_dict.popitem(last=False)
      total_time += time
      time_per_shard += time
      stories_in_shard.append(story)
      debug_map[str(i)][story] = time
    _add_benchmarks_to_shard(sharding_map, i, stories_in_shard, all_stories)
    debug_map[str(i)]['full_time'] = time_per_shard
  if debug:
    with open(debug, 'w') as output_file:
      json.dump(debug_map, output_file, indent = 4, separators=(',', ': '))

  return sharding_map


def _get_max_time_per_shard(timing_data, num_shards):
  total_run_time = 0
  for story in timing_data:
    total_run_time += timing_data[story]
  return total_run_time / num_shards


def _add_benchmarks_to_shard(sharding_map, shard_index, stories_in_shard,
    all_stories):
  benchmarks = OrderedDict()
  for story in stories_in_shard:
    (b, story) = story.split('/', 1)
    if b not in benchmarks:
      benchmarks[b] = []
    benchmarks[b].append(story)

  # Format the benchmark's stories by indices
  benchmarks_in_shard = OrderedDict()
  for b in benchmarks:
    benchmarks_in_shard[b] = {}
    first_story = all_stories[b].index(benchmarks[b][0])
    last_story = all_stories[b].index(benchmarks[b][-1]) + 1
    if first_story != 0:
      benchmarks_in_shard[b]['begin'] = first_story
    if last_story != len(all_stories[b]):
      benchmarks_in_shard[b]['end'] = last_story
  sharding_map[str(shard_index)] = {'benchmarks': benchmarks_in_shard}


def _load_timing_data_from_file(benchmarks, timing_data_file):
  story_timing_dict = _init_timing_dict_for_benchmarks(benchmarks)
  pageset_repeat_dict = _init_pageset_repeat_dict_for_benchmarks(benchmarks)
  with open(timing_data_file, 'r') as timing_data_file:
    story_timing = json.load(timing_data_file)
    for run in story_timing:
      benchmark = run['name'].split('/', 1)[0]
      if run['name'] in story_timing_dict:
        story_timing_dict[run['name']] = (float(run['avg_duration'])
            * pageset_repeat_dict[benchmark])
  return story_timing_dict


def _init_pageset_repeat_dict_for_benchmarks(benchmarks):
  pageset_repeat = {}
  for b in benchmarks:
    pageset_repeat[b.Name()] = b().options.get('pageset_repeat', 1)
  return pageset_repeat


def _init_timing_dict_for_benchmarks(benchmarks):
  timing_data = OrderedDict()
  for b in benchmarks:
    story_list = _get_stories_for_benchmark(b)
    for story in story_list:
      timing_data[b.Name() + '/' + story] = 0
  return timing_data


def _get_stories_for_benchmark(b):
    story_list = []
    benchmark = b()
    options = browser_options.BrowserFinderOptions()
    # Add default values for any extra commandline options
    # provided by the benchmark.
    parser = optparse.OptionParser()
    before, _ = parser.parse_args([])
    benchmark.AddBenchmarkCommandLineArgs(parser)
    after, _ = parser.parse_args([])
    for extra_option in dir(after):
      if extra_option not in dir(before):
        setattr(options, extra_option, getattr(after, extra_option))
    for story in benchmark.CreateStorySet(options).stories:
      if story.name not in story_list:
        story_list.append(story.name)
    return story_list


def _generate_empty_sharding_map(num_shards):
  sharding_map = OrderedDict()
  for i in range(0, num_shards):
    sharding_map[str(i)] = {'benchmarks': OrderedDict()}
  return sharding_map


def test_sharding_map(sharding_map_file, test_data):
  results = OrderedDict()
  timing_data = OrderedDict()
  with open(test_data, 'r') as story_timing_file:
    story_timing = json.load(story_timing_file)
    last_story = None
    for run in story_timing:
      benchmark = run['run_name'].split('/', 1)[0]
      if benchmark not in timing_data:
        timing_data[benchmark] = []
      if run['run_name'] == last_story:
        timing_data[benchmark][-1] += float(run['run_time'])
      else:
        timing_data[benchmark].append(float(run['run_time']))
      last_story = run['run_name']

  with open(sharding_map_file) as f:
    sharding_map = json.load(f, object_pairs_hook=OrderedDict)
    for shard in sharding_map:
      shard_total_time = 0
      for benchmark_name in sharding_map[shard]['benchmarks']:
        if benchmark_name in timing_data:
          benchmark = sharding_map[shard]['benchmarks'][benchmark_name]
          begin = 0
          end = len(timing_data[benchmark_name])
          if 'begin' in benchmark:
            begin = benchmark['begin']
          if 'end' in benchmark:
            end = benchmark['end']
          time = sum(timing_data[benchmark_name][begin : end])
          shard_total_time += time
      results[shard] = shard_total_time
  return results
