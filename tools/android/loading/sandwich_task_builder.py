# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import json
import os
import shutil

import chrome_cache
import common_util
import emulation
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


class SandwichTaskBuilder(task_manager.Builder):
  """A builder for a graph of tasks, each prepares or invokes a SandwichRunner.
  """

  def __init__(self, output_directory, android_device, job_path, url_repeat):
    """Constructor.

    Args:
      output_directory: As in task_manager.Builder.__init__
      android_device: The android DeviceUtils to run sandwich on or None to run
        it locally.
      job_path: Path of the sandwich's job.
      url_repeat: Non null integer controlling how many times the URLs should be
        repeated in the benchmarks.
    """
    task_manager.Builder.__init__(self, output_directory)
    self._android_device = android_device
    self._job_path = job_path
    self._url_repeat = url_repeat
    self._default_final_tasks = []

    self._original_wpr_task = None
    self._patched_wpr_task = None
    self._reference_cache_task = None
    self._subresources_for_urls_run_task = None
    self._subresources_for_urls_task = None

  @property
  def default_final_tasks(self):
      return self._default_final_tasks

  def _CreateSandwichRunner(self):
    """Create a runner for non benchmark purposes."""
    runner = sandwich_runner.SandwichRunner()
    runner.LoadJob(self._job_path)
    runner.android_device = self._android_device
    return runner

  def OverridePathToWprArchive(self, original_wpr_path):
    """Sets the original WPR archive path's to be used.

    Args:
      original_wpr_path: Path of the original WPR archive to be used.
    """
    self._original_wpr_task = \
        self.CreateStaticTask('common/webpages.wpr', original_wpr_path)

  def PopulateWprRecordingTask(self):
    """Records the original WPR archive."""
    @self.RegisterTask('common/webpages.wpr')
    def BuildOriginalWpr():
      common_util.EnsureParentDirectoryExists(BuildOriginalWpr.path)
      runner = self._CreateSandwichRunner()
      runner.wpr_archive_path = BuildOriginalWpr.path
      runner.wpr_record = True
      runner.Run()

    self._original_wpr_task = BuildOriginalWpr

  def PopulateCommonPipelines(self):
    """Creates necessary tasks to produce initial cache archive.

    Also creates a task for producing a json file with a mapping of URLs to
    subresources (urls-resources.json).

    Here is the full dependency tree for the returned task:
    common/cache-ref-validation.log
      depends on: common/cache-ref.zip
        depends on: common/webpages-patched.wpr
          depends on: common/webpages.wpr
      depends on: common/urls-resources.json
        depends on: common/urls-resources-run/
          depends on: common/webpages.wpr

    Returns:
      The last task of the pipeline.
    """
    @self.RegisterTask('common/webpages-patched.wpr', [self._original_wpr_task])
    def BuildPatchedWpr():
      common_util.EnsureParentDirectoryExists(BuildPatchedWpr.path)
      shutil.copyfile(self._original_wpr_task.path, BuildPatchedWpr.path)
      sandwich_misc.PatchWpr(BuildPatchedWpr.path)

    @self.RegisterTask('common/cache-ref.zip', [BuildPatchedWpr])
    def BuildReferenceCache():
      runner = self._CreateSandwichRunner()
      runner.wpr_archive_path = BuildPatchedWpr.path
      runner.cache_archive_path = BuildReferenceCache.path
      runner.cache_operation = 'save'
      runner.Run()

    @self.RegisterTask('common/subresources-for-urls-run/',
                       dependencies=[self._original_wpr_task])
    def UrlsResourcesRun():
      runner = self._CreateSandwichRunner()
      runner.wpr_archive_path = self._original_wpr_task.path
      runner.cache_operation = 'clear'
      runner.trace_output_directory = UrlsResourcesRun.path
      runner.Run()

    @self.RegisterTask('common/subresources-for-urls.json', [UrlsResourcesRun])
    def ListUrlsResources():
      json_content = sandwich_misc.ReadSubresourceMapFromBenchmarkOutput(
          UrlsResourcesRun.path)
      with open(ListUrlsResources.path, 'w') as output:
        json.dump(json_content, output)

    @self.RegisterTask('common/cache-ref-validation.log',
                       [BuildReferenceCache, ListUrlsResources])
    def ValidateReferenceCache():
      json_content = json.load(open(ListUrlsResources.path))
      ref_urls = set()
      for urls in json_content.values():
        ref_urls.update(set(urls))
      sandwich_misc.ValidateCacheArchiveContent(
          ref_urls, BuildReferenceCache.path)

    self._patched_wpr_task = BuildPatchedWpr
    self._reference_cache_task = BuildReferenceCache
    self._subresources_for_urls_run_task = UrlsResourcesRun
    self._subresources_for_urls_task = ListUrlsResources

    self._default_final_tasks.append(ValidateReferenceCache)
    return ValidateReferenceCache

  def PopulateLoadBenchmark(self, subresource_discoverer,
                            runner_transformer_name, runner_transformer):
    """Populate benchmarking tasks from its setup tasks.

    Args:
      subresource_discoverer: Name of a subresources discoverer.
      runner_transformer: A function that takes an instance of SandwichRunner as
          parameter, would be applied immediately before SandwichRunner.Run().
      runner_transformer_name: Name of the runner transformer used to generate
          task names.
      benchmark_name: The benchmark's name for that runner modifier.

    Here is the full dependency of the added tree for the returned task:
    <runner_transformer_name>/<subresource_discoverer>-metrics.csv
      depends on: <runner_transformer_name>/<subresource_discoverer>-run/
        depends on: common/<subresource_discoverer>-cache.zip
          depends on: some tasks saved by PopulateCommonPipelines()
          depends on: common/<subresource_discoverer>-setup.json
            depends on: some tasks saved by PopulateCommonPipelines()

    Returns:
      task_manager.Task for
          <runner_transformer_name>/<subresource_discoverer>-metrics.csv
    """
    assert subresource_discoverer in sandwich_misc.SUBRESOURCE_DISCOVERERS
    assert 'common' not in sandwich_misc.SUBRESOURCE_DISCOVERERS
    shared_task_prefix = os.path.join('common', subresource_discoverer)
    task_prefix = os.path.join(runner_transformer_name, subresource_discoverer)

    @self.RegisterTask(shared_task_prefix + '-setup.json', merge=True,
                       dependencies=[self._subresources_for_urls_task])
    def SetupBenchmark():
      trace_path = os.path.join(self._subresources_for_urls_run_task.path, '0',
                                sandwich_runner.TRACE_FILENAME)
      whitelisted_urls = sandwich_misc.ExtractDiscoverableUrls(
          trace_path, subresource_discoverer)

      urls_resources = json.load(open(self._subresources_for_urls_task.path))
      # TODO(gabadie): Implement support for multiple URLs in this Task.
      assert len(urls_resources) == 1
      url = urls_resources.keys()[0]
      url_resources = urls_resources[url]
      common_util.EnsureParentDirectoryExists(SetupBenchmark.path)
      with open(SetupBenchmark.path, 'w') as output:
        json.dump({
            'cache_whitelist': [url for url in whitelisted_urls],
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
      runner = self._CreateSandwichRunner()
      # runner.record_video = True
      runner.job_repeat = self._url_repeat
      runner_transformer(runner)
      runner.wpr_archive_path = self._patched_wpr_task.path
      runner.wpr_out_log_path = os.path.join(RunBenchmark.path, 'wpr.log')
      runner.cache_archive_path = BuildBenchmarkCacheArchive.path
      runner.cache_operation = 'push'
      runner.trace_output_directory = RunBenchmark.path
      runner.Run()

    @self.RegisterTask(task_prefix + '-metrics.csv',
                       dependencies=[RunBenchmark])
    def ExtractMetrics():
      sandwich_misc.VerifyBenchmarkOutputDirectory(
          SetupBenchmark.path, RunBenchmark.path)
      trace_metrics_list = sandwich_metrics.PullMetricsFromOutputDirectory(
          RunBenchmark.path)
      trace_metrics_list.sort(key=lambda e: e['id'])
      with open(ExtractMetrics.path, 'w') as csv_file:
        writer = csv.DictWriter(csv_file,
                                fieldnames=sandwich_metrics.CSV_FIELD_NAMES)
        writer.writeheader()
        for trace_metrics in trace_metrics_list:
          writer.writerow(trace_metrics)

    self._default_final_tasks.append(ExtractMetrics)
    return ExtractMetrics
