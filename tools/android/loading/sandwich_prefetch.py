# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import logging
import json
import os
import re
import shutil
from urlparse import urlparse

import chrome_cache
import common_util
import loading_trace
from prefetch_view import PrefetchSimulationView
from request_dependencies_lens import RequestDependencyLens
import sandwich_metrics
import sandwich_runner
import task_manager
import wpr_backend


# Do not prefetch anything.
EMPTY_CACHE_DISCOVERER = 'empty-cache'

# Prefetches everything to load fully from cache (impossible in practice).
FULL_CACHE_DISCOVERER = 'full-cache'

# Prefetches the first resource following the redirection chain.
REDIRECTED_MAIN_DISCOVERER = 'redirected-main'

# All resources which are fetched from the main document and their redirections.
PARSER_DISCOVERER = 'parser'

# Simulation of HTMLPreloadScanner on the main document and their redirections.
HTML_PRELOAD_SCANNER_DISCOVERER = 'html-scanner'

SUBRESOURCE_DISCOVERERS = set([
  EMPTY_CACHE_DISCOVERER,
  FULL_CACHE_DISCOVERER,
  REDIRECTED_MAIN_DISCOVERER,
  PARSER_DISCOVERER,
  HTML_PRELOAD_SCANNER_DISCOVERER
])

_UPLOAD_DATA_STREAM_REQUESTS_REGEX = re.compile(r'^\d+/(?P<url>.*)$')


def _PatchWpr(wpr_archive_path):
  """Patches a WPR archive to get all resources into the HTTP cache and avoid
  invalidation and revalidations.

  Args:
    wpr_archive_path: Path of the WPR archive to patch.
  """
  # Sets the resources cache max-age to 10 years.
  MAX_AGE = 10 * 365 * 24 * 60 * 60
  CACHE_CONTROL = 'public, max-age={}'.format(MAX_AGE)

  wpr_archive = wpr_backend.WprArchiveBackend(wpr_archive_path)
  for url_entry in wpr_archive.ListUrlEntries():
    response_headers = url_entry.GetResponseHeadersDict()
    if 'cache-control' in response_headers and \
        response_headers['cache-control'] == CACHE_CONTROL:
      continue
    logging.info('patching %s' % url_entry.url)
    # TODO(gabadie): may need to patch Last-Modified and If-Modified-Since.
    # TODO(gabadie): may need to delete ETag.
    # TODO(gabadie): may need to take care of x-cache.
    #
    # Override the cache-control header to set the resources max age to MAX_AGE.
    #
    # Important note: Some resources holding sensitive information might have
    # cache-control set to no-store which allow the resource to be cached but
    # not cached in the file system. NoState-Prefetch is going to take care of
    # this case. But in here, to simulate NoState-Prefetch, we don't have other
    # choices but save absolutely all cached resources on disk so they survive
    # after killing chrome for cache save, modification and push.
    url_entry.SetResponseHeader('cache-control', CACHE_CONTROL)

    # TODO(gabadie): May need to extend Vary blacklist (referer?)
    #
    # All of these Vary and Pragma possibilities need to be removed from
    # response headers in order for Chrome to store a resource in HTTP cache and
    # not to invalidate it.
    #
    # Note: HttpVaryData::Init() in Chrome adds an implicit 'Vary: cookie'
    # header to any redirect.
    # TODO(gabadie): Find a way to work around this issue.
    url_entry.RemoveResponseHeaderDirectives('vary', {'*', 'cookie'})
    url_entry.RemoveResponseHeaderDirectives('pragma', {'no-cache'})

  wpr_archive.Persist()


def _FilterOutDataAndIncompleteRequests(requests):
  for request in filter(lambda r: not r.IsDataRequest(), requests):
    # The protocol is only known once the response has been received. But the
    # trace recording might have been stopped with still some JavaScript
    # originated requests that have not received any responses yet.
    if request.protocol is None:
      assert not request.HasReceivedResponse()
      assert request.initiator['type'] == 'script'
      continue
    if request.protocol not in {'http/0.9', 'http/1.0', 'http/1.1'}:
      raise RuntimeError('Unknown request protocol {}'.format(request.protocol))
    yield request


