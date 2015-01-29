#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to trigger tasks or retrieve results from a Swarming server."""

__version__ = '0.6.1'

import collections
import json
import logging
import os
import re
import shutil
import StringIO
import subprocess
import sys
import threading
import time
import urllib
import urlparse
import zipfile

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from third_party.chromium import natsort
from utils import net
from utils import on_error
from utils import threading_utils
from utils import tools
from utils import zip_package

import auth
import isolated_format
import isolateserver
import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


class Failure(Exception):
  """Generic failure."""
  pass


### Isolated file handling.


def isolated_upload_zip_bundle(isolate_server, bundle):
  """Uploads a zip package to Isolate Server and returns raw fetch URL.

  Args:
    isolate_server: URL of an Isolate Server.
    bundle: instance of ZipPackage to upload.

  Returns:
    URL to get the file from.
  """
  # Swarming bot needs to be able to grab the file from the Isolate Server using
  # a simple HTTPS GET. Use 'default' namespace so that the raw data returned to
  # a bot is not zipped, since the swarming_bot doesn't understand compressed
  # data. This namespace have nothing to do with |namespace| passed to
  # run_isolated.py that is used to store files for isolated task.
  logging.info('Zipping up and uploading files...')
  start_time = time.time()
  isolate_item = isolateserver.BufferItem(bundle.zip_into_buffer())
  with isolateserver.get_storage(isolate_server, 'default') as storage:
    uploaded = storage.upload_items([isolate_item])
    bundle_url = storage.get_fetch_url(isolate_item)
  elapsed = time.time() - start_time
  if isolate_item in uploaded:
    logging.info('Upload complete, time elapsed: %f', elapsed)
  else:
    logging.info('Zip file already on server, time elapsed: %f', elapsed)
  return bundle_url


def isolated_get_data(isolate_server):
  """Returns the 'data' section with all files necessary to bootstrap a task
  execution running an isolated task.

  It's mainly zipping run_isolated.zip over and over again.
  TODO(maruel): Get rid of this with.
  https://code.google.com/p/swarming/issues/detail?id=173
  """
  bundle = zip_package.ZipPackage(ROOT_DIR)
  bundle.add_buffer(
      'run_isolated.zip',
      run_isolated.get_as_zip_package().zip_into_buffer(compress=False))
  bundle_url = isolated_upload_zip_bundle(isolate_server, bundle)
  return [(bundle_url, 'swarm_data.zip')]


def isolated_get_run_commands(
    isolate_server, namespace, isolated_hash, extra_args, verbose):
  """Returns the 'commands' to run an isolated task via run_isolated.zip.

  Returns:
    commands list to be added to the request.
  """
  run_cmd = [
    'python', 'run_isolated.zip',
    '--isolated', isolated_hash,
    '--isolate-server', isolate_server,
    '--namespace', namespace,
  ]
  if verbose:
    run_cmd.append('--verbose')
  # Pass all extra args for run_isolated.py, it will pass them to the command.
  if extra_args:
    run_cmd.append('--')
    run_cmd.extend(extra_args)
  return run_cmd


def isolated_archive(isolate_server, namespace, isolated, algo, verbose):
  """Archives a .isolated and all the dependencies on the Isolate Server."""
  logging.info(
      'isolated_archive(%s, %s, %s)', isolate_server, namespace, isolated)
  print('Archiving: %s' % isolated)
  cmd = [
    sys.executable,
    os.path.join(ROOT_DIR, 'isolate.py'),
    'archive',
    '--isolate-server', isolate_server,
    '--namespace', namespace,
    '--isolated', isolated,
  ]
  cmd.extend(['--verbose'] * verbose)
  logging.info(' '.join(cmd))
  if subprocess.call(cmd, verbose):
    return None
  return isolated_format.hash_file(isolated, algo)


def isolated_to_hash(isolate_server, namespace, arg, algo, verbose):
  """Archives a .isolated file if needed.

  Returns the file hash to trigger and a bool specifying if it was a file (True)
  or a hash (False).
  """
  if arg.endswith('.isolated'):
    file_hash = isolated_archive(isolate_server, namespace, arg, algo, verbose)
    if not file_hash:
      on_error.report('Archival failure %s' % arg)
      return None, True
    return file_hash, True
  elif isolated_format.is_valid_hash(arg, algo):
    return arg, False
  else:
    on_error.report('Invalid hash %s' % arg)
    return None, False


