#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for perf_device_trigger_unittest.py."""

import unittest

import perf_device_trigger

class Args(object):
  def __init__(self):
    self.shards = 1
    self.dump_json = ''
    self.multiple_trigger_configs = []
    self.multiple_dimension_script_verbose = False


class FakeTriggerer(perf_device_trigger.PerfDeviceTriggerer):
  def __init__(self, bot_configs):
    super(FakeTriggerer, self).__init__()
    self._bot_configs = bot_configs
    self._bot_statuses = []
    self._swarming_runs = []
    self._files = {}
    self._temp_file_id = 0

  def set_files(self, files):
    self._files = files

  def make_temp_file(self, prefix=None, suffix=None):
    result = prefix + str(self._temp_file_id) + suffix
    self._temp_file_id += 1
    return result

  def delete_temp_file(self, temp_file):
    pass

  def read_json_from_temp_file(self, temp_file):
    return self._files[temp_file]

  def write_json_to_file(self, merged_json, output_file):
    self._files[output_file] = merged_json

  def parse_bot_configs(self, args):
    pass

  def run_swarming(self, args, verbose):
    self._swarming_runs.append(args)


PERF_BOT1 = {
  'pool': 'Chrome-perf-fyi',
  'id': 'build1'
}

PERF_BOT2 = {
  'pool': 'Chrome-perf-fyi',
  'id': 'build2'
}

class UnitTest(unittest.TestCase):
  def basic_setup(self):
    triggerer = FakeTriggerer(
      [
        PERF_BOT1,
        PERF_BOT2
      ]
    )
    # Note: the contents of these JSON files don't accurately reflect
    # that produced by "swarming.py trigger". The unit tests only
    # verify that shard 0's JSON is preserved.
    triggerer.set_files({
      'base_trigger_dimensions0.json': {
        'base_task_name': 'webgl_conformance_tests',
        'request': {
          'expiration_secs': 3600,
          'properties': {
            'execution_timeout_secs': 3600,
          },
        },
        'tasks': {
          'webgl_conformance_tests on NVIDIA GPU on Windows': {
            'task_id': 'f001',
          },
        },
      },
      'base_trigger_dimensions1.json': {
        'tasks': {
          'webgl_conformance_tests on NVIDIA GPU on Windows': {
            'task_id': 'f002',
          },
        },
      },
    })
    args = Args()
    args.shards = 2
    args.dump_json = 'output.json'
    args.multiple_dimension_script_verbose = False
    triggerer.trigger_tasks(
      args,
      [
        'trigger',
        '--dimension',
        'pool',
        'chrome-perf-fyi',
        '--dimension',
        'id',
        'build1',
        '--',
        'benchmark1',
      ])
    return triggerer

  def list_contains_sublist(self, main_list, sub_list):
    return any(sub_list == main_list[offset:offset + len(sub_list)]
               for offset in xrange(len(main_list) - (len(sub_list) - 1)))

  def test_shard_env_vars_and_bot_id(self):
    triggerer = self.basic_setup()
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[0], ['--bot', 'build1']))
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[1], ['--bot', 'build2']))
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[0], ['--env', 'GTEST_SHARD_INDEX', '0']))
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[1], ['--env', 'GTEST_SHARD_INDEX', '1']))
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[0], ['--env', 'GTEST_TOTAL_SHARDS', '2']))
    self.assertTrue(self.list_contains_sublist(
      triggerer._swarming_runs[1], ['--env', 'GTEST_TOTAL_SHARDS', '2']))

  def test_json_merging(self):
    triggerer = self.basic_setup()
    self.assertTrue('output.json' in triggerer._files)
    output_json = triggerer._files['output.json']
    self.assertTrue('base_task_name' in output_json)
    self.assertTrue('request' in output_json)
    self.assertEqual(output_json['request']['expiration_secs'], 3600)
    self.assertEqual(
      output_json['request']['properties']['execution_timeout_secs'], 3600)

if __name__ == '__main__':
  unittest.main()
