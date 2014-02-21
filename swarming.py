#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to trigger tasks or retrieve results from a Swarming server."""

__version__ = '0.4.1'

import getpass
import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import time
import urllib

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from utils import net
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


NO_OUTPUT_FOUND = (
  'No output produced by the task, it may have failed to run.\n'
  '\n')


class Failure(Exception):
  """Generic failure."""
  pass


class Manifest(object):
  """Represents a Swarming task manifest.

  Also includes code to zip code and upload itself.
  """
  def __init__(
      self, isolate_server, namespace, isolated_hash, task_name, shards, env,
      dimensions, working_dir, verbose, profile, priority, algo):
    """Populates a manifest object.
      Args:
        isolate_server - isolate server url.
        namespace - isolate server namespace to use.
        isolated_hash - The manifest's sha-1 that the slave is going to fetch.
        task_name - The name to give the task request.
        shards - The number of swarming shards to request.
        env - environment variables to set.
        dimensions - dimensions to filter the task on.
        working_dir - Relative working directory to start the script.
        verbose - if True, have the slave print more details.
        profile - if True, have the slave print more timing data.
        priority - int between 0 and 1000, lower the higher priority.
        algo - hashing algorithm used.
    """
    self.isolate_server = isolate_server
    self.namespace = namespace
    # The reason is that swarm_bot doesn't understand compressed data yet. So
    # the data to be downloaded by swarm_bot is in 'default', independent of
    # what run_isolated.py is going to fetch.
    self.storage = isolateserver.get_storage(isolate_server, 'default')

    self.isolated_hash = isolated_hash
    self.bundle = zip_package.ZipPackage(ROOT_DIR)

    self._task_name = task_name
    self._shards = shards
    self._env = env.copy()
    self._dimensions = dimensions.copy()
    self._working_dir = working_dir

    self.verbose = bool(verbose)
    self.profile = bool(profile)
    self.priority = priority
    self._algo = algo

    self._isolate_item = None
    self._tasks = []

  def add_task(self, task_name, actions, time_out=600):
    """Appends a new task as a TestObject to the swarming manifest file.

    Tasks cannot be added once the manifest was uploaded.

    See TestObject in services/swarming/src/common/test_request_message.py for
    the valid format.
    """
    assert not self._isolate_item
    self._tasks.append(
        {
          'action': actions,
          'decorate_output': self.verbose,
          'test_name': task_name,
          'time_out': time_out,
        })

  def to_json(self):
    """Exports the current configuration into a swarm-readable manifest file.

    The actual serialization format is defined as a TestCase object as described
    in services/swarming/src/common/test_request_message.py

    This function doesn't mutate the object.
    """
    request = {
      'cleanup': 'root',
      'configurations': [
        # Is a TestConfiguration.
        {
          'config_name': 'isolated',
          'dimensions': self._dimensions,
          'min_instances': self._shards,
          'priority': self.priority,
        },
      ],
      'data': [],
      # TODO: Let the encoding get set from the command line.
      'encoding': 'UTF-8',
      'env_vars': self._env,
      'restart_on_failure': True,
      'test_case_name': self._task_name,
      'tests': self._tasks,
      'working_dir': self._working_dir,
    }
    if self._isolate_item:
      request['data'].append(
          [
            self.storage.get_fetch_url(self._isolate_item.digest),
            'swarm_data.zip',
          ])
    return json.dumps(request, sort_keys=True, separators=(',',':'))

  @property
  def isolate_item(self):
    """Calling this property 'closes' the manifest and it can't be modified
    afterward.
    """
    if self._isolate_item is None:
      self._isolate_item = isolateserver.BufferItem(
          self.bundle.zip_into_buffer(), self._algo, is_isolated=True)
    return self._isolate_item


def zip_and_upload(manifest):
  """Zips up all the files necessary to run a manifest and uploads to Swarming
  master.
  """
  try:
    start_time = time.time()
    with manifest.storage:
      uploaded = manifest.storage.upload_items([manifest.isolate_item])
    elapsed = time.time() - start_time
  except (IOError, OSError) as exc:
    tools.report_error('Failed to upload the zip file: %s' % exc)
    return False

  if manifest.isolate_item in uploaded:
    logging.info('Upload complete, time elapsed: %f', elapsed)
  else:
    logging.info('Zip file already on server, time elapsed: %f', elapsed)
  return True


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


