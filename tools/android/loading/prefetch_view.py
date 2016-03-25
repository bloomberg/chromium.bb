# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Models the effect of prefetching resources from a loading trace.

For example, this can be used to evaluate NoState Prefetch
(https://goo.gl/B3nRUR).

When executed as a script, takes a trace as a command-line arguments and shows
how many requests were prefetched.
"""

import itertools
import operator

import dependency_graph
import loading_trace
import user_satisfied_lens
import request_dependencies_lens
import request_track


class PrefetchSimulationView(object):
  """Simulates the effect of prefetching resources discoverable by the preload
  scanner.
  """
  def __init__(self, trace, dependencies_lens, user_lens):
    """Initializes an instance of PrefetchSimulationView.

    Args:
      trace: (LoadingTrace) a loading trace.
      dependencies_lens: (RequestDependencyLens) request dependencies.
      user_lens: (UserSatisfiedLens) Lens used to compute costs.
    """
    self.trace = trace
    self.dependencies_lens = dependencies_lens
    self._resource_events = self.trace.tracing_track.Filter(
        categories=set([u'blink.net']))
    assert len(self._resource_events.GetEvents()) > 0,\
            'Was the "blink.net" category enabled at trace collection time?"'
    self._user_lens = user_lens
    request_ids = self._user_lens.CriticalRequests()
    all_requests = self.trace.request_track.GetEvents()
    self._first_request_node = all_requests[0].request_id
    requests = [r for r in all_requests if r.request_id in request_ids]
    self.graph = dependency_graph.RequestDependencyGraph(
        requests, self.dependencies_lens)

  def ParserDiscoverableRequests(self, request, recurse=False):
    """Returns a list of requests discovered by the parser from a given request.

    Args:
      request: (Request) Root request.

    Returns:
      [Request]
    """
    # TODO(lizeb): handle the recursive case.
    assert not recurse
    discoverable_requests = [request]
    first_request = self.dependencies_lens.GetRedirectChain(request)[-1]
    deps = self.dependencies_lens.GetRequestDependencies()
    for (first, second, reason) in deps:
      if first.request_id == first_request.request_id and reason == 'parser':
        discoverable_requests.append(second)
    return discoverable_requests

  def ExpandRedirectChains(self, requests):
    return list(itertools.chain.from_iterable(
        [self.dependencies_lens.GetRedirectChain(r) for r in requests]))

  def PreloadedRequests(self, request):
    """Returns the requests that have been preloaded from a given request.

    This list is the set of request that are:
    - Discoverable by the parser
    - Found in the trace log.

    Before looking for dependencies, this follows the redirect chain.

    Args:
      request: (Request) Root request.

    Returns:
      A list of Request. Does not include the root request. This list is a
      subset of the one returned by ParserDiscoverableRequests().
    """
    # Preload step events are emitted in ResourceFetcher::preloadStarted().
    preload_step_events = filter(
        lambda e:  e.args.get('step') == 'Preload',
        self._resource_events.GetEvents())
    preloaded_urls = set()
    for preload_step_event in preload_step_events:
      preload_event = self._resource_events.EventFromStep(preload_step_event)
      if preload_event:
        preloaded_urls.add(preload_event.args['url'])
    parser_requests = self.ParserDiscoverableRequests(request)
    preloaded_root_requests = filter(
        lambda r: r.url in preloaded_urls, parser_requests)
    # We can actually fetch the whole redirect chain.
    return [request] + list(itertools.chain.from_iterable(
        [self.dependencies_lens.GetRedirectChain(r)
         for r in preloaded_root_requests]))


def _PrintSummary(prefetch_view, user_lens):
  requests = prefetch_view.trace.request_track.GetEvents()
  first_request = prefetch_view.trace.request_track.GetEvents()[0]
  parser_requests = prefetch_view.ExpandRedirectChains(
      prefetch_view.ParserDiscoverableRequests(first_request))
  preloaded_requests = prefetch_view.ExpandRedirectChains(
      prefetch_view.PreloadedRequests(first_request))
  print '%d requests, %d parser from the main request, %d preloaded' % (
      len(requests), len(parser_requests), len(preloaded_requests))
  print 'Time to user satisfaction: %.02fms' % (
      prefetch_view.graph.Cost() + user_lens.PostloadTimeMsec())

  print 'With 0-cost prefetched resources...'
  new_costs = {r.request_id: 0. for r in preloaded_requests}
  prefetch_view.graph.UpdateRequestsCost(new_costs)
  print 'Time to user satisfaction: %.02fms' % (
      prefetch_view.graph.Cost() + user_lens.PostloadTimeMsec())


def main(filename):
  trace = loading_trace.LoadingTrace.FromJsonFile(filename)
  dependencies_lens = request_dependencies_lens.RequestDependencyLens(trace)
  user_lens = user_satisfied_lens.FirstContentfulPaintLens(trace)
  prefetch_view = PrefetchSimulationView(trace, dependencies_lens, user_lens)
  _PrintSummary(prefetch_view, user_lens)


if __name__ == '__main__':
  import sys
  main(sys.argv[1])