def isolated_handle_options(options, args):
  """Handles '--isolated <isolated>', '<isolated>' and '-- <args...>' arguments.

  Returns:
    tuple(command, data).
  """
  isolated_cmd_args = []
  if not options.isolated:
    if '--' in args:
      index = args.index('--')
      isolated_cmd_args = args[index+1:]
      args = args[:index]
    else:
      # optparse eats '--' sometimes.
      isolated_cmd_args = args[1:]
      args = args[:1]
    if len(args) != 1:
      raise ValueError(
          'Use --isolated, --raw-cmd or \'--\' to pass arguments to the called '
          'process.')
    # Old code. To be removed eventually.
    options.isolated, is_file = isolated_to_hash(
        options.isolate_server, options.namespace, args[0],
        isolated_format.get_hash_algo(options.namespace), options.verbose)
    if not options.isolated:
      raise ValueError('Invalid argument %s' % args[0])
  elif args:
    is_file = False
    if '--' in args:
      index = args.index('--')
      isolated_cmd_args = args[index+1:]
      if index != 0:
        raise ValueError('Unexpected arguments.')
    else:
      # optparse eats '--' sometimes.
      isolated_cmd_args = args

  command = isolated_get_run_commands(
      options.isolate_server, options.namespace, options.isolated,
      isolated_cmd_args, options.verbose)

  # If a file name was passed, use its base name of the isolated hash.
  # Otherwise, use user name as an approximation of a task name.
  if not options.task_name:
    if is_file:
      key = os.path.splitext(os.path.basename(args[0]))[0]
    else:
      key = options.user
    options.task_name = u'%s/%s/%s' % (
        key,
        '_'.join(
            '%s=%s' % (k, v)
            for k, v in sorted(options.dimensions.iteritems())),
        options.isolated)

  try:
    data = isolated_get_data(options.isolate_server)
  except (IOError, OSError):
    on_error.report('Failed to upload the zip file')
    raise ValueError('Failed to upload the zip file')

  return command, data


### Triggering.


TaskRequest = collections.namedtuple(
    'TaskRequest',
    [
      'command',
      'data',
      'dimensions',
      'env',
      'expiration',
      'hard_timeout',
      'idempotent',
      'io_timeout',
      'name',
      'priority',
      'tags',
      'user',
      'verbose',
    ])


def task_request_to_raw_request(task_request):
  """Returns the json dict expected by the Swarming server for new request.

  This is for the v1 client Swarming API.
  """
  return {
    'name': task_request.name,
    'parent_task_id': os.environ.get('SWARMING_TASK_ID', ''),
    'priority': task_request.priority,
    'properties': {
      'commands': [task_request.command],
      'data': task_request.data,
      'dimensions': task_request.dimensions,
      'env': task_request.env,
      'execution_timeout_secs': task_request.hard_timeout,
      'io_timeout_secs': task_request.io_timeout,
      'idempotent': task_request.idempotent,
    },
    'scheduling_expiration_secs': task_request.expiration,
    'tags': task_request.tags,
    'user': task_request.user,
  }


def swarming_handshake(swarming):
  """Initiates the connection to the Swarming server."""
  headers = {'X-XSRF-Token-Request': '1'}
  response = net.url_read_json(
      swarming + '/swarming/api/v1/client/handshake',
      headers=headers,
      data={})
  if not response:
    logging.error('Failed to handshake with server')
    return None
  logging.info('Connected to server version: %s', response['server_version'])
  return response['xsrf_token']


def swarming_trigger(swarming, raw_request, xsrf_token):
  """Triggers a request on the Swarming server and returns the json data.

  It's the low-level function.

  Returns:
    {
      'request': {
        'created_ts': u'2010-01-02 03:04:05',
        'name': ..
      },
      'task_id': '12300',
    }
  """
  logging.info('Triggering: %s', raw_request['name'])

  headers = {'X-XSRF-Token': xsrf_token}
  result = net.url_read_json(
      swarming + '/swarming/api/v1/client/request',
      data=raw_request,
      headers=headers)
  if not result:
    on_error.report('Failed to trigger task %s' % raw_request['name'])
    return None
  return result


def setup_googletest(env, shards, index):
  """Sets googletest specific environment variables."""
  if shards > 1:
    env = env.copy()
    env['GTEST_SHARD_INDEX'] = str(index)
    env['GTEST_TOTAL_SHARDS'] = str(shards)
  return env


def trigger_task_shards(swarming, task_request, shards):
  """Triggers one or many subtasks of a sharded task.

  Returns:
    Dict with task details, returned to caller as part of --dump-json output.
    None in case of failure.
  """
  def convert(index):
    req = task_request
    if shards > 1:
      req = req._replace(
          env=setup_googletest(req.env, shards, index),
          name='%s:%s:%s' % (req.name, index, shards))
    return task_request_to_raw_request(req)

  requests = [convert(index) for index in xrange(shards)]
  xsrf_token = swarming_handshake(swarming)
  if not xsrf_token:
    return None
  tasks = {}
  priority_warning = False
  for index, request in enumerate(requests):
    task = swarming_trigger(swarming, request, xsrf_token)
    if not task:
      break
    logging.info('Request result: %s', task)
    if (not priority_warning and
        task['request']['priority'] != task_request.priority):
      priority_warning = True
      print >> sys.stderr, (
          'Priority was reset to %s' % task['request']['priority'])
    tasks[request['name']] = {
      'shard_index': index,
      'task_id': task['task_id'],
      'view_url': '%s/user/task/%s' % (swarming, task['task_id']),
    }

  # Some shards weren't triggered. Abort everything.
  if len(tasks) != len(requests):
    if tasks:
      print >> sys.stderr, 'Only %d shard(s) out of %d were triggered' % (
          len(tasks), len(requests))
      for task_dict in tasks.itervalues():
        abort_task(swarming, task_dict['task_id'])
    return None

  return tasks


