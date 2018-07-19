# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import json

from core import path_util

_VALID_SWARMING_DIMENSIONS = {
    'gpu', 'device_ids', 'os', 'pool', 'perf_tests', 'perf_tests_with_args',
    'device_os', 'device_type', 'device_os_flavor', 'id'}
_VALID_PERF_POOLS = {
    'chrome.tests.perf', 'chrome.tests.perf-webview',
    'chrome.tests.perf-fyi', 'chrome.tests.perf-webview-fyi'}


def _ValidateSwarmingDimension(builder_name, swarming_dimensions):
  for dimension in swarming_dimensions:
    for k, v in dimension.iteritems():
      if k not in _VALID_SWARMING_DIMENSIONS:
        raise ValueError('Invalid swarming dimension in %s: %s' % (
            builder_name, k))
      if k == 'pool' and v not in _VALID_PERF_POOLS:
        raise ValueError('Invalid perf pool %s in %s' % (v, builder_name))
      if k == 'os' and v == 'Android':
        if (not 'device_type' in dimension.keys() or
            not 'device_os' in dimension.keys() or
            not 'device_os_flavor' in dimension.keys()):
          raise ValueError(
              'Invalid android dimensions %s in %s' % (v, builder_name))


def _ParseShardMapFileName(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--test-shard-map-filename', dest='shard_file')
  options, _ = parser.parse_known_args(args)
  return options.shard_file


def _ParseBrowserFlags(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--browser')
  parser.add_argument('--webview-embedder-apk')
  options, _ = parser.parse_known_args(args)
  return options


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')


def _ValidateShardingData(builder_name, test_config):
  num_shards = test_config['swarming'].get('shards', 1)
  if num_shards == 1:
    return
  shard_file_name = _ParseShardMapFileName(test_config['args'])
  if not shard_file_name:
    raise ValueError('Must specify the shard map for case num shard >= 2')
  shard_file_path = os.path.join(_SHARD_MAP_DIR, shard_file_name)
  if not os.path.exists(shard_file_path):
    raise ValueError(
        "shard test file %s in config of builder %s does not exist" % (
          repr(shard_file_name), repr(builder_name)))

  with open(shard_file_path) as f:
    shard_map_data = json.load(f)

  shard_map_data.pop('extra_infos', None)
  shard_keys = set(shard_map_data.keys())
  expected_shard_keys = set([str(i) for i in xrange(num_shards)])
  if shard_keys != expected_shard_keys:
    raise ValueError(
        'The shard configuration of %s does not match the expected expected '
        'number of shards (%d) in config of builder %s' % (
            repr(shard_file_name), num_shards, repr(builder_name)))


def _ValidateBrowserType(builder_name, test_config):
  browser_options = _ParseBrowserFlags(test_config['args'])
  if 'WebView' in builder_name or 'webview' in builder_name:
    if browser_options.browser != 'android-webview':
      raise ValueError("%s must use 'android-webview' browser type" %
                       builder_name)
    if not browser_options.webview_embedder_apk:
      raise ValueError('%s must set --webview-embedder-apk flag' % builder_name)
  elif 'Android' in builder_name or 'android' in builder_name:
    if browser_options.browser != 'android-chromium':
      raise ValueError("%s must use 'android-chromium' browser" %
                       builder_name)
  elif builder_name in ('win-10-perf', 'Win 7 Nvidia GPU Perf'):
    if browser_options.browser != 'release_x64':
      raise ValueError("%s must use 'release_x64' browser type" %
                       builder_name)
  else:  # The rest must be desktop/laptop builders
    if browser_options.browser != 'release':
      raise ValueError("%s must use 'release' browser type" %
                       builder_name)


def ValidateTestingBuilder(builder_name, builder_data):
  isolated_scripts = builder_data['isolated_scripts']
  for test_config in isolated_scripts:
    _ValidateSwarmingDimension(
        builder_name,
        swarming_dimensions=test_config['swarming'].get('dimension_sets', {}))
    if (test_config['isolate_name'] in
        ('performance_test_suite', 'performance_webview_test_suite')):
      _ValidateShardingData(builder_name, test_config)
      _ValidateBrowserType(builder_name, test_config)


def _IsBuilderName(name):
  return not name.startswith('AAA')


def _IsCompilingBuilder(builder_name, builder_data):
  del builder_name  # unused
  return 'isolated_scripts' not in builder_data


def _IsTestingBuilder(builder_name, builder_data):
  del builder_name  # unused
  return 'isolated_scripts' in builder_data


def ValidatePerfConfigFile(file_handle):
  perf_data = json.load(file_handle)
  for key, value in perf_data.iteritems():
    if not _IsBuilderName(key):
      continue
    if _IsCompilingBuilder(builder_name=key, builder_data=value):
      pass
    elif _IsTestingBuilder(builder_name=key, builder_data=value):
      ValidateTestingBuilder(builder_name=key, builder_data=value)
    else:
      raise ValueError('%s has unrecognizable type: %s' % key)


def main(args):
  del args  # unused
  waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.json')
  fyi_waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.fyi.json')


  with open(fyi_waterfall_file) as f:
    ValidatePerfConfigFile(f)

  with open(waterfall_file) as f:
    ValidatePerfConfigFile(f)
