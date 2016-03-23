# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import wpr_backend
import loading_trace
import request_dependencies_lens


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


def ExtractParserDiscoverableResources(loading_trace_path):
  """Extracts the parser discoverable resources from a loading trace.

  Args:
    loading_trace_path: The loading trace's path.

  Returns:
    A set of urls.
  """
  whitelisted_urls = set()
  logging.info('loading %s' % loading_trace_path)
  trace = loading_trace.LoadingTrace.FromJsonFile(loading_trace_path)
  requests_lens = request_dependencies_lens.RequestDependencyLens(trace)
  deps = requests_lens.GetRequestDependencies()

  main_resource_request = deps[0][0]
  logging.info('white-listing %s' % main_resource_request.url)
  whitelisted_urls.add(main_resource_request.url)
  for (first, second, reason) in deps:
    # Work-around where the protocol may be none for an unclear reason yet.
    # TODO(gabadie): Follow up on this with Clovis guys and possibly remove
    #   this work-around.
    if not second.protocol:
      logging.info('ignoring %s (no protocol)' % second.url)
      continue
    # Ignore data protocols.
    if not second.protocol.startswith('http'):
      logging.info('ignoring %s (`%s` is not HTTP{,S} protocol)' % (
          second.url, second.protocol))
      continue
    if (first.request_id == main_resource_request.request_id and
        reason == 'parser' and second.url not in whitelisted_urls):
      logging.info('white-listing %s' % second.url)
      whitelisted_urls.add(second.url)
  return whitelisted_urls
