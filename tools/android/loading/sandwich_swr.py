# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import os
import shutil

import common_util
import sandwich_metrics
import sandwich_runner
import task_manager


class StaleWhileRevalidateBenchmarkBuilder(task_manager.Builder):
  """A builder for a graph of tasks for Stale-While-Revalidate study benchmarks.
  """

  def __init__(self, common_builder):
    task_manager.Builder.__init__(self,
                                  common_builder.output_directory,
                                  common_builder.output_subdirectory)
    self._common_builder = common_builder
    self._worstcase_cache_task = None
    self._swr_cache_task = None
    self._PopulateCommonPipelines()

  def _PopulateCommonPipelines(self):
    """Creates necessary tasks to produce initial cache archives.

    Here is the full dependency tree for the returned task:
    common/swr-patched-cache.zip
      depends on: common/worstcase-patched-cache.zip
        depends on: common/webpages.wpr
    """
    @self.RegisterTask('common/original-cache.zip',
                       dependencies=[self._common_builder.original_wpr_task])
    def BuildOriginalCache():
      runner = self._common_builder.CreateSandwichRunner()
      runner.wpr_archive_path = self._common_builder.original_wpr_task.path
      runner.cache_archive_path = BuildOriginalCache.path
      runner.cache_operation = sandwich_runner.CacheOperation.SAVE
      runner.output_dir = BuildOriginalCache.path[:-4] + '-run'
      runner.Run()

    @self.RegisterTask('common/worstcase-patched-cache.zip',
                       dependencies=[BuildOriginalCache])
    def BuildWorstCaseCache():
      # TODO(gabadie): Patch cache-control's max-age=0 if any.
      shutil.copyfile(BuildOriginalCache.path, BuildWorstCaseCache.path)

    @self.RegisterTask('common/swr-patched-cache.zip', [BuildWorstCaseCache])
    def BuildSWRCache():
      # TODO(gabadie): Patch cache-control's stale-while-revalidate=1 year if
      # any max-age.
      shutil.copyfile(BuildWorstCaseCache.path, BuildSWRCache.path)

    self._worstcase_cache_task = BuildWorstCaseCache
    self._swr_cache_task = BuildSWRCache

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
    task_prefix = os.path.join(transformer_list_name, '')
    if enable_swr:
      cache_task = self._swr_cache_task
      task_prefix += 'swr'
    else:
      cache_task = self._worstcase_cache_task
      task_prefix += 'worstcase'

    @self.RegisterTask(task_prefix + '-run/', [cache_task])
    def RunBenchmark():
      runner = self._common_builder.CreateSandwichRunner()
      for transformer in transformer_list:
        transformer(runner)
      runner.wpr_archive_path = self._common_builder.original_wpr_task.path
      runner.wpr_out_log_path = os.path.join(
          RunBenchmark.path, sandwich_runner.WPR_LOG_FILENAME)
      runner.cache_archive_path = cache_task.path
      runner.cache_operation = sandwich_runner.CacheOperation.PUSH
      runner.output_dir = RunBenchmark.path
      runner.Run()

    @self.RegisterTask(task_prefix + '-metrics.csv', [RunBenchmark])
    def ExtractMetrics():
      trace_metrics_list = \
          sandwich_metrics.ExtractMetricsFromRunnerOutputDirectory(
              None, RunBenchmark.path)
      trace_metrics_list.sort(key=lambda e: e['repeat_id'])
      with open(ExtractMetrics.path, 'w') as csv_file:
        writer = csv.DictWriter(csv_file,
                                fieldnames=sandwich_metrics.CSV_FIELD_NAMES)
        writer.writeheader()
        for trace_metrics in trace_metrics_list:
          writer.writerow(trace_metrics)

    self._common_builder.default_final_tasks.append(ExtractMetrics)
