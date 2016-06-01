# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import logging
import os
import shutil

import chrome_cache
import common_util
import loading_trace
import request_track
import sandwich_metrics
import sandwich_runner
import task_manager


def _BuildPatchedCache(original_cache_run_path, original_cache_archive_path,
      cache_archive_dest_path):
  CACHE_CONTROL_VALUE = 'max-age=0,stale-while-revalidate=315360000'
  trace_path = os.path.join(
      original_cache_run_path, '0', sandwich_runner.TRACE_FILENAME)
  trace = loading_trace.LoadingTrace.FromJsonFile(trace_path)
  patch_count = 0
  with common_util.TemporaryDirectory(prefix='sandwich_tmp') as tmp_path:
    cache_path = os.path.join(tmp_path, 'cache')
    chrome_cache.UnzipDirectoryContent(original_cache_archive_path, cache_path)
    cache_backend = chrome_cache.CacheBackend(cache_path, 'simple')
    cache_keys = set(cache_backend.ListKeys())
    for request in trace.request_track.GetEvents():
      if request.url not in cache_keys:
        continue
      caching_policy = request_track.CachingPolicy(request)
      assert caching_policy.IsCacheable()
      freshness = caching_policy.GetFreshnessLifetimes()
      if freshness[0] == 0:
        continue
      request.SetHTTPResponseHeader('cache-control', CACHE_CONTROL_VALUE)
      raw_headers = request.GetRawResponseHeaders()
      cache_backend.UpdateRawResponseHeaders(request.url, raw_headers)
      patch_count += 1
    chrome_cache.ZipDirectoryContent(cache_path, cache_archive_dest_path)
  logging.info('Patched %d cached resources out of %d' % (
      patch_count, len(cache_keys)))


class StaleWhileRevalidateBenchmarkBuilder(task_manager.Builder):
  """A builder for a graph of tasks for Stale-While-Revalidate study benchmarks.
  """

  def __init__(self, common_builder):
    task_manager.Builder.__init__(self,
                                  common_builder.output_directory,
                                  common_builder.output_subdirectory)
    self._common_builder = common_builder
    self._patched_cache_task = None
    self._PopulateCommonPipelines()

  def _PopulateCommonPipelines(self):
    """Creates necessary tasks to produce initial cache archives.

    Here is the full dependency tree for the returned task:
    common/patched-cache.zip
      depends on: common/original-cache.zip
        depends on: common/webpages.wpr
    """
    @self.RegisterTask('common/original-cache.zip',
                       dependencies=[self._common_builder.original_wpr_task])
    def BuildOriginalCache():
      runner = self._common_builder.CreateSandwichRunner()
      runner.wpr_archive_path = self._common_builder.original_wpr_task.path
      runner.cache_archive_path = BuildOriginalCache.path
      runner.cache_operation = sandwich_runner.CacheOperation.SAVE
      runner.output_dir = BuildOriginalCache.run_path
      runner.Run()
    BuildOriginalCache.run_path = BuildOriginalCache.path[:-4] + '-run'

    @self.RegisterTask('common/patched-cache.zip',
                       dependencies=[BuildOriginalCache])
    def BuildPatchedCache():
      _BuildPatchedCache(BuildOriginalCache.run_path, BuildOriginalCache.path,
                         BuildPatchedCache.path)

    self._patched_cache_task = BuildPatchedCache

  def PopulateBenchmark(self, enable_swr, transformer_list_name,
                        transformer_list):
    """Populate benchmarking tasks.

    Args:
      enable_swr: Enable SWR patching or not.
      transformer_list_name: A string describing the transformers, will be used
          in Task names (prefer names without spaces and special characters).
      transformer_list: An ordered list of function that takes an instance of
          SandwichRunner as parameter, would be applied immediately before
          SandwichRunner.Run() in the given order.

    Here is the full dependency of the added tree for the returned task:
    <transformer_list_name>/{swr,worstcase}-metrics.csv
      depends on: <transformer_list_name>/{swr,worstcase}-run/
        depends on: some tasks saved by PopulateCommonPipelines()
    """
    additional_column_names = ['url', 'repeat_id']

    task_prefix = os.path.join(transformer_list_name, '')
    if enable_swr:
      task_prefix += 'swr'
    else:
      task_prefix += 'worstcase'

    @self.RegisterTask(task_prefix + '-run/', [self._patched_cache_task])
    def RunBenchmark():
      runner = self._common_builder.CreateSandwichRunner()
      for transformer in transformer_list:
        transformer(runner)
      runner.wpr_archive_path = self._common_builder.original_wpr_task.path
      runner.wpr_out_log_path = os.path.join(
          RunBenchmark.path, sandwich_runner.WPR_LOG_FILENAME)
      runner.cache_archive_path = self._patched_cache_task.path
      runner.cache_operation = sandwich_runner.CacheOperation.PUSH
      runner.output_dir = RunBenchmark.path
      if enable_swr:
        runner.chrome_args.append('--enable-features=StaleWhileRevalidate2')
      runner.Run()

    @self.RegisterTask(task_prefix + '-metrics.csv', [RunBenchmark])
    def ExtractMetrics():
      run_metrics_list = []
      for repeat_id, repeat_dir in sandwich_runner.WalkRepeatedRuns(
          RunBenchmark.path):
        trace_path = os.path.join(repeat_dir, sandwich_runner.TRACE_FILENAME)
        logging.info('processing trace: %s', trace_path)
        trace = loading_trace.LoadingTrace.FromJsonFile(trace_path)
        run_metrics = {
            'url': trace.url,
            'repeat_id': repeat_id,
        }
        run_metrics.update(
            sandwich_metrics.ExtractCommonMetricsFromRepeatDirectory(
                repeat_dir, trace))
        run_metrics_list.append(run_metrics)

      run_metrics_list.sort(key=lambda e: e['repeat_id'])
      with open(ExtractMetrics.path, 'w') as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=(additional_column_names +
                                    sandwich_metrics.COMMON_CSV_COLUMN_NAMES))
        writer.writeheader()
        for trace_metrics in run_metrics_list:
          writer.writerow(trace_metrics)

    self._common_builder.default_final_tasks.append(ExtractMetrics)
