# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import logging
import json
import logging
import os
import shutil

import chrome_cache
import common_util
import emulation
import loading_trace
import sandwich_metrics
import sandwich_misc
import sandwich_runner
import task_manager


def NetworkSimulationTransformer(network_condition):
  """Creates a function that accepts a SandwichRunner as a parameter and sets
  network emulation options on it.

  Args:
    network_condition: The network condition to apply to the sandwich runner.

  Returns:
    A callback transforming the SandwichRunner given in argument accordingly
  """
  assert network_condition in emulation.NETWORK_CONDITIONS
  def Transformer(runner):
    assert isinstance(runner, sandwich_runner.SandwichRunner)
    runner.network_condition = network_condition
  return Transformer


class SandwichCommonBuilder(task_manager.Builder):
  """A builder for a graph of tasks, each prepares or invokes a SandwichRunner.
  """

  def __init__(self, android_device, url, output_directory,
               output_subdirectory):
    """Constructor.

    Args:
      android_device: The android DeviceUtils to run sandwich on or None to run
        it locally.
      url: URL to benchmark.
      output_directory: As in task_manager.Builder.__init__
      output_subdirectory: As in task_manager.Builder.__init__
    """
    task_manager.Builder.__init__(self, output_directory, output_subdirectory)
    self._android_device = android_device
    self._url = url
    self.default_final_tasks = []

    self.original_wpr_task = None

  def CreateSandwichRunner(self):
    """Create a runner for non benchmark purposes."""
    runner = sandwich_runner.SandwichRunner()
    runner.url = self._url
    runner.android_device = self._android_device
    return runner

  def OverridePathToWprArchive(self, original_wpr_path):
    """Sets the original WPR archive path's to be used.

    Args:
      original_wpr_path: Path of the original WPR archive to be used.
    """
    self.original_wpr_task = \
        self.CreateStaticTask('common/webpages.wpr', original_wpr_path)

  def PopulateWprRecordingTask(self):
    """Records the original WPR archive."""
    @self.RegisterTask('common/webpages.wpr')
    def BuildOriginalWpr():
      common_util.EnsureParentDirectoryExists(BuildOriginalWpr.path)
      runner = self.CreateSandwichRunner()
      runner.wpr_archive_path = BuildOriginalWpr.path
      runner.wpr_record = True
      runner.Run()

    self.original_wpr_task = BuildOriginalWpr