def retrieve_results(base_url, task_key, timeout, should_stop):
  """Retrieves results for a single task_key."""
  assert isinstance(timeout, float), timeout
  params = [('r', task_key)]
  result_url = '%s/get_result?%s' % (base_url, urllib.urlencode(params))
  start = now()
  while True:
    if timeout and (now() - start) >= timeout:
      logging.error('retrieve_results(%s) timed out', base_url)
      return {}
    # Do retries ourselves.
    response = net.url_read(result_url, retry_404=False, retry_50x=False)
    if response is None:
      # Aggressively poll for results. Do not use retry_404 so
      # should_stop is polled more often.
      remaining = min(5, timeout - (now() - start)) if timeout else 5
      if remaining > 0:
        if should_stop.get():
          return {}
        net.sleep_before_retry(1, remaining)
    else:
      try:
        data = json.loads(response) or {}
      except (ValueError, TypeError):
        logging.warning(
            'Received corrupted data for task_key %s. Retrying.', task_key)
      else:
        if data['output']:
          return data
    if should_stop.get():
      return {}


def yield_results(swarm_base_url, task_keys, timeout, max_threads):
  """Yields swarming task results from the swarming server as (index, result).

  Duplicate shards are ignored, the first one to complete is returned.

  max_threads is optional and is used to limit the number of parallel fetches
  done. Since in general the number of task_keys is in the range <=10, it's not
  worth normally to limit the number threads. Mostly used for testing purposes.

  Yields:
    (index, result). In particular, 'result' is defined as the
    GetRunnerResults() function in services/swarming/server/test_runner.py.
  """
  shards_remaining = range(len(task_keys))
  number_threads = (
      min(max_threads, len(task_keys)) if max_threads else len(task_keys))
  should_stop = threading_utils.Bit()
  results_remaining = len(task_keys)
  with threading_utils.ThreadPool(number_threads, number_threads, 0) as pool:
    try:
      for task_key in task_keys:
        pool.add_task(
            0, retrieve_results, swarm_base_url, task_key, timeout, should_stop)
      while shards_remaining and results_remaining:
        result = pool.get_one_result()
        results_remaining -= 1
        if not result:
          # Failed to retrieve one key.
          logging.error('Failed to retrieve the results for a swarming key')
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


def chromium_setup(manifest):
  """Sets up the commands to run.

  Highly chromium specific.
  """
  # Add uncompressed zip here. It'll be compressed as part of the package sent
  # to Swarming server.
  run_test_name = 'run_isolated.zip'
  manifest.bundle.add_buffer(run_test_name,
    run_isolated.get_as_zip_package().zip_into_buffer(compress=False))

  cleanup_script_name = 'swarm_cleanup.py'
  manifest.bundle.add_file(os.path.join(TOOLS_PATH, cleanup_script_name),
    cleanup_script_name)

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
  manifest.add_task('Run Test', run_cmd)

  # Clean up
  manifest.add_task('Clean Up', ['python', cleanup_script_name])


def googletest_setup(env, shards):
  """Sets googletest specific environment variables."""
  if shards > 1:
    env = env.copy()
    env['GTEST_SHARD_INDEX'] = '%(instance_index)s'
    env['GTEST_TOTAL_SHARDS'] = '%(num_instances)s'
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


def process_manifest(
    swarming, isolate_server, namespace, isolated_hash, task_name, shards,
    dimensions, env, working_dir, verbose, profile, priority, algo):
  """Processes the manifest file and send off the swarming task request."""
  try:
    manifest = Manifest(
        isolate_server=isolate_server,
        namespace=namespace,
        isolated_hash=isolated_hash,
        task_name=task_name,
        shards=shards,
        dimensions=dimensions,
        env=env,
        working_dir=working_dir,
        verbose=verbose,
        profile=profile,
        priority=priority,
        algo=algo)
  except ValueError as e:
    tools.report_error('Unable to process %s: %s' % (task_name, e))
    return 1

  chromium_setup(manifest)

  logging.info('Zipping up files...')
  if not zip_and_upload(manifest):
    return 1

  logging.info('Server: %s', swarming)
  logging.info('Task name: %s', task_name)
  trigger_url = swarming + '/test'
  manifest_text = manifest.to_json()
  result = net.url_read(trigger_url, data={'request': manifest_text})
  if not result:
    tools.report_error(
        'Failed to trigger task %s\n%s' % (task_name, trigger_url))
    return 1
  try:
    json.loads(result)
  except (ValueError, TypeError) as e:
    msg = '\n'.join((
        'Failed to trigger task %s' % task_name,
        'Manifest: %s' % manifest_text,
        'Bad response: %s' % result,
        str(e)))
    tools.report_error(msg)
    return 1
  return 0


