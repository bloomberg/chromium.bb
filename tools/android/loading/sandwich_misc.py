# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from loading_trace import LoadingTrace
from prefetch_view import PrefetchSimulationView
from request_dependencies_lens import RequestDependencyLens
from user_satisfied_lens import FirstContentfulPaintLens
import wpr_backend


# Prefetches the first resource following the redirection chain.
REDIRECTED_MAIN_DISCOVERER = 'redirected-main'

# All resources which are fetched from the main document and their redirections.
PARSER_DISCOVERER = 'parser'

# Simulation of HTMLPreloadScanner on the main document and their redirections.
HTML_PRELOAD_SCANNER_DISCOVERER = 'html-scanner'

SUBRESOURCE_DISCOVERERS = set([
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
  if subresource_discoverer == REDIRECTED_MAIN_DISCOVERER:
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
  whitelisted_urls.add(first_resource_request.url)
  for request in discovered_requests:
    # Work-around where the protocol may be none for an unclear reason yet.
    # TODO(gabadie): Follow up on this with Clovis guys and possibly remove
    #   this work-around.
    if not request.protocol:
      logging.warning('ignoring %s (no protocol)' % request.url)
      continue
    # Ignore data protocols.
    if not request.protocol.startswith('http'):
      continue
    logging.info('white-listing %s' % request.url)
    whitelisted_urls.add(request.url)
  return whitelisted_urls
