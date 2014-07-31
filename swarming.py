#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to trigger tasks or retrieve results from a Swarming server."""

__version__ = '0.4.13'

import getpass
import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib

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
import isolateserver
import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_PATH = os.path.join(ROOT_DIR, 'tools')


# The default time to wait for a shard to finish running.
DEFAULT_SHARD_WAIT_TIME = 80 * 60.

# How often to print status updates to stdout in 'collect'.
STATUS_UPDATE_INTERVAL = 15 * 60.


NO_OUTPUT_FOUND = (
  'No output produced by the task, it may have failed to run.\n'
  '\n')


class Failure(Exception):
  """Generic failure."""
  pass


class Manifest(object):
  """Represents a Swarming task manifest."""

  def __init__(
      self, isolate_server, namespace, isolated_hash, task_name, extra_args,
      env, dimensions, deadline, verbose, profile,
      priority):
    """Populates a manifest object.
      Args:
        isolate_server - isolate server url.
        namespace - isolate server namespace to use.
        isolated_hash - the manifest's sha-1 that the slave is going to fetch.
        task_name - the name to give the task request.
        extra_args - additional arguments to pass to isolated command.
        env - environment variables to set.
        dimensions - dimensions to filter the task on.
        deadline - maximum pending time before this task expires.
        verbose - if True, have the slave print more details.
        profile - if True, have the slave print more timing data.
        priority - int between 0 and 1000, lower the higher priority.
    """
    self.isolate_server = isolate_server
    self.namespace = namespace
    self.isolated_hash = isolated_hash
    self.task_name = task_name
    self.extra_args = tuple(extra_args or [])
    self.env = env.copy()
    self.dimensions = dimensions.copy()
    self.deadline = deadline
    self.verbose = bool(verbose)
    self.profile = bool(profile)
    self.priority = priority
    self._tasks = []
    self._files = []

  def add_task(self, task_name, actions, time_out=2*60*60):
    """Appends a new task as a TestObject to the swarming manifest file.

    Tasks cannot be added once the manifest was uploaded.

    By default, command will be killed after 2 hours of execution.

    See TestObject in services/swarming/src/common/test_request_message.py for
    the valid format.
    """
    self._tasks.append(
        {
          'action': actions,
          'decorate_output': self.verbose,
          'test_name': task_name,
          'hard_time_out': time_out,
        })

  def add_bundled_file(self, file_name, file_url):
    """Appends a file to the manifest.

    File will be downloaded and extracted by the swarm bot before launching the
    task.
    """
    self._files.append([file_url, file_name])

  def to_json(self):
    """Exports the current configuration into a swarm-readable manifest file.

    The actual serialization format is defined as a TestCase object as described
    in services/swarming/src/common/test_request_message.py
    """
    request = {
      'cleanup': 'root',
      'configurations': [
        # Is a TestConfiguration.
        {
          'config_name': 'isolated',
          'deadline_to_run': self.deadline,
          'dimensions': self.dimensions,
          'priority': self.priority,
        },
      ],
      'data': self._files,
      'env_vars': self.env,
      'test_case_name': self.task_name,
      'tests': self._tasks,
    }
    return json.dumps(request, sort_keys=True, separators=(',',':'))


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

    Called concurrently from multiple threads.
    """
    # Sanity check index is in expected range.
    assert isinstance(shard_index, int)
    if shard_index < 0 or shard_index >= self.shard_count:
      logging.warning(
          'Shard index %d is outside of expected range: [0; %d]',
          shard_index, self.shard_count - 1)
      return

    # Store result dict of that shard, ignore results we've already seen.
    with self._lock:
      if shard_index in self._per_shard_results:
        logging.warning('Ignoring duplicate shard index %d', shard_index)
        return
      self._per_shard_results[shard_index] = result

    # Fetch output files if necessary.
    if self.task_output_dir:
      isolated_files_location = extract_output_files_location(result['output'])
      if isolated_files_location:
        isolate_server, namespace, isolated_hash = isolated_files_location
        storage = self._get_storage(isolate_server, namespace)
        if storage:
          # Output files are supposed to be small and they are not reused across
          # tasks. So use MemoryCache for them instead of on-disk cache. Make
          # files writable, so that calling script can delete them.
          isolateserver.fetch_isolated(
              isolated_hash,
              storage,
              isolateserver.MemoryCache(file_mode_mask=0700),
              os.path.join(self.task_output_dir, str(shard_index)),
              False)

  def finalize(self):
    """Assembles and returns task summary JSON, shutdowns underlying Storage."""
    with self._lock:
      # Write an array of shard results with None for missing shards.
      summary = {
        'task_name': self.task_name,
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


def now():
  """Exists so it can be mocked easily."""
  return time.time()


def get_task_keys(swarm_base_url, task_name):
  """Returns the Swarming task key for each shards of task_name."""
  key_data = urllib.urlencode([('name', task_name)])
  url = '%s/get_matching_test_cases?%s' % (swarm_base_url, key_data)

  for _ in net.retry_loop(max_attempts=net.URL_OPEN_MAX_ATTEMPTS):
    result = net.url_read(url, retry_404=True)
    if result is None:
      raise Failure(
          'Error: Unable to find any task with the name, %s, on swarming server'
          % task_name)

    # TODO(maruel): Compare exact string.
    if 'No matching' in result:
      logging.warning('Unable to find any task with the name, %s, on swarming '
                      'server' % task_name)
      continue
    return json.loads(result)

  raise Failure(
      'Error: Unable to find any task with the name, %s, on swarming server'
      % task_name)


def extract_output_files_location(task_log):
  """Task log -> location of task output files to fetch.

  TODO(vadimsh,maruel): Use side-channel to get this information.
  See 'run_tha_test' in run_isolated.py for where the data is generated.

  Returns:
    Tuple (isolate server URL, namespace, isolated hash) on success.
    None if information is missing or can not be parsed.
  """
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
    return (isolate_server, namespace, isolated_hash)
  except (KeyError, ValueError):
    logging.warning(
        'Unexpected value of run_isolated_out_hack: %s', match.group(1))
    return None


def retrieve_results(
    base_url, shard_index, task_key, timeout, should_stop, output_collector):
  """Retrieves results for a single task_key.

  Returns:
    <result dict> on success.
    None on failure.
  """
  assert isinstance(timeout, float), timeout
  params = [('r', task_key)]
  result_url = '%s/get_result?%s' % (base_url, urllib.urlencode(params))
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

    # Disable internal retries in net.url_read, since we are doing retries
    # ourselves. Do not use retry_404 so should_stop is polled more often.
    response = net.url_read(result_url, retry_404=False, retry_50x=False)

    # Request failed. Try again.
    if response is None:
      continue

    # Got some response, ensure it is JSON dict, retry if not.
    try:
      result = json.loads(response) or {}
      if not isinstance(result, dict):
        raise ValueError()
    except (ValueError, TypeError):
      logging.warning(
          'Received corrupted or invalid data for task_key %s, retrying: %r',
          task_key, response)
      continue

    # Swarming server uses non-empty 'output' value as a flag that task has
    # finished. How to wait for tasks that produce no output is a mystery.
    if result.get('output'):
      # Record the result, try to fetch attached output files (if any).
      if output_collector:
        # TODO(vadimsh): Respect |should_stop| and |deadline| when fetching.
        output_collector.process_shard_result(shard_index, result)
      return result


def yield_results(
    swarm_base_url, task_keys, timeout, max_threads,
    print_status_updates, output_collector):
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
      min(max_threads, len(task_keys)) if max_threads else len(task_keys))
  should_stop = threading.Event()
  results_channel = threading_utils.TaskChannel()

  with threading_utils.ThreadPool(number_threads, number_threads, 0) as pool:
    try:
      # Adds a task to the thread pool to call 'retrieve_results' and return
      # the results together with shard_index that produced them (as a tuple).
      def enqueue_retrieve_results(shard_index, task_key):
        task_fn = lambda *args: (shard_index, retrieve_results(*args))
        pool.add_task(
            0, results_channel.wrap_task(task_fn),
            swarm_base_url, shard_index, task_key, timeout,
            should_stop, output_collector)

      # Enqueue 'retrieve_results' calls for each shard key to run in parallel.
      for shard_index, task_key in enumerate(task_keys):
        enqueue_retrieve_results(shard_index, task_key)

      # Wait for all of them to finish.
      shards_remaining = range(len(task_keys))
      active_task_count = len(task_keys)
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


def setup_run_isolated(manifest, bundle):
  """Sets up the manifest to run an isolated task via run_isolated.py.

  Modifies |bundle| (by adding files) and |manifest| (by adding commands) in
  place.

  Args:
    manifest: Manifest with swarm task definition.
    bundle: ZipPackage with files that would be transfered to swarm bot.
        If None, only |manifest| is modified (useful in tests).
  """
  # Add uncompressed zip here. It'll be compressed as part of the package sent
  # to Swarming server.
  run_test_name = 'run_isolated.zip'
  if bundle and run_test_name not in bundle.files:
    bundle.add_buffer(
        run_test_name,
        run_isolated.get_as_zip_package().zip_into_buffer(compress=False))

  cleanup_script_name = 'swarm_cleanup.py'
  if bundle and cleanup_script_name not in bundle.files:
    bundle.add_file(
        os.path.join(TOOLS_PATH, cleanup_script_name), cleanup_script_name)

  run_cmd = [
    'python', run_test_name,
    '--hash', manifest.isolated_hash,
    '--namespace', manifest.namespace,
  ]
  if file_path.is_url(manifest.isolate_server):
    run_cmd.extend(('--isolate-server', manifest.isolate_server))
  else:
    run_cmd.extend(('--indir', manifest.isolate_server))

  if manifest.verbose or manifest.profile:
    # Have it print the profiling section.
    run_cmd.append('--verbose')

  # Pass all extra args for run_isolated.py, it will pass them to the command.
  if manifest.extra_args:
    run_cmd.append('--')
    run_cmd.extend(manifest.extra_args)

  manifest.add_task('Run Test', run_cmd)

  # Clean up
  manifest.add_task('Clean Up', ['python', cleanup_script_name])


def setup_googletest(env, shards, index):
  """Sets googletest specific environment variables."""
  if shards > 1:
    env = env.copy()
    env['GTEST_SHARD_INDEX'] = str(index)
    env['GTEST_TOTAL_SHARDS'] = str(shards)
  return env


def archive(isolate_server, namespace, isolated, algo, verbose):
  """Archives a .isolated and all the dependencies on the CAC."""
  logging.info('archive(%s, %s, %s)', isolate_server, namespace, isolated)
  tempdir = None
  if file_path.is_url(isolate_server):
    command = 'archive'
    flag = '--isolate-server'
  else:
    command = 'hashtable'
    flag = '--outdir'

  print('Archiving: %s' % isolated)
  try:
    cmd = [
      sys.executable,
      os.path.join(ROOT_DIR, 'isolate.py'),
      command,
      flag, isolate_server,
      '--namespace', namespace,
      '--isolated', isolated,
    ]
    cmd.extend(['--verbose'] * verbose)
    logging.info(' '.join(cmd))
    if subprocess.call(cmd, verbose):
      return
    return isolateserver.hash_file(isolated, algo)
  finally:
    if tempdir:
      shutil.rmtree(tempdir)


def get_shard_task_name(task_name, shards, index):
  """Returns a task name to use for a single shard of a task."""
  if shards == 1:
    return task_name
  return '%s:%s:%s' % (task_name, shards, index)


def upload_zip_bundle(isolate_server, bundle):
  """Uploads a zip package to isolate storage and returns raw fetch URL.

  Args:
    isolate_server: URL of an isolate server.
    bundle: instance of ZipPackage to upload.

  Returns:
    URL to get the file from.
  """
  # Swarming bot would need to be able to grab the file from the storage
  # using raw HTTP GET. Use 'default' namespace so that the raw data returned
  # to a bot is not zipped, since swarm_bot doesn't understand compressed
  # data yet. This namespace have nothing to do with |namespace| passed to
  # run_isolated.py that is used to store files for isolated task.
  logging.info('Zipping up and uploading files...')
  start_time = now()
  isolate_item = isolateserver.BufferItem(
      bundle.zip_into_buffer(), high_priority=True)
  with isolateserver.get_storage(isolate_server, 'default') as storage:
    uploaded = storage.upload_items([isolate_item])
    bundle_url = storage.get_fetch_url(isolate_item)
  elapsed = now() - start_time
  if isolate_item in uploaded:
    logging.info('Upload complete, time elapsed: %f', elapsed)
  else:
    logging.info('Zip file already on server, time elapsed: %f', elapsed)
  return bundle_url


def trigger_by_manifest(swarming, manifest):
  """Given a task manifest, triggers it for execution on swarming.

  Args:
    swarming: URL of a swarming service.
    manifest: instance of Manifest.

  Returns:
    tuple(Task id, priority) on success. tuple(None, None) on failure.
  """
  logging.info('Triggering: %s', manifest.task_name)
  manifest_text = manifest.to_json()
  result = net.url_read(swarming + '/test', data={'request': manifest_text})
  if not result:
    on_error.report('Failed to trigger task %s' % manifest.task_name)
    return None, None
  try:
    data = json.loads(result)
  except (ValueError, TypeError):
    msg = '\n'.join((
        'Failed to trigger task %s' % manifest.task_name,
        'Manifest: %s' % manifest_text,
        'Bad response: %s' % result))
    on_error.report(msg)
    return None, None
  if not data:
    return None, None
  return data['test_keys'][0]['test_key'], data['priority']


def abort_task(_swarming, _manifest):
  """Given a task manifest that was triggered, aborts its execution."""
  # TODO(vadimsh): No supported by the server yet.


def trigger_task_shards(
    swarming, isolate_server, namespace, isolated_hash, task_name, extra_args,
    shards, dimensions, env, deadline, verbose, profile, priority):
  """Triggers multiple subtasks of a sharded task.

  Returns:
    Dict with task details, returned to caller as part of --dump-json output.
    None in case of failure.
  """
  # Collects all files that are necessary to bootstrap a task execution
  # on the bot. Usually it includes self contained run_isolated.zip and
  # a bunch of small other scripts. All heavy files are pulled
  # by run_isolated.zip. Updated in 'setup_run_isolated'.
  bundle = zip_package.ZipPackage(ROOT_DIR)

  # Make a separate Manifest for each shard, put shard index and number of
  # shards into env and subtask name.
  manifests = []
  for index in xrange(shards):
    manifest = Manifest(
        isolate_server=isolate_server,
        namespace=namespace,
        isolated_hash=isolated_hash,
        task_name=get_shard_task_name(task_name, shards, index),
        extra_args=extra_args,
        dimensions=dimensions,
        env=setup_googletest(env, shards, index),
        deadline=deadline,
        verbose=verbose,
        profile=profile,
        priority=priority)
    setup_run_isolated(manifest, bundle)
    manifests.append(manifest)

  # Upload zip bundle file to get its URL.
  try:
    bundle_url = upload_zip_bundle(isolate_server, bundle)
  except (IOError, OSError):
    on_error.report('Failed to upload the zip file for task %s' % task_name)
    return None, None

  # Attach that file to all manifests.
  for manifest in manifests:
    manifest.add_bundled_file('swarm_data.zip', bundle_url)

  # Trigger all the subtasks.
  tasks = {}
  priority_warning = False
  for index, manifest in enumerate(manifests):
    task_id, priority = trigger_by_manifest(swarming, manifest)
    if not task_id:
      break
    if not priority_warning and priority != manifest.priority:
      priority_warning = True
      print >> sys.stderr, 'Priority was reset to %s' % priority
    tasks[manifest.task_name] = {
      'shard_index': index,
      'task_id': task_id,
      'view_url': '%s/user/task/%s' % (swarming, task_id),
    }

  # Some shards weren't triggered. Abort everything.
  if len(tasks) != len(manifests):
    if tasks:
      print >> sys.stderr, 'Not all shards were triggered'
      for task_dict in tasks.itervalues():
        abort_task(swarming, task_dict['task_id'])
    return None

  return tasks


def isolated_to_hash(isolate_server, namespace, arg, algo, verbose):
  """Archives a .isolated file if needed.

  Returns the file hash to trigger and a bool specifying if it was a file (True)
  or a hash (False).
  """
  if arg.endswith('.isolated'):
    file_hash = archive(isolate_server, namespace, arg, algo, verbose)
    if not file_hash:
      on_error.report('Archival failure %s' % arg)
      return None, True
    return file_hash, True
  elif isolateserver.is_valid_hash(arg, algo):
    return arg, False
  else:
    on_error.report('Invalid hash %s' % arg)
    return None, False


def trigger(
    swarming,
    isolate_server,
    namespace,
    file_hash_or_isolated,
    task_name,
    extra_args,
    shards,
    dimensions,
    env,
    deadline,
    verbose,
    profile,
    priority):
  """Sends off the hash swarming task requests.

  Returns:
    tuple(dict(task_name: task_id), base task name). The dict of tasks is None
    in case of failure.
  """
  file_hash, is_file = isolated_to_hash(
      isolate_server, namespace, file_hash_or_isolated, hashlib.sha1, verbose)
  if not file_hash:
    return 1, ''
  if not task_name:
    # If a file name was passed, use its base name of the isolated hash.
    # Otherwise, use user name as an approximation of a task name.
    if is_file:
      key = os.path.splitext(os.path.basename(file_hash_or_isolated))[0]
    else:
      key = getpass.getuser()
    task_name = '%s/%s/%s/%d' % (
        key,
        '_'.join('%s=%s' % (k, v) for k, v in sorted(dimensions.iteritems())),
        file_hash,
        now() * 1000)

  tasks = trigger_task_shards(
      swarming=swarming,
      isolate_server=isolate_server,
      namespace=namespace,
      isolated_hash=file_hash,
      task_name=task_name,
      extra_args=extra_args,
      shards=shards,
      dimensions=dimensions,
      deadline=deadline,
      env=env,
      verbose=verbose,
      profile=profile,
      priority=priority)
  return tasks, task_name


def decorate_shard_output(shard_index, result, shard_exit_code):
  """Returns wrapped output for swarming task shard."""
  tag = 'index %s (machine tag: %s, id: %s)' % (
      shard_index,
      result['machine_id'],
      result.get('machine_tag', 'unknown'))
  return (
    '\n'
    '================================================================\n'
    'Begin output from shard %s\n'
    '================================================================\n'
    '\n'
    '%s'
    '================================================================\n'
    'End output from shard %s.\nExit code %d (%s).\n'
    '================================================================\n') % (
        tag, result['output'] or NO_OUTPUT_FOUND, tag,
        shard_exit_code, hex(0xffffffff & shard_exit_code))


def collect(
    url, task_name, shards, timeout, decorate,
    print_status_updates, task_summary_json, task_output_dir):
  """Retrieves results of a Swarming task."""
  # Grab task keys for each shard. Order is important, used to figure out
  # shard index based on the key.
  # TODO(vadimsh): Simplify this once server support is added.
  task_keys = []
  for index in xrange(shards):
    shard_task_name = get_shard_task_name(task_name, shards, index)
    logging.info('Collecting %s', shard_task_name)
    shard_task_keys = get_task_keys(url, shard_task_name)
    if not shard_task_keys:
      raise Failure('No task keys to get results with: %s' % shard_task_name)
    if len(shard_task_keys) != 1:
      raise Failure('Expecting only one shard for a task: %s' % shard_task_name)
    task_keys.append(shard_task_keys[0])

  # Collect summary JSON and output files (if task_output_dir is not None).
  output_collector = TaskOutputCollector(
      task_output_dir, task_name, len(task_keys))

  seen_shards = set()
  exit_codes = []

  try:
    for index, output in yield_results(
        url, task_keys, timeout, None, print_status_updates, output_collector):
      seen_shards.add(index)

      # Grab first non-zero exit code as an overall shard exit code.
      shard_exit_code = 0
      for code in map(int, (output['exit_codes'] or '1').split(',')):
        if code:
          shard_exit_code = code
          break
      exit_codes.append(shard_exit_code)

      if decorate:
        print decorate_shard_output(index, output, shard_exit_code)
      else:
        print(
            '%s/%s: %s' % (
                output['machine_id'],
                output['machine_tag'],
                output['exit_codes']))
        print(''.join('  %s\n' % l for l in output['output'].splitlines()))
  finally:
    summary = output_collector.finalize()
    if task_summary_json:
      tools.write_json(task_summary_json, summary, False)

  if len(seen_shards) != len(task_keys):
    missing_shards = [x for x in range(len(task_keys)) if x not in seen_shards]
    print >> sys.stderr, ('Results from some shards are missing: %s' %
        ', '.join(map(str, missing_shards)))
    return 1

  return int(bool(any(exit_codes)))


def add_filter_options(parser):
  parser.filter_group = tools.optparse.OptionGroup(parser, 'Filtering slaves')
  parser.filter_group.add_option(
      '-d', '--dimension', default=[], action='append', nargs=2,
      dest='dimensions', metavar='FOO bar',
      help='dimension to filter on')
  parser.add_option_group(parser.filter_group)


def process_filter_options(parser, options):
  options.dimensions = dict(options.dimensions)
  if not options.dimensions:
    parser.error('Please at least specify one --dimension')


def add_sharding_options(parser):
  parser.sharding_group = tools.optparse.OptionGroup(parser, 'Sharding options')
  parser.sharding_group.add_option(
      '--shards', type='int', default=1,
      help='Number of shards to trigger and collect.')
  parser.add_option_group(parser.sharding_group)


def add_trigger_options(parser):
  """Adds all options to trigger a task on Swarming."""
  isolateserver.add_isolate_server_options(parser, True)
  add_filter_options(parser)

  parser.task_group = tools.optparse.OptionGroup(parser, 'Task properties')
  parser.task_group.add_option(
      '-e', '--env', default=[], action='append', nargs=2, metavar='FOO bar',
      help='Environment variables to set')
  parser.task_group.add_option(
      '--priority', type='int', default=100,
      help='The lower value, the more important the task is')
  parser.task_group.add_option(
      '-T', '--task-name',
      help='Display name of the task. It uniquely identifies the task. '
           'Defaults to <base_name>/<dimensions>/<isolated hash>/<timestamp> '
           'if an isolated file is provided, if a hash is provided, it '
           'defaults to <user>/<dimensions>/<isolated hash>/<timestamp>')
  parser.task_group.add_option(
      '--deadline', type='int', default=6*60*60,
      help='Seconds to allow the task to be pending for a bot to run before '
           'this task request expires.')
  parser.add_option_group(parser.task_group)
  # TODO(maruel): This is currently written in a chromium-specific way.
  parser.group_logging.add_option(
      '--profile', action='store_true',
      default=bool(os.environ.get('ISOLATE_DEBUG')),
      help='Have run_isolated.py print profiling info')


def process_trigger_options(parser, options, args):
  isolateserver.process_isolate_server_options(parser, options)
  if len(args) != 1:
    parser.error('Must pass one .isolated file or its hash (sha1).')
  process_filter_options(parser, options)


def add_collect_options(parser):
  parser.server_group.add_option(
      '-t', '--timeout',
      type='float',
      default=DEFAULT_SHARD_WAIT_TIME,
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


def extract_isolated_command_extra_args(args):
  try:
    index = args.index('--')
  except ValueError:
    return (args, [])
  return (args[:index], args[index+1:])


@subcommand.usage('task_name')
def CMDcollect(parser, args):
  """Retrieves results of a Swarming task.

  The result can be in multiple part if the execution was sharded. It can
  potentially have retries.
  """
  add_collect_options(parser)
  add_sharding_options(parser)
  (options, args) = parser.parse_args(args)
  if not args:
    parser.error('Must specify one task name.')
  elif len(args) > 1:
    parser.error('Must specify only one task name.')

  auth.ensure_logged_in(options.swarming)
  try:
    return collect(
        options.swarming,
        args[0],
        options.shards,
        options.timeout,
        options.decorate,
        options.print_status_updates,
        options.task_summary_json,
        options.task_output_dir)
  except Failure:
    on_error.report(None)
    return 1


def CMDquery(parser, args):
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

  auth.ensure_logged_in(options.swarming)
  service = net.get_http_service(options.swarming)

  data = service.json_request('GET', '/swarming/api/v1/bots')
  if data is None:
    print >> sys.stderr, 'Failed to access %s' % options.swarming
    return 1
  for machine in natsort.natsorted(data['machines'], key=lambda x: x['id']):
    if options.dead_only:
      if not machine['is_dead']:
        continue
    elif not options.keep_dead and machine['is_dead']:
      continue

    # If the user requested to filter on dimensions, ensure the bot has all the
    # dimensions requested.
    dimensions = machine['dimensions']
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
      print machine['id']
      if not options.bare:
        print '  %s' % dimensions
  return 0


@subcommand.usage('(hash|isolated) [-- extra_args]')
def CMDrun(parser, args):
  """Triggers a task and wait for the results.

  Basically, does everything to run a command remotely.
  """
  add_trigger_options(parser)
  add_collect_options(parser)
  add_sharding_options(parser)
  args, isolated_cmd_args = extract_isolated_command_extra_args(args)
  options, args = parser.parse_args(args)
  process_trigger_options(parser, options, args)

  auth.ensure_logged_in(options.swarming)
  if file_path.is_url(options.isolate_server):
    auth.ensure_logged_in(options.isolate_server)
  try:
    tasks, task_name = trigger(
        swarming=options.swarming,
        isolate_server=options.isolate_server or options.indir,
        namespace=options.namespace,
        file_hash_or_isolated=args[0],
        task_name=options.task_name,
        extra_args=isolated_cmd_args,
        shards=options.shards,
        dimensions=options.dimensions,
        env=dict(options.env),
        deadline=options.deadline,
        verbose=options.verbose,
        profile=options.profile,
        priority=options.priority)
  except Failure as e:
    on_error.report(
        'Failed to trigger %s(%s): %s' %
        (options.task_name, args[0], e.args[0]))
    return 1
  if not tasks:
    on_error.report('Failed to trigger the task.')
    return 1
  if task_name != options.task_name:
    print('Triggered task: %s' % task_name)
  try:
    # TODO(maruel): Use task_ids, it's much more efficient!
    return collect(
        options.swarming,
        task_name,
        options.shards,
        options.timeout,
        options.decorate,
        options.print_status_updates,
        options.task_summary_json,
        options.task_output_dir)
  except Failure:
    on_error.report(None)
    return 1


@subcommand.usage("(hash|isolated) [-- extra_args]")
def CMDtrigger(parser, args):
  """Triggers a Swarming task.

  Accepts either the hash (sha1) of a .isolated file already uploaded or the
  path to an .isolated file to archive, packages it if needed and sends a
  Swarming manifest file to the Swarming server.

  If an .isolated file is specified instead of an hash, it is first archived.

  Passes all extra arguments provided after '--' as additional command line
  arguments for an isolated command specified in *.isolate file.
  """
  add_trigger_options(parser)
  add_sharding_options(parser)
  args, isolated_cmd_args = extract_isolated_command_extra_args(args)
  parser.add_option(
      '--dump-json',
      metavar='FILE',
      help='Dump details about the triggered task(s) to this file as json')
  options, args = parser.parse_args(args)
  process_trigger_options(parser, options, args)

  auth.ensure_logged_in(options.swarming)
  if file_path.is_url(options.isolate_server):
    auth.ensure_logged_in(options.isolate_server)
  try:
    tasks, task_name = trigger(
        swarming=options.swarming,
        isolate_server=options.isolate_server or options.indir,
        namespace=options.namespace,
        file_hash_or_isolated=args[0],
        task_name=options.task_name,
        extra_args=isolated_cmd_args,
        shards=options.shards,
        dimensions=options.dimensions,
        env=dict(options.env),
        deadline=options.deadline,
        verbose=options.verbose,
        profile=options.profile,
        priority=options.priority)
    if tasks:
      if task_name != options.task_name:
        print('Triggered task: %s' % task_name)
      if options.dump_json:
        data = {
          'base_task_name': task_name,
          'tasks': tasks,
        }
        tools.write_json(options.dump_json, data, True)
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
    options.swarming = options.swarming.rstrip('/')
    if not options.swarming:
      self.error('--swarming is required.')
    auth.process_auth_options(self, options)
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserSwarming(version=__version__), args)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