### Collection.


# How often to print status updates to stdout in 'collect'.
STATUS_UPDATE_INTERVAL = 15 * 60.


class State(object):
  """States in which a task can be.

  WARNING: Copy-pasted from appengine/swarming/server/task_result.py. These
  values are part of the API so if they change, the API changed.

  It's in fact an enum. Values should be in decreasing order of importance.
  """
  RUNNING = 0x10
  PENDING = 0x20
  EXPIRED = 0x30
  TIMED_OUT = 0x40
  BOT_DIED = 0x50
  CANCELED = 0x60
  COMPLETED = 0x70

  STATES = (RUNNING, PENDING, EXPIRED, TIMED_OUT, BOT_DIED, CANCELED, COMPLETED)
  STATES_RUNNING = (RUNNING, PENDING)
  STATES_NOT_RUNNING = (EXPIRED, TIMED_OUT, BOT_DIED, CANCELED, COMPLETED)
  STATES_DONE = (TIMED_OUT, COMPLETED)
  STATES_ABANDONED = (EXPIRED, BOT_DIED, CANCELED)

  _NAMES = {
    RUNNING: 'Running',
    PENDING: 'Pending',
    EXPIRED: 'Expired',
    TIMED_OUT: 'Execution timed out',
    BOT_DIED: 'Bot died',
    CANCELED: 'User canceled',
    COMPLETED: 'Completed',
  }

  @classmethod
  def to_string(cls, state):
    """Returns a user-readable string representing a State."""
    if state not in cls._NAMES:
      raise ValueError('Invalid state %s' % state)
    return cls._NAMES[state]


class TaskOutputCollector(object):
  """Assembles task execution summary (for --task-summary-json output).

  Optionally fetches task outputs from isolate server to local disk (used when
  --task-output-dir is passed).

  This object is shared among multiple threads running 'retrieve_results'
  function, in particular they call 'process_shard_result' method in parallel.
  """

  def __init__(self, task_output_dir, task_name, shard_count):
    """Initializes TaskOutputCollector, ensures |task_output_dir| exists.

    Args:
      task_output_dir: (optional) local directory to put fetched files to.
      task_name: name of the swarming task results belong to.
      shard_count: expected number of task shards.
    """
    self.task_output_dir = task_output_dir
    self.task_name = task_name
    self.shard_count = shard_count

    self._lock = threading.Lock()
    self._per_shard_results = {}
    self._storage = None

    if self.task_output_dir and not os.path.isdir(self.task_output_dir):
      os.makedirs(self.task_output_dir)

  def process_shard_result(self, shard_index, result):
    """Stores results of a single task shard, fetches output files if necessary.

    Modifies |result| in place.

    Called concurrently from multiple threads.
    """
    # Sanity check index is in expected range.
    assert isinstance(shard_index, int)
    if shard_index < 0 or shard_index >= self.shard_count:
      logging.warning(
          'Shard index %d is outside of expected range: [0; %d]',
          shard_index, self.shard_count - 1)
      return

    assert not 'isolated_out' in result
    result['isolated_out'] = None
    for output in result['outputs']:
      isolated_files_location = extract_output_files_location(output)
      if isolated_files_location:
        if result['isolated_out']:
          raise ValueError('Unexpected two task with output')
        result['isolated_out'] = isolated_files_location

    # Store result dict of that shard, ignore results we've already seen.
    with self._lock:
      if shard_index in self._per_shard_results:
        logging.warning('Ignoring duplicate shard index %d', shard_index)
        return
      self._per_shard_results[shard_index] = result

    # Fetch output files if necessary.
    if self.task_output_dir and result['isolated_out']:
      storage = self._get_storage(
          result['isolated_out']['server'],
          result['isolated_out']['namespace'])
      if storage:
        # Output files are supposed to be small and they are not reused across
        # tasks. So use MemoryCache for them instead of on-disk cache. Make
        # files writable, so that calling script can delete them.
        isolateserver.fetch_isolated(
            result['isolated_out']['hash'],
            storage,
            isolateserver.MemoryCache(file_mode_mask=0700),
            os.path.join(self.task_output_dir, str(shard_index)),
            False)

  def finalize(self):
    """Assembles and returns task summary JSON, shutdowns underlying Storage."""
    with self._lock:
      # Write an array of shard results with None for missing shards.
      summary = {
        'shards': [
          self._per_shard_results.get(i) for i in xrange(self.shard_count)
        ],
      }
      # Write summary.json to task_output_dir as well.
      if self.task_output_dir:
        tools.write_json(
            os.path.join(self.task_output_dir, 'summary.json'),
            summary,
            False)
      if self._storage:
        self._storage.close()
        self._storage = None
      return summary

  def _get_storage(self, isolate_server, namespace):
    """Returns isolateserver.Storage to use to fetch files."""
    assert self.task_output_dir
    with self._lock:
      if not self._storage:
        self._storage = isolateserver.get_storage(isolate_server, namespace)
      else:
        # Shards must all use exact same isolate server and namespace.
        if self._storage.location != isolate_server:
          logging.error(
              'Task shards are using multiple isolate servers: %s and %s',
              self._storage.location, isolate_server)
          return None
        if self._storage.namespace != namespace:
          logging.error(
              'Task shards are using multiple namespaces: %s and %s',
              self._storage.namespace, namespace)
          return None
      return self._storage