class PrefetchBenchmarkBuilder(task_manager.Builder):
  """A builder for a graph of tasks for NoState-Prefetch emulated benchmarks."""

  def __init__(self, common_builder):
    task_manager.Builder.__init__(self,
                                  common_builder.output_directory,
                                  common_builder.output_subdirectory)
    self._common_builder = common_builder

    self._patched_wpr_task = None
    self._reference_cache_task = None
    self._trace_from_grabbing_reference_cache = None
    self._subresources_for_urls_task = None
    self._PopulateCommonPipelines()

  def _PopulateCommonPipelines(self):
    """Creates necessary tasks to produce initial cache archive.

    Also creates a task for producing a json file with a mapping of URLs to
    subresources (urls-resources.json).

    Here is the full dependency tree for the returned task:
    common/patched-cache-validation.log
      depends on: common/patched-cache.zip
        depends on: common/original-cache.zip
          depends on: common/webpages-patched.wpr
            depends on: common/webpages.wpr
      depends on: common/urls-resources.json
        depends on: common/original-cache.zip
    """
    @self.RegisterTask('common/webpages-patched.wpr',
                       dependencies=[self._common_builder.original_wpr_task])
    def BuildPatchedWpr():
      common_util.EnsureParentDirectoryExists(BuildPatchedWpr.path)
      shutil.copyfile(
          self._common_builder.original_wpr_task.path, BuildPatchedWpr.path)
      sandwich_misc.PatchWpr(BuildPatchedWpr.path)

    @self.RegisterTask('common/original-cache.zip', [BuildPatchedWpr])
    def BuildOriginalCache():
      runner = self._common_builder.CreateSandwichRunner()
      runner.wpr_archive_path = BuildPatchedWpr.path
      runner.cache_archive_path = BuildOriginalCache.path
      runner.cache_operation = sandwich_runner.CacheOperation.SAVE
      runner.output_dir = BuildOriginalCache.run_path
      runner.Run()
    BuildOriginalCache.run_path = BuildOriginalCache.path[:-4] + '-run'
    original_cache_trace_path = os.path.join(
        BuildOriginalCache.run_path, '0', sandwich_runner.TRACE_FILENAME)

    @self.RegisterTask('common/patched-cache.zip', [BuildOriginalCache])
    def BuildPatchedCache():
      sandwich_misc.PatchCacheArchive(BuildOriginalCache.path,
          original_cache_trace_path, BuildPatchedCache.path)

    @self.RegisterTask('common/subresources-for-urls.json',
                       [BuildOriginalCache])
    def ListUrlsResources():
      url_resources = sandwich_misc.ReadSubresourceFromRunnerOutputDir(
          BuildOriginalCache.run_path)
      with open(ListUrlsResources.path, 'w') as output:
        json.dump(url_resources, output)

    @self.RegisterTask('common/patched-cache-validation.log',
                       [BuildPatchedCache])
    def ValidatePatchedCache():
      handler = logging.FileHandler(ValidatePatchedCache.path)
      logging.getLogger().addHandler(handler)
      try:
        sandwich_misc.ValidateCacheArchiveContent(
            original_cache_trace_path, BuildPatchedCache.path)
      finally:
        logging.getLogger().removeHandler(handler)

    self._patched_wpr_task = BuildPatchedWpr
    self._trace_from_grabbing_reference_cache = original_cache_trace_path
    self._reference_cache_task = BuildPatchedCache
    self._subresources_for_urls_task = ListUrlsResources

    self._common_builder.default_final_tasks.append(ValidatePatchedCache)

  def PopulateLoadBenchmark(self, subresource_discoverer,
                            transformer_list_name, transformer_list):
    """Populate benchmarking tasks from its setup tasks.

    Args:
      subresource_discoverer: Name of a subresources discoverer.
      transformer_list_name: A string describing the transformers, will be used
          in Task names (prefer names without spaces and special characters).
      transformer_list: An ordered list of function that takes an instance of
          SandwichRunner as parameter, would be applied immediately before
          SandwichRunner.Run() in the given order.

    Here is the full dependency of the added tree for the returned task:
    <transformer_list_name>/<subresource_discoverer>-metrics.csv
      depends on: <transformer_list_name>/<subresource_discoverer>-run/
        depends on: common/<subresource_discoverer>-cache.zip
          depends on: some tasks saved by PopulateCommonPipelines()
          depends on: common/<subresource_discoverer>-setup.json
            depends on: some tasks saved by PopulateCommonPipelines()
    """
    additional_column_names = [
        'url',
        'repeat_id',
        'subresource_discoverer',
        'subresource_count',
        # The amount of subresources detected at SetupBenchmark step.
        'subresource_count_theoretic',
        # Amount of subresources for caching as suggested by the subresource
        # discoverer.
        'cached_subresource_count_theoretic',
        'cached_subresource_count']

    assert subresource_discoverer in sandwich_misc.SUBRESOURCE_DISCOVERERS
    assert 'common' not in sandwich_misc.SUBRESOURCE_DISCOVERERS
    shared_task_prefix = os.path.join('common', subresource_discoverer)
    task_prefix = os.path.join(transformer_list_name, subresource_discoverer)

    @self.RegisterTask(shared_task_prefix + '-setup.json', merge=True,
                       dependencies=[self._subresources_for_urls_task])
    def SetupBenchmark():
      whitelisted_urls = sandwich_misc.ExtractDiscoverableUrls(
          self._trace_from_grabbing_reference_cache, subresource_discoverer)

      url_resources = json.load(open(self._subresources_for_urls_task.path))
      common_util.EnsureParentDirectoryExists(SetupBenchmark.path)
      with open(SetupBenchmark.path, 'w') as output:
        json.dump({
            'cache_whitelist': [url for url in whitelisted_urls],
            'subresource_discoverer': subresource_discoverer,
            'url_resources': url_resources,
          }, output)

    @self.RegisterTask(shared_task_prefix + '-cache.zip', merge=True,
                       dependencies=[
                           SetupBenchmark, self._reference_cache_task])
    def BuildBenchmarkCacheArchive():
      setup = json.load(open(SetupBenchmark.path))
      chrome_cache.ApplyUrlWhitelistToCacheArchive(
          cache_archive_path=self._reference_cache_task.path,
          whitelisted_urls=setup['cache_whitelist'],
          output_cache_archive_path=BuildBenchmarkCacheArchive.path)

    @self.RegisterTask(task_prefix + '-run/',
                       dependencies=[BuildBenchmarkCacheArchive])
    def RunBenchmark():
      runner = self._common_builder.CreateSandwichRunner()
      for transformer in transformer_list:
        transformer(runner)
      runner.wpr_archive_path = self._patched_wpr_task.path
      runner.wpr_out_log_path = os.path.join(
          RunBenchmark.path, sandwich_runner.WPR_LOG_FILENAME)
      runner.cache_archive_path = BuildBenchmarkCacheArchive.path
      runner.cache_operation = sandwich_runner.CacheOperation.PUSH
      runner.output_dir = RunBenchmark.path
      runner.Run()

    @self.RegisterTask(task_prefix + '-metrics.csv',
                       dependencies=[RunBenchmark])
    def ExtractMetrics():
      # TODO(gabadie): Performance improvement: load each trace only once and
      # use it for validation and extraction of metrics later.
      sandwich_misc.VerifyBenchmarkOutputDirectory(
          SetupBenchmark.path, RunBenchmark.path)

      benchmark_setup = json.load(open(SetupBenchmark.path))
      run_metrics_list = []
      for repeat_id, repeat_dir in sandwich_runner.WalkRepeatedRuns(
          RunBenchmark.path):
        trace_path = os.path.join(repeat_dir, sandwich_runner.TRACE_FILENAME)
        logging.info('processing trace: %s', trace_path)
        trace = loading_trace.LoadingTrace.FromJsonFile(trace_path)
        run_metrics = {
            'url': trace.url,
            'repeat_id': repeat_id,
            'subresource_discoverer': benchmark_setup['subresource_discoverer'],
            'subresource_count': len(sandwich_misc.ListUrlRequests(
                trace, sandwich_misc.RequestOutcome.All)),
            'subresource_count_theoretic':
                len(benchmark_setup['url_resources']),
            'cached_subresource_count': len(sandwich_misc.ListUrlRequests(
                trace, sandwich_misc.RequestOutcome.ServedFromCache)),
            'cached_subresource_count_theoretic':
                len(benchmark_setup['cache_whitelist']),
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
