#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Uploads a ton of stuff to isolateserver to test its handling.

Generates an histogram with the latencies to download a just uploaded file.
"""

import hashlib
import logging
import optparse
import os
import random
import sys
import time
import urllib
import zlib

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(0, ROOT_DIR)

from third_party import colorama

import isolateserver
import run_isolated

from utils import net
from utils import threading_utils


def gen_histo(data, buckets):
  """Cheap histogram."""
  if not data:
    return {}

  minimum = min(data)
  maximum = max(data)
  if minimum == maximum:
    return {data[0]: len(data)}

  buckets = min(len(data), buckets)
  bucket_size = (maximum-minimum)/buckets
  out = dict((i, 0) for i in xrange(buckets))
  for i in data:
    out[min(int((i-minimum)/bucket_size), buckets-1)] += 1
  return dict(((k*bucket_size)+minimum, v) for k, v in out.iteritems())


def graph_histo(data, columns, key_format):
  """Graphs an histogram."""
  # TODO(maruel): Add dots for tens.
  if not data:
    # Nothing to print.
    return

  width = columns - 10
  assert width > 1

  maxvalue = max(data.itervalues())
  if all(isinstance(i, int) for i in data.itervalues()) and maxvalue < width:
    width = maxvalue
  norm = float(maxvalue) / width

  for k in sorted(data):
    length = int(data[k] / norm)
    print((key_format + ': %s%s%s') % (
      k,
      colorama.Fore.GREEN, '*' * length, colorama.Fore.RESET))


def to_units(number):
  """Convert a string to numbers."""
  UNITS = ('', 'k', 'm', 'g', 't', 'p', 'e', 'z', 'y')
  unit = 0
  while number >= 1024.:
    unit += 1
    number = number / 1024.
    if unit == len(UNITS) - 1:
      break
  if unit:
    return '%.2f%s' % (number, UNITS[unit])
  return '%d' % number


def main():
  colorama.init()

  try:
    _, columns = os.popen('stty size', 'r').read().split()
  except (IOError, OSError, ValueError):
    columns = 80

  # Effectively disable logging to reduce spew.
  logging.basicConfig(level=logging.FATAL)
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  # Create a temporary non-compressed name space and store zeros in there.
  parser.add_option(
      '-I', '--isolate-server', metavar='URL',
      default='https://isolateserver-dev.appspot.com')
  parser.add_option(
      '--threads', type='int', default=16, metavar='N',
      help='Parallel worker threads to use, default:%default')
  parser.add_option(
      '--items', type='int', default=0, metavar='N',
      help='Number of items to upload')
  parser.add_option(
      '--max-size', type='int', default=0, metavar='N',
      help='Loop until this amount of data was transferred')
  parser.add_option(
      '--mid-size', type='int', default=100*1024, metavar='N',
      help='Rough average size of each item, default:%default')
  parser.add_option(
      '--columns', type='int', default=columns, metavar='N',
      help='For histogram display, default:%default')
  parser.add_option(
      '--buckets', type='int', default=20, metavar='N',
      help='Number of buckets for histogram display, default:%default')
  parser.add_option(
      '--dry-run', action='store_true', help='Do not send anything')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported args: %s' % args)
  if bool(options.max_size) == bool(options.items):
    parser.error(
        'Use one of --max-size or --items.\n'
        '  Use --max-size if you want to run it until NN bytes where '
        'transfered.\n'
        '  Otherwise use --items to run it for NN items.')
  if not options.dry_run:
    options.isolate_server = options.isolate_server.rstrip('/')
    if not options.isolate_server:
      parser.error('--isolate-server is required.')

  print(
      ' - Using %d thread,  items=%d,  max-size=%d,  mid-size=%d' % (
      options.threads, options.items, options.max_size, options.mid_size))
  if options.dry_run:
    print(' - %sDRY RUN MODE%s' % (colorama.Fore.GREEN, colorama.Fore.RESET))

  start = time.time()
  if not options.dry_run:
    url = options.isolate_server + '/content/get_token?from_smoke_test=1'
    token = urllib.quote(net.url_read(url))
    print(' - Got token after %.1fs' % (time.time() - start))

  # Create 10mb of random data in a pool in 1kb chunks.
  random_pool_size = 10*1024
  random_pool = [
    ''.join(chr(random.randrange(256)) for _ in xrange(1024))
    for _ in xrange(random_pool_size)
  ]
  print(' - Generated pool after %.1fs' % (time.time() - start))

  def gen_file(size):
    """Returns a str containing random data from the pool of size |size|."""
    chunks = int(size / 1024)
    rest = size - (chunks*1024)

    data = ''.join(random.choice(random_pool) for _ in xrange(chunks))
    data += random.choice(random_pool) [:rest]
    return data

  def gen_size():
    """Interesting non-guassian distribution, to get a few very large files.

    Found via guessing on Wikipedia. Module 'random' says it's threadsafe.
    """
    return random.gammavariate(3, 2) * options.mid_size / 4

  progress = threading_utils.Progress(options.items)

  def send_and_receive(size):
    """Sends a packet and gets it back. Notes the time it took before
    success.

    Returns (delay, size)
    """
    # Create a file out of the pool.
    start = time.time()
    content = gen_file(size)
    data = zlib.compress(content, 0)
    sha1 = hashlib.sha1(content).hexdigest()
    try:
      if not options.dry_run:
        # 1 Upload.
        isolateserver.upload_file(
            options.isolate_server, namespace, data, sha1, token)

        # 2 Download.
        # Only count download time when not in dry run time.
        # TODO(maruel): Count the number of retries!
        start = time.time()
        net.url_read(
            options.isolate_server +
            '/content/retrieve/%s/%s' % (namespace, sha1))
      duration = max(0, time.time()-start)
    except run_isolated.MappingError as e:
      duration = e
    progress.update_item(to_units(size), True, not options.items)
    return (duration, size)

  # The namespace must end in '-gzip' since all files are now compressed
  # before being uploaded.
  namespace = 'temporary%d-gzip' % time.time()
  with threading_utils.ThreadPoolWithProgress(
      progress, options.threads, options.threads, 0) as pool:
    if options.items:
      for i in xrange(options.items):
        pool.add_task(0, send_and_receive, int(gen_size()))
    elif options.max_size:
      # This one is approximate.
      total = 0
      while True:
        size = int(gen_size())
        pool.add_task(0, send_and_receive, size)
        total += size
        if total >= options.max_size:
          break
    # TODO(maruel): Create a progress indicator, use QueueWithProgress from
    # run_test_cases.
    results = sorted(pool.join())

  print('')
  delays = [i[0] for i in results if isinstance(i[0], float)]
  failures = [i for i in results if not isinstance(i[0], float)]
  sizes = [i[1] for i in results]

  print('%sSIZES%s (bytes):' % (colorama.Fore.RED, colorama.Fore.RESET))
  graph_histo(gen_histo(sizes, options.buckets), options.columns, '%8d')
  print('')
  total_size = sum(sizes)
  print('Total size  : %s' % to_units(total_size))
  print('Total items : %d' % len(sizes))
  print('Average size: %s' % to_units(total_size / len(sizes)))
  print('Largest item: %s' % to_units(max(sizes)))
  print('')
  print('%sDELAYS%s (seconds):' % (colorama.Fore.RED, colorama.Fore.RESET))
  graph_histo(gen_histo(delays, options.buckets), options.columns, '%8.3g')

  if failures:
    print('')
    print('%sFAILURES%s:' % (colorama.Fore.RED, colorama.Fore.RESET))
    print('\n'.join('  %s (%s)' % (i[0], to_units(i[1])) for i in failures))
  return 0


if __name__ == '__main__':
  sys.exit(main())