def extract_output_files_location(task_log):
  """Task log -> location of task output files to fetch.

  TODO(vadimsh,maruel): Use side-channel to get this information.
  See 'run_tha_test' in run_isolated.py for where the data is generated.

  Returns:
    Tuple (isolate server URL, namespace, isolated hash) on success.
    None if information is missing or can not be parsed.
  """
  if not task_log:
    return None
  match = re.search(
      r'\[run_isolated_out_hack\](.*)\[/run_isolated_out_hack\]',
      task_log,
      re.DOTALL)
  if not match:
    return None

  def to_ascii(val):
    if not isinstance(val, basestring):
      raise ValueError()
    return val.encode('ascii')

  try:
    data = json.loads(match.group(1))
    if not isinstance(data, dict):
      raise ValueError()
    isolated_hash = to_ascii(data['hash'])
    namespace = to_ascii(data['namespace'])
    isolate_server = to_ascii(data['storage'])
    if not file_path.is_url(isolate_server):
      raise ValueError()
    data = {
        'hash': isolated_hash,
        'namespace': namespace,
        'server': isolate_server,
        'view_url': '%s/browse?%s' % (isolate_server, urllib.urlencode(
            [('namespace', namespace), ('hash', isolated_hash)])),
    }
    return data
  except (KeyError, ValueError):
    logging.warning(
        'Unexpected value of run_isolated_out_hack: %s', match.group(1))
    return None


def now():
  """Exists so it can be mocked easily."""
  return time.time()


def retrieve_results(
    base_url, shard_index, task_id, timeout, should_stop, output_collector):
  """Retrieves results for a single task ID.

  Returns:
    <result dict> on success.
    None on failure.
  """
  assert isinstance(timeout, float), timeout
  result_url = '%s/swarming/api/v1/client/task/%s' % (base_url, task_id)
  output_url = '%s/swarming/api/v1/client/task/%s/output/all' % (
      base_url, task_id)
  started = now()
  deadline = started + timeout if timeout else None
  attempt = 0

  while not should_stop.is_set():
    attempt += 1

    # Waiting for too long -> give up.
    current_time = now()
    if deadline and current_time >= deadline:
      logging.error('retrieve_results(%s) timed out on attempt %d',
          base_url, attempt)
      return None

    # Do not spin too fast. Spin faster at the beginning though.
    # Start with 1 sec delay and for each 30 sec of waiting add another second
    # of delay, until hitting 15 sec ceiling.
    if attempt > 1:
      max_delay = min(15, 1 + (current_time - started) / 30.0)
      delay = min(max_delay, deadline - current_time) if deadline else max_delay
      if delay > 0:
        logging.debug('Waiting %.1f sec before retrying', delay)
        should_stop.wait(delay)
        if should_stop.is_set():
          return None

    # Disable internal retries in net.url_read_json, since we are doing retries
    # ourselves.
    # TODO(maruel): We'd need to know if it's a 404 and not retry at all.
    result = net.url_read_json(result_url, retry_50x=False)
    if not result:
      continue
    if result['state'] in State.STATES_NOT_RUNNING:
      out = net.url_read_json(output_url)
      result['outputs'] = (out or {}).get('outputs', [])
      if not result['outputs']:
        logging.error('No output found for task %s', task_id)
      # Record the result, try to fetch attached output files (if any).
      if output_collector:
        # TODO(vadimsh): Respect |should_stop| and |deadline| when fetching.
        output_collector.process_shard_result(shard_index, result)
      return result


def yield_results(
    swarm_base_url, task_ids, timeout, max_threads, print_status_updates,
    output_collector):
  """Yields swarming task results from the swarming server as (index, result).

  Duplicate shards are ignored. Shards are yielded in order of completion.
  Timed out shards are NOT yielded at all. Caller can compare number of yielded
  shards with len(task_keys) to verify all shards completed.

  max_threads is optional and is used to limit the number of parallel fetches
  done. Since in general the number of task_keys is in the range <=10, it's not
  worth normally to limit the number threads. Mostly used for testing purposes.

  output_collector is an optional instance of TaskOutputCollector that will be
  used to fetch files produced by a task from isolate server to the local disk.

  Yields:
    (index, result). In particular, 'result' is defined as the
    GetRunnerResults() function in services/swarming/server/test_runner.py.
  """
  number_threads = (
      min(max_threads, len(task_ids)) if max_threads else len(task_ids))
  should_stop = threading.Event()
  results_channel = threading_utils.TaskChannel()

  with threading_utils.ThreadPool(number_threads, number_threads, 0) as pool:
    try:
      # Adds a task to the thread pool to call 'retrieve_results' and return
      # the results together with shard_index that produced them (as a tuple).
      def enqueue_retrieve_results(shard_index, task_id):
        task_fn = lambda *args: (shard_index, retrieve_results(*args))
        pool.add_task(
            0, results_channel.wrap_task(task_fn), swarm_base_url, shard_index,
            task_id, timeout, should_stop, output_collector)

      # Enqueue 'retrieve_results' calls for each shard key to run in parallel.
      for shard_index, task_id in enumerate(task_ids):
        enqueue_retrieve_results(shard_index, task_id)

      # Wait for all of them to finish.
      shards_remaining = range(len(task_ids))
      active_task_count = len(task_ids)
      while active_task_count:
        shard_index, result = None, None
        try:
          shard_index, result = results_channel.pull(
              timeout=STATUS_UPDATE_INTERVAL)
        except threading_utils.TaskChannel.Timeout:
          if print_status_updates:
            print(
                'Waiting for results from the following shards: %s' %
                ', '.join(map(str, shards_remaining)))
            sys.stdout.flush()
          continue
        except Exception:
          logging.exception('Unexpected exception in retrieve_results')

        # A call to 'retrieve_results' finished (successfully or not).
        active_task_count -= 1
        if not result:
          logging.error('Failed to retrieve the results for a swarming key')
          continue

        # Yield back results to the caller.
        assert shard_index in shards_remaining
        shards_remaining.remove(shard_index)
        yield shard_index, result

    finally:
      # Done or aborted with Ctrl+C, kill the remaining threads.
      should_stop.set()


