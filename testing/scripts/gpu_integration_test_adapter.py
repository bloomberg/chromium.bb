# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common

class GpuIntegrationTestAdapater(common.BaseIsolatedScriptArgsAdapter):

  def generate_test_output_args(self, output):
    return ['--write-full-results-to', output]

  def generate_test_also_run_disabled_tests_args(self):
    return ['--also-run-disabled-tests']

  def generate_test_filter_args(self, test_filter_str):
    filter_list = common.extract_filter_list(test_filter_str)
    # isolated_script_test_filter comes in like:
    #   gpu_tests.webgl_conformance_integration_test.WebGLConformanceIntegrationTest.WebglExtension_WEBGL_depth_texture  # pylint: disable=line-too-long
    # but we need to pass it to --test-filter like this:
    #   WebglExtension_WEBGL_depth_texture
    filter_list = [f.split('.')[-1] for f in filter_list]
    # Need to convert this to a valid regex.
    filter_regex = '(' + '|'.join(filter_list) + ')'
    return ['--test-filter=%s' % filter_regex]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-shards=%d' % total_shards,
            '--shard-index=%d' % shard_index]

  def generate_test_launcher_retry_limit_args(self, retry_limit):
    return ['--retry-limit=%d' % retry_limit]

  def generate_test_repeat_args(self, repeat_count):
    return ['--repeat=%d' % repeat_count]
