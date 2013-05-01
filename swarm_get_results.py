#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Retrieves all the output that the Swarm server has produced for requests with
that name.
"""

import json
import logging
import optparse
import Queue
import re
import sys
import threading
import time
import urllib

#from common import find_depot_tools  # pylint: disable=W0611
#
## From the depot tools
#import fix_encoding

import run_isolated

SHARD_LINE = re.compile(
    r'^Note: This is test shard ([0-9]+) of ([0-9]+)\.$', re.MULTILINE)


class Failure(Exception):
  """Generic failure."""
  pass


def get_test_keys(swarm_base_url, test_name, timeout):
  """Returns the Swarm test key for each shards of test_name."""
  assert isinstance(timeout, float)
  key_data = urllib.urlencode([('name', test_name)])
  url = '%s/get_matching_test_cases?%s' % (swarm_base_url, key_data)
  response = run_isolated.url_open(url, retry_404=True, timeout=timeout)
  if response is None:
    raise Failure(
        'Error: Unable to find any tests with the name, %s, on swarm server'
          % test_name)
  result = response.read()
  # TODO(maruel): Compare exact string.
  if 'No matching' in result:
    raise Failure(
        'Error: Unable to find any tests with the name, %s, on swarm server'
          % test_name)
  return json.loads(result) or []


class Bit(object):
  """Thread safe setable bit."""
  _lock = threading.Lock()
  _value = False

  def get(self):
    with self._lock:
      return self._value

  def set(self):
    with self._lock:
      self._value = True


def now():
  """Exists so it can be mocked easily."""
  return time.time()


def retrieve_results(base_url, test_key, timeout, should_stop):
  """Retrieves results for a single test_key."""
  assert isinstance(timeout, float)
  params = [('r', test_key)]
  result_url = '%s/get_result?%s' % (base_url, urllib.urlencode(params))
  start = now()
  while True:
    if timeout and (now() - start) >= timeout:
      logging.warning('retrieve_results(%s) timed out', base_url)
      return None
    # Do retries ourselve.
    response = run_isolated.url_open(
        result_url, retry_404=False, retry_50x=False)
    if response is None:
      # Aggressively poll for results. Do not use retry_404 so
      # should_stop is polled more often.
      remaining = min(5, timeout - (now() - start)) if timeout else 5
      if remaining > 0:
        run_isolated.HttpService.sleep_before_retry(1, remaining)
    else:
      try:
        data = json.load(response)
      except (ValueError, TypeError):
        logging.warning(
            'Received corrupted data for test_key %s. Retrying.', test_key)
      else:
        if data['output']:
          # Ignore the result code.
          run_isolated.url_open('%s/cleanup_results' % base_url, data=params)
          return data
    if should_stop.get():
      return None


def yield_results(swarm_base_url, shard_count, test_keys, timeout):
  """Yields swarm test results from the swarm server as (index, result).

  If shard_count < len(test_keys), it will return as soon as all the shards
  under shard_count are retrieved. It may return duplicate if they are returned
  before the slowest shard.
  """
  shards_remaining = range(shard_count)
  results = Queue.Queue()
  threads = []
  should_stop = Bit()
  def append(k):
    """Swallows exceptions."""
    try:
      r = retrieve_results(swarm_base_url, k, timeout, should_stop)
    except Exception as e:
      logging.exception('Dang')
      r = e
    results.put(r)
  for test_key in test_keys:
    t = threading.Thread(target=append, args=(test_key,))
    t.daemon = True
    t.start()
    threads.append(t)

  results_remaining = len(test_keys)
  try:
    while shards_remaining and results_remaining:
      # Retrieve the number of shard count, even if lower than test_keys.
      result = results.get()
      results_remaining -= 1
      if isinstance(result, Exception):
        raise result
      if result:
        # TODO(maruel): Quite adhoc.
        match = SHARD_LINE.search(result['output'])
        shard_index = int(match.group(1)) - 1 if match else 0
        if shard_index in shards_remaining:
          shards_remaining.remove(shard_index)
          yield shard_index, result
        else:
          logging.warning('Ignoring duplicate shard index %d', shard_index)
  finally:
    # Done, kill the remaining threads.
    should_stop.set()
    for t in threads:
      t.join()


def parse_args():
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog [options] test_name',
      description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-u', '--url', default='http://localhost:8080',
      help='Specify the url of the Swarm server, defaults: %default')
  parser.add_option(
      '-v', '--verbose', action='store_true',
      help='Print verbose logging')
  parser.add_option(
      '-t', '--timeout',
      type='float',
      default=run_isolated.URL_OPEN_TIMEOUT,
      help='Timeout to wait for result, set to 0 for no timeout; default: '
           '%default s')
  # TODO(csharp): Change default to 0 once all callers have been updated to
  # pass in --shards.
  parser.add_option(
      '-s', '--shards',
      default=-1,
      type=int,
      help='Specify the number of shards that the test was split into '
           '(i.e. how many shards should be retrieved from swarm.')

  (options, args) = parser.parse_args()
  if not args:
    parser.error('Must specify one test name.')
  elif len(args) > 1:
    parser.error('Must specify only one test name.')
  options.url = options.url.rstrip('/')
  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)
  return parser, options, args[0]


def main():
  parser, options, test_name = parse_args()
  try:
    test_keys = get_test_keys(options.url, test_name, options.timeout)
  except Failure as e:
    parser.error(e.args[0])
  if not test_keys:
    parser.error('No test keys to get results with.')

  options.shards = len(test_keys) if options.shards == -1 else options.shards
  exit_code = None
  for _index, output in yield_results(
      options.url, options.shards, test_keys, options.timeout):
    print(
        '%s/%s: %s' % (
            output['machine_id'], output['machine_tag'], output['exit_codes']))
    print(''.join('  %s\n' % l for l in output['output'].splitlines()))
    exit_code = max(exit_code, max(map(int, output['exit_codes'].split(','))))

  return exit_code


if __name__ == '__main__':
  #fix_encoding.fix_encoding()
  sys.exit(main())