def decorate_shard_output(
    swarming, shard_index, result, shard_exit_code, shard_duration):
  """Returns wrapped output for swarming task shard."""
  url = '%s/user/task/%s' % (swarming, result['id'])
  tag_header = 'Shard %d  %s' % (shard_index, url)
  tag_footer = 'End of shard %d  Duration: %.1fs  Bot: %s  Exit code %s' % (
      shard_index, shard_duration, result['bot_id'], shard_exit_code)

  tag_len = max(len(tag_header), len(tag_footer))
  dash_pad = '+-%s-+\n' % ('-' * tag_len)
  tag_header = '| %s |\n' % tag_header.ljust(tag_len)
  tag_footer = '| %s |\n' % tag_footer.ljust(tag_len)

  header = dash_pad + tag_header + dash_pad
  footer = dash_pad + tag_footer + dash_pad[:-1]
  output = '\n'.join(o for o in result['outputs'] if o).rstrip() + '\n'
  return header + output + footer


def collect(
    swarming, task_name, task_ids, timeout, decorate, print_status_updates,
    task_summary_json, task_output_dir):
  """Retrieves results of a Swarming task."""
  # Collect summary JSON and output files (if task_output_dir is not None).
  output_collector = TaskOutputCollector(
      task_output_dir, task_name, len(task_ids))

  seen_shards = set()
  exit_code = 0
  total_duration = 0
  try:
    for index, output in yield_results(
        swarming, task_ids, timeout, None, print_status_updates,
        output_collector):
      seen_shards.add(index)

      # Grab first non-zero exit code as an overall shard exit code. Default to
      # failure if there was no process that even started.
      shard_exit_code = 1
      shard_exit_codes = sorted(output['exit_codes'], key=lambda x: not x)
      if shard_exit_codes:
        shard_exit_code = shard_exit_codes[0]
      if shard_exit_code:
        exit_code = shard_exit_code

      shard_duration = sum(i for i in output['durations'] if i)
      total_duration += shard_duration
      if decorate:
        print(decorate_shard_output(
            swarming, index, output, shard_exit_code, shard_duration))
        if len(seen_shards) < len(task_ids):
          print('')
      else:
        print('%s: %s %d' % (output['bot_id'], output['id'], shard_exit_code))
        for output in output['outputs']:
          if not output:
            continue
          output = output.rstrip()
          if output:
            print(''.join('  %s\n' % l for l in output.splitlines()))
  finally:
    summary = output_collector.finalize()
    if task_summary_json:
      tools.write_json(task_summary_json, summary, False)

  if decorate and total_duration:
    print('Total duration: %.1fs' % total_duration)

  if len(seen_shards) != len(task_ids):
    missing_shards = [x for x in range(len(task_ids)) if x not in seen_shards]
    print >> sys.stderr, ('Results from some shards are missing: %s' %
        ', '.join(map(str, missing_shards)))
    return 1

  return exit_code


### Commands.


def abort_task(_swarming, _manifest):
  """Given a task manifest that was triggered, aborts its execution."""
  # TODO(vadimsh): No supported by the server yet.


def add_filter_options(parser):
  parser.filter_group = tools.optparse.OptionGroup(parser, 'Filtering slaves')
  parser.filter_group.add_option(
      '-d', '--dimension', default=[], action='append', nargs=2,
      dest='dimensions', metavar='FOO bar',
      help='dimension to filter on')
  parser.add_option_group(parser.filter_group)


def add_sharding_options(parser):
  parser.sharding_group = tools.optparse.OptionGroup(parser, 'Sharding options')
  parser.sharding_group.add_option(
      '--shards', type='int', default=1,
      help='Number of shards to trigger and collect.')
  parser.add_option_group(parser.sharding_group)


