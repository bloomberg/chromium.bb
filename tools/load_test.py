#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Uploads a ton of stuff to isolateserver to test its handling.

Generates an histogram with the latencies to download a just uploaded file.

Note that it only looks at uploading and downloading and do not test
/content/contains, which is datastore read bound.
"""

import functools
import hashlib
import json
import logging
import optparse
import os
import random
import re
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


class Randomness(object):
  def __init__(self, random_pool_size=1024):
    """Creates 1mb of random data in a pool in 1kb chunks."""
    self.pool = [
      ''.join(chr(random.randrange(256)) for _ in xrange(1024))
      for _ in xrange(random_pool_size)
    ]

  def gen(self, size):
    """Returns a str containing random data from the pool of size |size|."""
    chunks = int(size / 1024)
    rest = size - (chunks*1024)
    data = ''.join(random.choice(self.pool) for _ in xrange(chunks))
    data += random.choice(self.pool)[:rest]
    return data


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


def from_units(text):
  """Convert a text to numbers.

  Example: from_unit('0.1k') == 102
  """
  UNITS = ('', 'k', 'm', 'g', 't', 'p', 'e', 'z', 'y')
  match = re.match(r'^([0-9\.]+)(|[' + ''.join(UNITS[1:]) + r'])$', text)
  if not match:
    return None

  number = float(match.group(1))
  unit = match.group(2)
  return int(number * 1024**UNITS.index(unit))


def unit_arg(option, opt, value, parser):
  """OptionParser callback that supports units like 10.5m or 20k."""
  actual = from_units(value)
  if actual is None:
    parser.error('Invalid value \'%s\' for %s' % (value, opt))
  setattr(parser.values, option.dest, actual)


def unit_option(parser, *args, **kwargs):
  """Add an option that uses unit_arg()."""
  parser.add_option(
      *args, type='str', metavar='N', action='callback', callback=unit_arg,
      nargs=1, **kwargs)


class Progress(threading_utils.Progress):
  def __init__(self, *args, **kwargs):
    super(Progress, self).__init__(*args, **kwargs)
    self.total = 0

  def update_item(self, name):  # pylint: disable=W0221
    super(Progress, self).update_item(name, True, False)

  def increment_count(self):
    super(Progress, self).update_item(None, False, True)

  def gen_line(self, name):
    """Generates the line to be printed.

    |name| is actually the item size.
    """
    if name:
      self.total += name
      name = to_units(name)

    next_line = ('[%*d/%d] %6.2fs %8s %8s') % (
        len(str(self.size)), self.index,
        self.size,
        time.time() - self.start,
        name,
        to_units(self.total))
    # Fill it with whitespace only if self.use_cr_only is set.
    prefix = ''
    if self.use_cr_only and self.last_printed_line:
      prefix = '\r'
    if self.use_cr_only:
      suffix = ' ' * max(0, len(self.last_printed_line) - len(next_line))
    else:
      suffix = '\n'
    return '%s%s%s' % (prefix, next_line, suffix), next_line


def print_results(results, columns, buckets):
  delays = [i[0] for i in results if isinstance(i[0], float)]
  failures = [i for i in results if not isinstance(i[0], float)]
  sizes = [i[1] for i in results]

  print('%sSIZES%s (bytes):' % (colorama.Fore.RED, colorama.Fore.RESET))
  graph_histo(gen_histo(sizes, buckets), columns, '%8d')
  print('')
  total_size = sum(sizes)
  print('Total size  : %s' % to_units(total_size))
  print('Total items : %d' % len(sizes))
  print('Average size: %s' % to_units(total_size / len(sizes)))
  print('Largest item: %s' % to_units(max(sizes)))
  print('')
  print('%sDELAYS%s (seconds):' % (colorama.Fore.RED, colorama.Fore.RESET))
  graph_histo(gen_histo(delays, buckets), columns, '%8.3f')

  if failures:
    print('')
    print('%sFAILURES%s:' % (colorama.Fore.RED, colorama.Fore.RESET))
    print('\n'.join('  %s (%s)' % (i[0], to_units(i[1])) for i in failures))


def gen_size(mid_size):
  """Interesting non-guassian distribution, to get a few very large files.

  Found via guessing on Wikipedia. Module 'random' says it's threadsafe.
  """
  return int(random.gammavariate(3, 2) * mid_size / 4)


def send_and_receive(
    random_pool, dry_run, isolate_server, namespace, token, progress, size):
  """Sends a random file and gets it back.

  Returns (delay, size)
  """
  # Create a file out of the pool.
  start = time.time()
  content = random_pool.gen(size)
  data = zlib.compress(content, 0)
  sha1 = hashlib.sha1(content).hexdigest()
  try:
    if not dry_run:
      # 1 Upload.
      isolateserver.upload_file(isolate_server, namespace, data, sha1, token)

      # 2 Download.
      # Only count download time when not in dry run time.
      # TODO(maruel): Count the number of retries!
      start = time.time()
      net.url_read(
          isolate_server + '/content/retrieve/%s/%s' % (namespace, sha1))
    else:
      time.sleep(size / 10.)
    duration = max(0, time.time() - start)
  except run_isolated.MappingError as e:
    duration = str(e)
  progress.update_item(size if isinstance(duration, float) else 0)
  return (duration, size)


def main():
  colorama.init()

  try:
    _, columns = os.popen('stty size', 'r').read().split()
  except (IOError, OSError, ValueError):
    columns = 80

  # Effectively disable logging to reduce spew.
  logging.basicConfig(level=logging.FATAL)

  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-I', '--isolate-server', metavar='URL',
      default='https://isolateserver-dev.appspot.com')
  parser.add_option(
      '--namespace', default='temporary%d-gzip' % time.time(), metavar='XX',
      help='Namespace to use on the server, default: %default')
  parser.add_option(
      '--threads', type='int', default=16, metavar='N',
      help='Parallel worker threads to use, default:%default')
  unit_option(
      parser, '--items', default=0, help='Number of items to upload')
  unit_option(
      parser, '--max-size', default=0,
      help='Loop until this amount of data was transferred')
  unit_option(
      parser, '--mid-size', default=100*1024,
      help='Rough average size of each item, default:%default')
  parser.add_option(
      '--columns', type='int', default=columns, metavar='N',
      help='For histogram display, default:%default')
  parser.add_option(
      '--buckets', type='int', default=20, metavar='N',
      help='Number of buckets for histogram display, default:%default')
  parser.add_option(
      '--dump', metavar='FOO.JSON', help='Dumps to json file')
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
  token = None
  if not options.dry_run:
    url = options.isolate_server + '/content/get_token?from_smoke_test=1'
    token = urllib.quote(net.url_read(url))
    print(' - Got token after %.1fs' % (time.time() - start))

  random_pool = Randomness()
  print(' - Generated pool after %.1fs' % (time.time() - start))

  progress = Progress(options.items)
  do_item = functools.partial(
      send_and_receive,
      random_pool,
      options.dry_run,
      options.isolate_server,
      options.namespace,
      token,
      progress)

  # TODO(maruel): Handle Ctrl-C should:
  # - Stop adding tasks.
  # - Stop scheduling tasks in ThreadPool.
  # - Wait for the remaining ungoing tasks to complete.
  # - Still print details and write the json file.
  with threading_utils.ThreadPoolWithProgress(
      progress, options.threads, options.threads, 0) as pool:
    if options.items:
      for _ in xrange(options.items):
        pool.add_task(0, do_item, gen_size(options.mid_size))
    elif options.max_size:
      # This one is approximate.
      total = 0
      while True:
        size = gen_size(options.mid_size)
        progress.increment_count()
        progress.print_update()
        pool.add_task(0, do_item, size)
        total += size
        if total >= options.max_size:
          break
    results = sorted(pool.join())

  print('')
  print(' - Took %.1fs.' % (time.time() - start))
  print('')
  print_results(results, options.columns, options.buckets)
  if options.dump:
    with open(options.dump, 'w') as f:
      json.dump(results, f, separators=(',',':'))
  return 0


if __name__ == '__main__':
  sys.exit(main())
