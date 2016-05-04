# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import json
import os
from urlparse import urlparse

import chrome_cache
import common_util
from loading_trace import LoadingTrace
from prefetch_view import PrefetchSimulationView
from request_dependencies_lens import RequestDependencyLens
import sandwich_runner
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


def PatchWpr(wpr_archive_path):
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
    # TODO(gabadie): may need to patch Vary.
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
  wpr_archive.Persist()


def _FilterOutDataRequests(requests):
  for request in filter(lambda r: not r.IsDataRequest(), requests):
    if request.protocol not in {'http/0.9', 'http/1.0', 'http/1.1'}:
      raise RuntimeError('Unknown request protocol {}'.format(request.protocol))
    yield request


def ExtractDiscoverableUrls(loading_trace_path, subresource_discoverer):
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
  trace = LoadingTrace.FromJsonFile(loading_trace_path)
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

  # Prune out data:// requests.
  whitelisted_urls = set()
  logging.info('white-listing %s' % first_resource_request.url)
  for request in _FilterOutDataRequests(discovered_requests):
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
  logging.error('  %s are not matching.' % url_set_name)
  logging.error('    List of missing resources:')
  for url in ref_url_set.difference(url_set):
    logging.error('-     ' + url)
  logging.error('    List of unexpected resources:')
  for url in url_set.difference(ref_url_set):
    logging.error('+     ' + url)


class RequestOutcome:
  All, ServedFromCache, NotServedFromCache = range(3)


def ListUrlRequests(trace, request_kind):
  """Lists requested URLs from a trace.

  Args:
    trace: (LoadingTrace) loading trace.
    request_kind: RequestOutcome indicating the subset of requests to output.

  Returns:
    set([str])
  """
  urls = set()
  for request_event in _FilterOutDataRequests(trace.request_track.GetEvents()):
    if (request_kind == RequestOutcome.ServedFromCache and
        request_event.from_disk_cache):
      urls.add(request_event.url)
    elif (request_kind == RequestOutcome.NotServedFromCache and
        not request_event.from_disk_cache):
      urls.add(request_event.url)
    elif request_kind == RequestOutcome.All:
      urls.add(request_event.url)
  return urls


def VerifyBenchmarkOutputDirectory(benchmark_setup_path,
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
    trace = LoadingTrace.FromJsonFile(trace_path)
    logging.info('verifying %s from %s' % (trace.url, trace_path))

    effective_requests = ListUrlRequests(trace, RequestOutcome.All)
    effective_cached_requests = \
        ListUrlRequests(trace, RequestOutcome.ServedFromCache)
    effective_uncached_requests = \
        ListUrlRequests(trace, RequestOutcome.NotServedFromCache)

    missing_requests = original_requests.difference(effective_requests)
    unexpected_requests = effective_requests.difference(original_requests)
    expected_cached_requests = \
        original_cached_requests.difference(missing_requests)
    missing_cached_requests = \
        expected_cached_requests.difference(effective_cached_requests)
    expected_uncached_requests = original_uncached_requests.union(
        unexpected_requests).union(missing_cached_requests)
    all_sent_url_requests.update(effective_uncached_requests)

    _PrintUrlSetComparison(original_requests, effective_requests,
                           'All resources')
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


def ReadSubresourceMapFromBenchmarkOutput(benchmark_output_directory_path):
  """Extracts a map URL-to-subresources for each navigation in benchmark
  directory.

  Args:
    benchmark_output_directory_path: Path of the benchmark output directory to
        verify.

  Returns:
    {url -> [URLs of sub-resources]}
  """
  url_subresources = {}
  run_id = -1
  while True:
    run_id += 1
    run_path = os.path.join(benchmark_output_directory_path, str(run_id))
    if not os.path.isdir(run_path):
      break
    trace_path = os.path.join(run_path, sandwich_runner.TRACE_FILENAME)
    if not os.path.isfile(trace_path):
      continue
    trace = LoadingTrace.FromJsonFile(trace_path)
    if trace.url in url_subresources:
      continue
    logging.info('lists resources of %s from %s' % (trace.url, trace_path))
    urls_set = set()
    for request_event in _FilterOutDataRequests(
        trace.request_track.GetEvents()):
      if request_event.url not in urls_set:
        logging.info('  %s' % request_event.url)
        urls_set.add(request_event.url)
    url_subresources[trace.url] = [url for url in urls_set]
  return url_subresources


def ValidateCacheArchiveContent(ref_urls, cache_archive_path):
  """Validates a cache archive content.

  Args:
    ref_urls: Reference list of urls.
    cache_archive_path: Cache archive's path to validate.
  """
  # TODO(gabadie): What's the best way of propagating errors happening in here?
  logging.info('lists cached urls from %s' % cache_archive_path)
  with common_util.TemporaryDirectory() as cache_directory:
    chrome_cache.UnzipDirectoryContent(cache_archive_path, cache_directory)
    cached_urls = \
        chrome_cache.CacheBackend(cache_directory, 'simple').ListKeys()
  _PrintUrlSetComparison(set(ref_urls), set(cached_urls), 'cached resources')