def add_trigger_options(parser):
  """Adds all options to trigger a task on Swarming."""
  isolateserver.add_isolate_server_options(parser)
  add_filter_options(parser)

  parser.task_group = tools.optparse.OptionGroup(parser, 'Task properties')
  parser.task_group.add_option(
      '-s', '--isolated',
      help='Hash of the .isolated to grab from the isolate server')
  parser.task_group.add_option(
      '-e', '--env', default=[], action='append', nargs=2, metavar='FOO bar',
      help='Environment variables to set')
  parser.task_group.add_option(
      '--priority', type='int', default=100,
      help='The lower value, the more important the task is')
  parser.task_group.add_option(
      '-T', '--task-name',
      help='Display name of the task. Defaults to '
           '<base_name>/<dimensions>/<isolated hash>/<timestamp> if an '
           'isolated file is provided, if a hash is provided, it defaults to '
           '<user>/<dimensions>/<isolated hash>/<timestamp>')
  parser.task_group.add_option(
      '--tags', action='append', default=[],
      help='Tags to assign to the task.')
  parser.task_group.add_option(
      '--user', default='',
      help='User associated with the task. Defaults to authenticated user on '
           'the server.')
  parser.task_group.add_option(
      '--idempotent', action='store_true', default=False,
      help='When set, the server will actively try to find a previous task '
           'with the same parameter and return this result instead if possible')
  parser.task_group.add_option(
      '--expiration', type='int', default=6*60*60,
      help='Seconds to allow the task to be pending for a bot to run before '
           'this task request expires.')
  parser.task_group.add_option(
      '--deadline', type='int', dest='expiration',
      help=tools.optparse.SUPPRESS_HELP)
  parser.task_group.add_option(
      '--hard-timeout', type='int', default=60*60,
      help='Seconds to allow the task to complete.')
  parser.task_group.add_option(
      '--io-timeout', type='int', default=20*60,
      help='Seconds to allow the task to be silent.')
  parser.task_group.add_option(
      '--raw-cmd', action='store_true', default=False,
      help='When set, the command after -- is used as-is without run_isolated. '
           'In this case, no .isolated file is expected.')
  parser.add_option_group(parser.task_group)


def process_trigger_options(parser, options, args):
  """Processes trigger options and uploads files to isolate server if necessary.
  """
  options.dimensions = dict(options.dimensions)
  options.env = dict(options.env)

  data = []
  if not options.dimensions:
    parser.error('Please at least specify one --dimension')
  if options.raw_cmd:
    if not args:
      parser.error(
          'Arguments with --raw-cmd should be passed after -- as command '
          'delimiter.')
    if options.isolate_server:
      parser.error('Can\'t use both --raw-cmd and --isolate-server.')

    command = args
    if not options.task_name:
      options.task_name = u'%s/%s' % (
          options.user,
          '_'.join(
            '%s=%s' % (k, v)
            for k, v in sorted(options.dimensions.iteritems())))
  else:
    isolateserver.process_isolate_server_options(parser, options, False)
    try:
      command, data = isolated_handle_options(options, args)
    except ValueError as e:
      parser.error(str(e))

  return TaskRequest(
      command=command,
      data=data,
      dimensions=options.dimensions,
      env=options.env,
      expiration=options.expiration,
      hard_timeout=options.hard_timeout,
      idempotent=options.idempotent,
      io_timeout=options.io_timeout,
      name=options.task_name,
      priority=options.priority,
      tags=options.tags,
      user=options.user,
      verbose=options.verbose)


def add_collect_options(parser):
  parser.server_group.add_option(
      '-t', '--timeout',
      type='float',
      default=80*60.,
      help='Timeout to wait for result, set to 0 for no timeout; default: '
           '%default s')
  parser.group_logging.add_option(
      '--decorate', action='store_true', help='Decorate output')
  parser.group_logging.add_option(
      '--print-status-updates', action='store_true',
      help='Print periodic status updates')
  parser.task_output_group = tools.optparse.OptionGroup(parser, 'Task output')
  parser.task_output_group.add_option(
      '--task-summary-json',
      metavar='FILE',
      help='Dump a summary of task results to this file as json. It contains '
           'only shards statuses as know to server directly. Any output files '
           'emitted by the task can be collected by using --task-output-dir')
  parser.task_output_group.add_option(
      '--task-output-dir',
      metavar='DIR',
      help='Directory to put task results into. When the task finishes, this '
           'directory contains per-shard directory with output files produced '
           'by shards: <task-output-dir>/<zero-based-shard-index>/.')
  parser.add_option_group(parser.task_output_group)