def _PatchCacheArchive(cache_archive_path, loading_trace_path,
                       cache_archive_dest_path):
  """Patch the cache archive.

  Note: This method update the raw response headers of cache entries' to store
    the ones such as Set-Cookie that were pruned by the
    net::HttpCacheTransaction, and remove the stream index 2 holding resource's
    compile meta data.

  Args:
    cache_archive_path: Input archive's path to patch.
    loading_trace_path: Path of the loading trace that have recorded the cache
        archive <cache_archive_path>.
    cache_archive_dest_path: Archive destination's path.
  """
  trace = loading_trace.LoadingTrace.FromJsonFile(loading_trace_path)
  with common_util.TemporaryDirectory(prefix='sandwich_tmp') as tmp_path:
    cache_path = os.path.join(tmp_path, 'cache')
    chrome_cache.UnzipDirectoryContent(cache_archive_path, cache_path)
    cache_backend = chrome_cache.CacheBackend(cache_path, 'simple')
    cache_entries = set(cache_backend.ListKeys())
    logging.info('Original cache size: %d bytes' % cache_backend.GetSize())
    for request in _FilterOutDataAndIncompleteRequests(
        trace.request_track.GetEvents()):
      # On requests having an upload data stream such as POST requests,
      # net::HttpCache::GenerateCacheKey() prefixes the cache entry's key with
      # the upload data stream's session unique identifier.
      #
      # It is fine to not patch these requests since when reopening Chrome,
      # there is no way the entry can be reused since the upload data stream's
      # identifier will be different.
      #
      # The fact that these entries are kept in the cache after closing Chrome
      # properly by closing the Chrome tab as the ChromeControler.SetSlowDeath()
      # do is known chrome bug (crbug.com/610725).
      if request.url not in cache_entries:
        continue
      # Chrome prunes Set-Cookie from response headers before storing them in
      # disk cache. Also, it adds implicit "Vary: cookie" header to all redirect
      # response headers. Sandwich manages the cache, but between recording the
      # cache and benchmarking the cookie jar is invalidated. This leads to
      # invalidation of all cacheable redirects.
      raw_headers = request.GetRawResponseHeaders()
      cache_backend.UpdateRawResponseHeaders(request.url, raw_headers)
      # NoState-Prefetch would only fetch the resources, but not parse them.
      cache_backend.DeleteStreamForKey(request.url, 2)
    chrome_cache.ZipDirectoryContent(cache_path, cache_archive_dest_path)
    logging.info('Patched cache size: %d bytes' % cache_backend.GetSize())


def _ExtractDiscoverableUrls(loading_trace_path, subresource_discoverer):
  """Extracts discoverable resource urls from a loading trace according to a
  sub-resource discoverer.

  Args:
    loading_trace_path: The loading trace's path.
    subresource_discoverer: The sub-resources discoverer that should white-list
      the resources to keep in cache for the NoState-Prefetch benchmarks.

  Returns:
    A set of urls.
  """
  assert subresource_discoverer in SUBRESOURCE_DISCOVERERS, \
      'unknown prefetch simulation {}'.format(subresource_discoverer)

  # Load trace and related infos.
  logging.info('loading %s' % loading_trace_path)
  trace = loading_trace.LoadingTrace.FromJsonFile(loading_trace_path)
  dependencies_lens = RequestDependencyLens(trace)
  first_resource_request = trace.request_track.GetFirstResourceRequest()

  # Build the list of discovered requests according to the desired simulation.
  discovered_requests = []
  if subresource_discoverer == EMPTY_CACHE_DISCOVERER:
    pass
  elif subresource_discoverer == FULL_CACHE_DISCOVERER:
    discovered_requests = trace.request_track.GetEvents()
  elif subresource_discoverer == REDIRECTED_MAIN_DISCOVERER:
    discovered_requests = \
        [dependencies_lens.GetRedirectChain(first_resource_request)[-1]]
  elif subresource_discoverer == PARSER_DISCOVERER:
    discovered_requests = PrefetchSimulationView.ParserDiscoverableRequests(
        first_resource_request, dependencies_lens)
  elif subresource_discoverer == HTML_PRELOAD_SCANNER_DISCOVERER:
    discovered_requests = PrefetchSimulationView.PreloadedRequests(
        first_resource_request, dependencies_lens, trace)
  else:
    assert False

  whitelisted_urls = set()
  logging.info('white-listing %s' % first_resource_request.url)
  for request in _FilterOutDataAndIncompleteRequests(discovered_requests):
    logging.info('white-listing %s' % request.url)
    whitelisted_urls.add(request.url)
  return whitelisted_urls


def _PrintUrlSetComparison(ref_url_set, url_set, url_set_name):
  """Compare URL sets and log the diffs.

  Args:
    ref_url_set: Set of reference urls.
    url_set: Set of urls to compare to the reference.
    url_set_name: The set name for logging purposes.
  """
  assert type(ref_url_set) == set
  assert type(url_set) == set
  if ref_url_set == url_set:
    logging.info('  %d %s are matching.' % (len(ref_url_set), url_set_name))
    return
  missing_urls = ref_url_set.difference(url_set)
  unexpected_urls = url_set.difference(ref_url_set)
  logging.error('  %s are not matching (expected %d, had %d)' % \
      (url_set_name, len(ref_url_set), len(url_set)))
  logging.error('    List of %d missing resources:' % len(missing_urls))
  for url in sorted(missing_urls):
    logging.error('-     ' + url)
  logging.error('    List of %d unexpected resources:' % len(unexpected_urls))
  for url in sorted(unexpected_urls):
    logging.error('+     ' + url)