def isolated_to_hash(isolate_server, namespace, arg, algo, verbose):
  """Archives a .isolated file if needed.

  Returns the file hash to trigger and a bool specifying if it was a file (True)
  or a hash (False).
  """
  if arg.endswith('.isolated'):
    file_hash = archive(isolate_server, namespace, arg, algo, verbose)
    if not file_hash:
      tools.report_error('Archival failure %s' % arg)
      return None, True
    return file_hash, True
  elif isolateserver.is_valid_hash(arg, algo):
    return arg, False
  else:
    tools.report_error('Invalid hash %s' % arg)
    return None, False


def trigger(
    swarming,
    isolate_server,
    namespace,
    file_hash_or_isolated,
    task_name,
    shards,
    dimensions,
    env,
    working_dir,
    verbose,
    profile,
    priority):
  """Sends off the hash swarming task requests."""
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
    task_name = '%s/%s/%s' % (
        key,
        '_'.join('%s=%s' % (k, v) for k, v in sorted(dimensions.iteritems())),
        file_hash)

  env = googletest_setup(env, shards)
  # TODO(maruel): It should first create a request manifest object, then pass
  # it to a function to zip, archive and trigger.
  result = process_manifest(
      swarming=swarming,
      isolate_server=isolate_server,
      namespace=namespace,
      isolated_hash=file_hash,
      task_name=task_name,
      shards=shards,
      dimensions=dimensions,
      env=env,
      working_dir=working_dir,
      verbose=verbose,
      profile=profile,
      priority=priority,
      algo=hashlib.sha1)
  return result, task_name


def decorate_shard_output(result, shard_exit_code):
  """Returns wrapped output for swarming task shard."""
  tag = 'index %s (machine tag: %s, id: %s)' % (
      result['config_instance_index'],
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
    'End output from shard %s. Return %d\n'
    '================================================================\n'
    ) % (tag, result['output'] or NO_OUTPUT_FOUND, tag, shard_exit_code)


def collect(url, task_name, timeout, decorate):
  """Retrieves results of a Swarming task."""
  logging.info('Collecting %s', task_name)
  task_keys = get_task_keys(url, task_name)
  if not task_keys:
    raise Failure('No task keys to get results with.')

  exit_code = None
  for _index, output in yield_results(url, task_keys, timeout, None):
    shard_exit_codes = (output['exit_codes'] or '1').split(',')
    shard_exit_code = max(int(i) for i in shard_exit_codes)
    if decorate:
      print decorate_shard_output(output, shard_exit_code)
    else:
      print(
          '%s/%s: %s' % (
              output['machine_id'],
              output['machine_tag'],
              output['exit_codes']))
      print(''.join('  %s\n' % l for l in output['output'].splitlines()))
    exit_code = exit_code or shard_exit_code
  return exit_code if exit_code is not None else 1