def CMDbots(parser, args):
  """Returns information about the bots connected to the Swarming server."""
  add_filter_options(parser)
  parser.filter_group.add_option(
      '--dead-only', action='store_true',
      help='Only print dead bots, useful to reap them and reimage broken bots')
  parser.filter_group.add_option(
      '-k', '--keep-dead', action='store_true',
      help='Do not filter out dead bots')
  parser.filter_group.add_option(
      '-b', '--bare', action='store_true',
      help='Do not print out dimensions')
  options, args = parser.parse_args(args)

  if options.keep_dead and options.dead_only:
    parser.error('Use only one of --keep-dead and --dead-only')

  bots = []
  cursor = None
  limit = 250
  # Iterate via cursors.
  base_url = options.swarming + '/swarming/api/v1/client/bots?limit=%d' % limit
  while True:
    url = base_url
    if cursor:
      url += '&cursor=%s' % urllib.quote(cursor)
    data = net.url_read_json(url)
    if data is None:
      print >> sys.stderr, 'Failed to access %s' % options.swarming
      return 1
    bots.extend(data['items'])
    cursor = data['cursor']
    if not cursor:
      break

  for bot in natsort.natsorted(bots, key=lambda x: x['id']):
    if options.dead_only:
      if not bot['is_dead']:
        continue
    elif not options.keep_dead and bot['is_dead']:
      continue

    # If the user requested to filter on dimensions, ensure the bot has all the
    # dimensions requested.
    dimensions = bot['dimensions']
    for key, value in options.dimensions:
      if key not in dimensions:
        break
      # A bot can have multiple value for a key, for example,
      # {'os': ['Windows', 'Windows-6.1']}, so that --dimension os=Windows will
      # be accepted.
      if isinstance(dimensions[key], list):
        if value not in dimensions[key]:
          break
      else:
        if value != dimensions[key]:
          break
    else:
      print bot['id']
      if not options.bare:
        print '  %s' % json.dumps(dimensions, sort_keys=True)
        if bot.get('task_id'):
          print '  task: %s' % bot['task_id']
  return 0


@subcommand.usage('--json file | task_id...')
def CMDcollect(parser, args):
  """Retrieves results of one or multiple Swarming task by its ID.

  The result can be in multiple part if the execution was sharded. It can
  potentially have retries.
  """
  add_collect_options(parser)
  parser.add_option(
      '-j', '--json',
      help='Load the task ids from .json as saved by trigger --dump-json')
  (options, args) = parser.parse_args(args)
  if not args and not options.json:
    parser.error('Must specify at least one task id or --json.')
  if args and options.json:
    parser.error('Only use one of task id or --json.')

  if options.json:
    with open(options.json) as f:
      tasks = sorted(
          json.load(f)['tasks'].itervalues(), key=lambda x: x['shard_index'])
      args = [t['task_id'] for t in tasks]
  else:
    valid = frozenset('0123456789abcdef')
    if any(not valid.issuperset(task_id) for task_id in args):
      parser.error('Task ids are 0-9a-f.')

  try:
    return collect(
        options.swarming,
        None,
        args,
        options.timeout,
        options.decorate,
        options.print_status_updates,
        options.task_summary_json,
        options.task_output_dir)
  except Failure:
    on_error.report(None)
    return 1


@subcommand.usage('[resource name]')
def CMDquery(parser, args):
  """Returns raw JSON information via an URL endpoint. Use 'list' to gather the
  list of valid values from the server.

  Examples:
    Printing the list of known URLs:
      swarming.py query -S https://server-url list

    Listing last 50 tasks on a specific bot named 'swarm1'
      swarming.py query -S https://server-url --limit 50 bot/swarm1/tasks
  """
  CHUNK_SIZE = 250

  parser.add_option(
      '-L', '--limit', type='int', default=200,
      help='Limit to enforce on limitless items (like number of tasks); '
           'default=%default')
  (options, args) = parser.parse_args(args)
  if len(args) != 1:
    parser.error('Must specify only one resource name.')

  base_url = options.swarming + '/swarming/api/v1/client/' + args[0]
  url = base_url
  if options.limit:
    # Check check, change if not working out.
    merge_char = '&' if '?' in url else '?'
    url += '%slimit=%d' % (merge_char, min(CHUNK_SIZE, options.limit))
  data = net.url_read_json(url)
  if data is None:
    print >> sys.stderr, 'Failed to access %s' % options.swarming
    return 1

  # Some items support cursors. Try to get automatically if cursors are needed
  # by looking at the 'cursor' items.
  while (
      data.get('cursor') and
      (not options.limit or len(data['items']) < options.limit)):
    url = base_url + '?cursor=%s' % urllib.quote(data['cursor'])
    if options.limit:
      url += '&limit=%d' % min(CHUNK_SIZE, options.limit - len(data['items']))
    new = net.url_read_json(url)
    if new is None:
      print >> sys.stderr, 'Failed to access %s' % options.swarming
      return 1
    data['items'].extend(new['items'])
    data['cursor'] = new['cursor']

  if options.limit and len(data.get('items', [])) > options.limit:
    data['items'] = data['items'][:options.limit]
  data.pop('cursor', None)

  json.dump(data, sys.stdout, indent=2, sort_keys=True)
  sys.stdout.write('\n')
  return 0