class _RequestOutcome:
  All, ServedFromCache, NotServedFromCache, Post = range(4)


def _ListUrlRequests(trace, request_kind):
  """Lists requested URLs from a trace.

  Args:
    trace: (loading_trace.LoadingTrace) loading trace.
    request_kind: _RequestOutcome indicating the subset of requests to output.

  Returns:
    set([str])
  """
  urls = set()
  for request_event in _FilterOutDataAndIncompleteRequests(
      trace.request_track.GetEvents()):
    if (request_kind == _RequestOutcome.ServedFromCache and
        request_event.from_disk_cache):
      urls.add(request_event.url)
    elif (request_kind == _RequestOutcome.Post and
        request_event.method.upper().strip() == 'POST'):
      urls.add(request_event.url)
    elif (request_kind == _RequestOutcome.NotServedFromCache and
        not request_event.from_disk_cache):
      urls.add(request_event.url)
    elif request_kind == _RequestOutcome.All:
      urls.add(request_event.url)
  return urls


def _VerifyBenchmarkOutputDirectory(benchmark_setup_path,
                                    benchmark_output_directory_path):
  """Verifies that all run inside the run_output_directory worked as expected.

  Args:
    benchmark_setup_path: Path of the JSON of the benchmark setup.
    benchmark_output_directory_path: Path of the benchmark output directory to
        verify.
  """
  # TODO(gabadie): What's the best way of propagating errors happening in here?
  benchmark_setup = json.load(open(benchmark_setup_path))
  cache_whitelist = set(benchmark_setup['cache_whitelist'])
  original_requests = set(benchmark_setup['url_resources'])
  original_cached_requests = original_requests.intersection(cache_whitelist)
  original_uncached_requests = original_requests.difference(cache_whitelist)
  all_sent_url_requests = set()

  # Verify requests from traces.
  run_id = -1
  while True:
    run_id += 1
    run_path = os.path.join(benchmark_output_directory_path, str(run_id))
    if not os.path.isdir(run_path):
      break
    trace_path = os.path.join(run_path, sandwich_runner.TRACE_FILENAME)
    if not os.path.isfile(trace_path):
      logging.error('missing trace %s' % trace_path)
      continue
    trace = loading_trace.LoadingTrace.FromJsonFile(trace_path)
    logging.info('verifying %s from %s' % (trace.url, trace_path))

    effective_requests = _ListUrlRequests(trace, _RequestOutcome.All)
    effective_post_requests = _ListUrlRequests(trace, _RequestOutcome.Post)
    effective_cached_requests = \
        _ListUrlRequests(trace, _RequestOutcome.ServedFromCache)
    effective_uncached_requests = \
        _ListUrlRequests(trace, _RequestOutcome.NotServedFromCache)

    missing_requests = original_requests.difference(effective_requests)
    unexpected_requests = effective_requests.difference(original_requests)
    expected_cached_requests = \
        original_cached_requests.difference(missing_requests)
    missing_cached_requests = \
        expected_cached_requests.difference(effective_cached_requests)
    expected_uncached_requests = original_uncached_requests.union(
        unexpected_requests).union(missing_cached_requests)
    all_sent_url_requests.update(effective_uncached_requests)

    # POST requests are known to be unable to use the cache.
    expected_cached_requests.difference_update(effective_post_requests)
    expected_uncached_requests.update(effective_post_requests)

    _PrintUrlSetComparison(original_requests, effective_requests,
                           'All resources')
    _PrintUrlSetComparison(set(), effective_post_requests,
                           'POST resources')
    _PrintUrlSetComparison(expected_cached_requests, effective_cached_requests,
                           'Cached resources')
    _PrintUrlSetComparison(expected_uncached_requests,
                           effective_uncached_requests, 'Non cached resources')

  # Verify requests from WPR.
  wpr_log_path = os.path.join(
      benchmark_output_directory_path, sandwich_runner.WPR_LOG_FILENAME)
  logging.info('verifying requests from %s' % wpr_log_path)
  all_wpr_requests = wpr_backend.ExtractRequestsFromLog(wpr_log_path)
  all_wpr_urls = set()
  unserved_wpr_urls = set()
  wpr_command_colliding_urls = set()

  for request in all_wpr_requests:
    if request.is_wpr_host:
      continue
    if urlparse(request.url).path.startswith('/web-page-replay'):
      wpr_command_colliding_urls.add(request.url)
    elif request.is_served is False:
      unserved_wpr_urls.add(request.url)
    all_wpr_urls.add(request.url)

  _PrintUrlSetComparison(set(), unserved_wpr_urls,
                         'Distinct unserved resources from WPR')
  _PrintUrlSetComparison(set(), wpr_command_colliding_urls,
                         'Distinct resources colliding to WPR commands')
  _PrintUrlSetComparison(all_wpr_urls, all_sent_url_requests,
                         'Distinct resource requests to WPR')