def add_trigger_options(parser):
  """Adds all options to trigger a task on Swarming."""
  isolateserver.add_isolate_server_options(parser, True)

  parser.filter_group = tools.optparse.OptionGroup(parser, 'Filtering slaves')
  parser.filter_group.add_option(
      '-d', '--dimension', default=[], action='append', nargs=2,
      dest='dimensions', metavar='FOO bar',
      help='dimension to filter on')
  parser.add_option_group(parser.filter_group)

  parser.task_group = tools.optparse.OptionGroup(parser, 'Task properties')
  parser.task_group.add_option(
      '-w', '--working-dir', default='swarm_tests',
      help='Working directory on the swarming slave side. default: %default.')
  parser.task_group.add_option(
      '--working_dir', help=tools.optparse.SUPPRESS_HELP)
  parser.task_group.add_option(
      '-e', '--env', default=[], action='append', nargs=2, metavar='FOO bar',
      help='environment variables to set')
  parser.task_group.add_option(
      '--priority', type='int', default=100,
      help='The lower value, the more important the task is')
  parser.task_group.add_option(
      '--shards', type='int', default=1, help='number of shards to use')
  parser.task_group.add_option(
      '-T', '--task-name',
      help='Display name of the task. It uniquely identifies the task. '
           'Defaults to <base_name>/<dimensions>/<isolated hash> if an '
           'isolated file is provided, if a hash is provided, it defaults to '
           '<user>/<dimensions>/<isolated hash>')
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
  options.dimensions = dict(options.dimensions)
  if not options.dimensions.get('os'):
    parser.error(
        'Please at least specify the dimension of the swarming bot OS with '
        '--dimension os <something>.')


def add_collect_options(parser):
  parser.server_group.add_option(
      '-t', '--timeout',
      type='float',
      default=DEFAULT_SHARD_WAIT_TIME,
      help='Timeout to wait for result, set to 0 for no timeout; default: '
           '%default s')
  parser.group_logging.add_option(
      '--decorate', action='store_true', help='Decorate output')


@subcommand.usage('task_name')
def CMDcollect(parser, args):
  """Retrieves results of a Swarming task.

  The result can be in multiple part if the execution was sharded. It can
  potentially have retries.
  """
  add_collect_options(parser)
  (options, args) = parser.parse_args(args)
  if not args:
    parser.error('Must specify one task name.')
  elif len(args) > 1:
    parser.error('Must specify only one task name.')

  try:
    return collect(options.swarming, args[0], options.timeout, options.decorate)
  except Failure as e:
    tools.report_error(e)
    return 1


@subcommand.usage('[hash|isolated]')
def CMDrun(parser, args):
  """Triggers a task and wait for the results.

  Basically, does everything to run a command remotely.
  """
  add_trigger_options(parser)
  add_collect_options(parser)
  options, args = parser.parse_args(args)
  process_trigger_options(parser, options, args)

  try:
    result, task_name = trigger(
        swarming=options.swarming,
        isolate_server=options.isolate_server or options.indir,
        namespace=options.namespace,
        file_hash_or_isolated=args[0],
        task_name=options.task_name,
        shards=options.shards,
        dimensions=options.dimensions,
        env=dict(options.env),
        working_dir=options.working_dir,
        verbose=options.verbose,
        profile=options.profile,
        priority=options.priority)
  except Failure as e:
    tools.report_error(
        'Failed to trigger %s(%s): %s' %
        (options.task_name, args[0], e.args[0]))
    return 1
  if result:
    tools.report_error('Failed to trigger the task.')
    return result
  if task_name != options.task_name:
    print('Triggered task: %s' % task_name)
  try:
    return collect(
        options.swarming,
        task_name,
        options.timeout,
        options.decorate)
  except Failure as e:
    tools.report_error(e)
    return 1


@subcommand.usage("(hash|isolated)")
def CMDtrigger(parser, args):
  """Triggers a Swarming task.

  Accepts either the hash (sha1) of a .isolated file already uploaded or the
  path to an .isolated file to archive, packages it if needed and sends a
  Swarming manifest file to the Swarming server.

  If an .isolated file is specified instead of an hash, it is first archived.
  """
  add_trigger_options(parser)
  options, args = parser.parse_args(args)
  process_trigger_options(parser, options, args)

  try:
    result, task_name = trigger(
        swarming=options.swarming,
        isolate_server=options.isolate_server or options.indir,
        namespace=options.namespace,
        file_hash_or_isolated=args[0],
        task_name=options.task_name,
        dimensions=options.dimensions,
        shards=options.shards,
        env=dict(options.env),
        working_dir=options.working_dir,
        verbose=options.verbose,
        profile=options.profile,
        priority=options.priority)
    if task_name != options.task_name and not result:
      print('Triggered task: %s' % task_name)
    return result
  except Failure as e:
    tools.report_error(e)
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
  try:
    return dispatcher.execute(OptionParserSwarming(version=__version__), args)
  except Exception as e:
    tools.report_error(e)
    return 1


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
