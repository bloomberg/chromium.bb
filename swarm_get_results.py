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
import sys
import threading
import time
import urllib

#from common import find_depot_tools  # pylint: disable=W0611
#
## From the depot tools
#import fix_encoding

import run_isolated


class Failure(Exception):
  """Generic failure."""
  pass


def get_test_keys(swarm_base_url, test_name, timeout):
  """Returns the Swarm test key for each shards of test_name."""
  assert isinstance(timeout, float)
  key_data = urllib.urlencode([('name', test_name)])
  url = '%s/get_matching_test_cases?%s' % (swarm_base_url, key_data)

  for i in range(run_isolated.URL_OPEN_MAX_ATTEMPTS):
    response = run_isolated.url_open(url, retry_404=True, timeout=timeout)
    if response is None:
      raise Failure(
          'Error: Unable to find any tests with the name, %s, on swarm server'
          % test_name)

    result = response.read()
    # TODO(maruel): Compare exact string.
    if 'No matching' in result:
      logging.warning('Unable to find any tests with the name, %s, on swarm '
                      'server' % test_name)
      if i != run_isolated.URL_OPEN_MAX_ATTEMPTS:
        run_isolated.HttpService.sleep_before_retry(i, None)
      continue
    return json.loads(result)

  raise Failure(
      'Error: Unable to find any tests with the name, %s, on swarm server'
      % test_name)


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
      return {}
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
        data = json.load(response) or {}
      except (ValueError, TypeError):
        logging.warning(
            'Received corrupted data for test_key %s. Retrying.', test_key)
      else:
        if data['output']:
          return data
    if should_stop.get():
      return {}


def yield_results(swarm_base_url, test_keys, timeout, max_threads):
  """Yields swarm test results from the swarm server as (index, result).

  Duplicate shards are ignored, the first one to complete is returned.

  max_threads is optional and is used to limit the number of parallel fetches
  done. Since in general the number of test_keys is in the range <=10, it's not
  worth normally to limit the number threads. Mostly used for testing purposes.
  """
  shards_remaining = range(len(test_keys))
  number_threads = (
      min(max_threads, len(test_keys)) if max_threads else len(test_keys))
  should_stop = Bit()
  results_remaining = len(test_keys)
  with run_isolated.ThreadPool(number_threads, number_threads, 0) as pool:
    try:
      for test_key in test_keys:
        pool.add_task(
            0, retrieve_results, swarm_base_url, test_key, timeout, should_stop)
      while shards_remaining and results_remaining:
        result = pool.get_one_result()
        results_remaining -= 1
        if not result:
          # Failed to retrieve one key.
          continue
        shard_index = result['config_instance_index']
        if shard_index in shards_remaining:
          shards_remaining.remove(shard_index)
          yield shard_index, result
        else:
          logging.warning('Ignoring duplicate shard index %d', shard_index)
          # Pop the last entry, there's no such shard.
          shards_remaining.pop()
    finally:
      # Done, kill the remaining threads.
      should_stop.set()


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
  # TODO(maruel): Remove once the masters have been updated.
  parser.add_option(
      '-s', '--shards',
      help=optparse.SUPPRESS_HELP)

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
      options.url, test_keys, options.timeout, None):
    print(
        '%s/%s: %s' % (
            output['machine_id'], output['machine_tag'], output['exit_codes']))
    print(''.join('  %s\n' % l for l in output['output'].splitlines()))
    exit_code = max(exit_code, max(map(int, output['exit_codes'].split(','))))

  return exit_code


if __name__ == '__main__':
  #fix_encoding.fix_encoding()
  sys.exit(main())