def _ReadSubresourceFromRunnerOutputDir(runner_output_dir):
  """Extracts a list of subresources in runner output directory.

  Args:
    runner_output_dir: Path of the runner's output directory.

  Returns:
    [URLs of sub-resources]
  """
  trace_path = os.path.join(
      runner_output_dir, '0', sandwich_runner.TRACE_FILENAME)
  trace = loading_trace.LoadingTrace.FromJsonFile(trace_path)
  url_set = set()
  for request_event in _FilterOutDataAndIncompleteRequests(
      trace.request_track.GetEvents()):
    url_set.add(request_event.url)
  logging.info('lists %s resources of %s from %s' % \
               (len(url_set), trace.url, trace_path))
  return [url for url in url_set]


def _ValidateCacheArchiveContent(cache_build_trace_path, cache_archive_path):
  """Validates a cache archive content.

  Args:
    cache_build_trace_path: Path of the generated trace at the cache build time.
    cache_archive_path: Cache archive's path to validate.
  """
  # TODO(gabadie): What's the best way of propagating errors happening in here?
  logging.info('lists cached urls from %s' % cache_archive_path)
  with common_util.TemporaryDirectory() as cache_directory:
    chrome_cache.UnzipDirectoryContent(cache_archive_path, cache_directory)
    cache_keys = set(
        chrome_cache.CacheBackend(cache_directory, 'simple').ListKeys())
  trace = loading_trace.LoadingTrace.FromJsonFile(cache_build_trace_path)
  effective_requests = _ListUrlRequests(trace, _RequestOutcome.All)
  effective_post_requests = _ListUrlRequests(trace, _RequestOutcome.Post)

  upload_data_stream_cache_entry_keys = set()
  upload_data_stream_requests = set()
  for cache_entry_key in cache_keys:
    match = _UPLOAD_DATA_STREAM_REQUESTS_REGEX.match(cache_entry_key)
    if not match:
      continue
    upload_data_stream_cache_entry_keys.add(cache_entry_key)
    upload_data_stream_requests.add(match.group('url'))

  expected_cached_requests = effective_requests.difference(
      effective_post_requests)
  effective_cache_keys = cache_keys.difference(
      upload_data_stream_cache_entry_keys)

  _PrintUrlSetComparison(effective_post_requests, upload_data_stream_requests,
                         'POST resources')
  _PrintUrlSetComparison(expected_cached_requests, effective_cache_keys,
                         'Cached resources')


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
      _PatchWpr(BuildPatchedWpr.path)

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
      _PatchCacheArchive(BuildOriginalCache.path,
          original_cache_trace_path, BuildPatchedCache.path)

    @self.RegisterTask('common/subresources-for-urls.json',
                       [BuildOriginalCache])
    def ListUrlsResources():
      url_resources = _ReadSubresourceFromRunnerOutputDir(
          BuildOriginalCache.run_path)
      with open(ListUrlsResources.path, 'w') as output:
        json.dump(url_resources, output)

    @self.RegisterTask('common/patched-cache-validation.log',
                       [BuildPatchedCache])
    def ValidatePatchedCache():
      handler = logging.FileHandler(ValidatePatchedCache.path)
      logging.getLogger().addHandler(handler)
      try:
        _ValidateCacheArchiveContent(
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

    assert subresource_discoverer in SUBRESOURCE_DISCOVERERS
    assert 'common' not in SUBRESOURCE_DISCOVERERS
    shared_task_prefix = os.path.join('common', subresource_discoverer)
    task_prefix = os.path.join(transformer_list_name, subresource_discoverer)

    @self.RegisterTask(shared_task_prefix + '-setup.json', merge=True,
                       dependencies=[self._subresources_for_urls_task])
    def SetupBenchmark():
      whitelisted_urls = _ExtractDiscoverableUrls(
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
      _VerifyBenchmarkOutputDirectory(SetupBenchmark.path, RunBenchmark.path)

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
            'subresource_count': len(_ListUrlRequests(
                trace, _RequestOutcome.All)),
            'subresource_count_theoretic':
                len(benchmark_setup['url_resources']),
            'cached_subresource_count': len(_ListUrlRequests(
                trace, _RequestOutcome.ServedFromCache)),
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