@subcommand.usage('(hash|isolated) [-- extra_args]')
def CMDrun(parser, args):
  """Triggers a task and wait for the results.

  Basically, does everything to run a command remotely.
  """
  add_trigger_options(parser)
  add_collect_options(parser)
  add_sharding_options(parser)
  options, args = parser.parse_args(args)
  task_request = process_trigger_options(parser, options, args)
  try:
    tasks = trigger_task_shards(
        options.swarming, task_request, options.shards)
  except Failure as e:
    on_error.report(
        'Failed to trigger %s(%s): %s' %
        (options.task_name, args[0], e.args[0]))
    return 1
  if not tasks:
    on_error.report('Failed to trigger the task.')
    return 1
  print('Triggered task: %s' % options.task_name)
  task_ids = [
    t['task_id']
    for t in sorted(tasks.itervalues(), key=lambda x: x['shard_index'])
  ]
  try:
    return collect(
        options.swarming,
        options.task_name,
        task_ids,
        options.timeout,
        options.decorate,
        options.print_status_updates,
        options.task_summary_json,
        options.task_output_dir)
  except Failure:
    on_error.report(None)
    return 1


@subcommand.usage('task_id')
def CMDreproduce(parser, args):
  """Runs a task locally that was triggered on the server.

  This running locally the same commands that have been run on the bot. The data
  downloaded will be in a subdirectory named 'work' of the current working
  directory.
  """
  options, args = parser.parse_args(args)
  if len(args) != 1:
    parser.error('Must specify exactly one task id.')

  url = options.swarming + '/swarming/api/v1/client/task/%s/request' % args[0]
  request = net.url_read_json(url)
  if not request:
    print >> sys.stderr, 'Failed to retrieve request data for the task'
    return 1

  if not os.path.isdir('work'):
    os.mkdir('work')

  swarming_host = urlparse.urlparse(options.swarming).netloc
  properties = request['properties']
  for data_url, _ in properties['data']:
    assert data_url.startswith('https://'), data_url
    data_host = urlparse.urlparse(data_url).netloc
    if data_host != swarming_host:
      auth.ensure_logged_in('https://' + data_host)

    content = net.url_read(data_url)
    if content is None:
      print >> sys.stderr, 'Failed to download %s' % data_url
      return 1
    with zipfile.ZipFile(StringIO.StringIO(content)) as zip_file:
      zip_file.extractall('work')

  env = None
  if properties['env']:
    env = os.environ.copy()
    logging.info('env: %r', properties['env'])
    env.update(
        (k.encode('utf-8'), v.encode('utf-8'))
        for k, v in properties['env'].iteritems())

  exit_code = 0
  for cmd in properties['commands']:
    try:
      c = subprocess.call(cmd, env=env, cwd='work')
    except OSError as e:
      print >> sys.stderr, 'Failed to run: %s' % ' '.join(cmd)
      print >> sys.stderr, str(e)
      c = 1
    if not exit_code:
      exit_code = c
  return exit_code


@subcommand.usage("(hash|isolated) [-- extra_args|raw command]")
def CMDtrigger(parser, args):
  """Triggers a Swarming task.

  Accepts either the hash (sha1) of a .isolated file already uploaded or the
  path to an .isolated file to archive.

  If an .isolated file is specified instead of an hash, it is first archived.

  Passes all extra arguments provided after '--' as additional command line
  arguments for an isolated command specified in *.isolate file.
  """
  add_trigger_options(parser)
  add_sharding_options(parser)
  parser.add_option(
      '--dump-json',
      metavar='FILE',
      help='Dump details about the triggered task(s) to this file as json')
  options, args = parser.parse_args(args)
  task_request = process_trigger_options(parser, options, args)
  try:
    tasks = trigger_task_shards(
        options.swarming, task_request, options.shards)
    if tasks:
      print('Triggered task: %s' % options.task_name)
      tasks_sorted = sorted(
          tasks.itervalues(), key=lambda x: x['shard_index'])
      if options.dump_json:
        data = {
          'base_task_name': options.task_name,
          'tasks': tasks,
        }
        tools.write_json(options.dump_json, data, True)
        print('To collect results, use:')
        print('  swarming.py collect -S %s --json %s' %
            (options.swarming, options.dump_json))
      else:
        print('To collect results, use:')
        print('  swarming.py collect -S %s %s' %
            (options.swarming, ' '.join(t['task_id'] for t in tasks_sorted)))
      print('Or visit:')
      for t in tasks_sorted:
        print('  ' + t['view_url'])
    return int(not tasks)
  except Failure:
    on_error.report(None)
    return 1


class OptionParserSwarming(tools.OptionParserWithLogging):
  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(
        self, prog='swarming.py', **kwargs)
    self.server_group = tools.optparse.OptionGroup(self, 'Server')
    self.server_group.add_option(
        '-S', '--swarming',
        metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
        help='Swarming server to use')
    self.add_option_group(self.server_group)
    auth.add_auth_options(self)

  def parse_args(self, *args, **kwargs):
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    auth.process_auth_options(self, options)
    user = self._process_swarming(options)
    if hasattr(options, 'user') and not options.user:
      options.user = user
    return options, args

  def _process_swarming(self, options):
    """Processes the --swarming option and aborts if not specified.

    Returns the identity as determined by the server.
    """
    if not options.swarming:
      self.error('--swarming is required.')
    try:
      options.swarming = net.fix_url(options.swarming)
    except ValueError as e:
      self.error('--swarming %s' % e)
    on_error.report_on_exception_exit(options.swarming)
    try:
      user = auth.ensure_logged_in(options.swarming)
    except ValueError as e:
      self.error(str(e))
    return user


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserSwarming(version=__version__), args)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
