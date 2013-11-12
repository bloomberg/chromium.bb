#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to trigger tasks or retrieve results from a Swarming server."""

__version__ = '0.2'

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

from utils import net
from utils import threading_utils
from utils import tools
from utils import zip_package

import isolateserver
import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_PATH = os.path.join(ROOT_DIR, 'tools')


# The default time to wait for a shard to finish running.
DEFAULT_SHARD_WAIT_TIME = 80 * 60.


NO_OUTPUT_FOUND = (
  'No output produced by the test, it may have failed to run.\n'
  '\n')


# TODO(maruel): cygwin != Windows. If a swarm_bot is running in cygwin, it's
# different from running in native python.
PLATFORM_MAPPING_SWARMING = {
  'cygwin': 'Windows',
  'darwin': 'Mac',
  'linux2': 'Linux',
  'win32': 'Windows',
}

PLATFORM_MAPPING_ISOLATE = {
  'linux2': 'linux',
  'darwin': 'mac',
  'win32': 'win',
}


class Failure(Exception):
  """Generic failure."""
  pass


class Manifest(object):
  """Represents a Swarming task manifest.

  Also includes code to zip code and upload itself.
  """
  def __init__(
      self, isolate_server, isolated_hash, test_name, shards, env,
      dimensions, working_dir, verbose, profile, priority, algo):
    """Populates a manifest object.
      Args:
        isolate_server - isolate server url.
        isolated_hash - The manifest's sha-1 that the slave is going to fetch.
        test_name - The name to give the test request.
        shards - The number of swarm shards to request.
        env - environment variables to set.
        dimensions - dimensions to filter the task on.
        working_dir - Relative working directory to start the script.
        verbose - if True, have the slave print more details.
        profile - if True, have the slave print more timing data.
        priority - int between 0 and 1000, lower the higher priority.
        algo - hashing algorithm used.
    """
    self.isolate_server = isolate_server
    self.storage = isolateserver.get_storage(isolate_server, 'default')

    self.isolated_hash = isolated_hash
    self.bundle = zip_package.ZipPackage(ROOT_DIR)

    self._test_name = test_name
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
    """Appends a new task to the swarm manifest file."""
    # See swarming/src/common/test_request_message.py TestObject constructor for
    # the valid flags.
    self._tasks.append(
        {
          'action': actions,
          'decorate_output': self.verbose,
          'test_name': task_name,
          'time_out': time_out,
        })

  def zip_and_upload(self):
    """Zips up all the files necessary to run a shard and uploads to Swarming
    master.
    """
    assert not self._isolate_item

    start_time = time.time()
    self._isolate_item = isolateserver.BufferItem(
        self.bundle.zip_into_buffer(), self._algo, is_isolated=True)
    print 'Zipping completed, time elapsed: %f' % (time.time() - start_time)

    try:
      start_time = time.time()
      uploaded = self.storage.upload_items([self._isolate_item])
      elapsed = time.time() - start_time
    except (IOError, OSError) as exc:
      tools.report_error('Failed to upload the zip file: %s' % exc)
      return False

    if self._isolate_item in uploaded:
      print 'Upload complete, time elapsed: %f' % elapsed
    else:
      print 'Zip file already on server, time elapsed: %f' % elapsed

    return True

  def to_json(self):
    """Exports the current configuration into a swarm-readable manifest file.

    This function doesn't mutate the object.
    """
    test_case = {
      'cleanup': 'root',
      'configurations': [
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
      'test_case_name': self._test_name,
      'tests': self._tasks,
      'working_dir': self._working_dir,
    }
    if self._isolate_item:
      test_case['data'].append(
          [
            self.storage.get_fetch_url(self._isolate_item.digest),
            'swarm_data.zip',
          ])
    return json.dumps(test_case, separators=(',',':'))


def now():
  """Exists so it can be mocked easily."""
  return time.time()


def get_test_keys(swarm_base_url, test_name):
  """Returns the Swarm test key for each shards of test_name."""
  key_data = urllib.urlencode([('name', test_name)])
  url = '%s/get_matching_test_cases?%s' % (swarm_base_url, key_data)

  for _ in net.retry_loop(max_attempts=net.URL_OPEN_MAX_ATTEMPTS):
    result = net.url_read(url, retry_404=True)
    if result is None:
      raise Failure(
          'Error: Unable to find any tests with the name, %s, on swarm server'
          % test_name)

    # TODO(maruel): Compare exact string.
    if 'No matching' in result:
      logging.warning('Unable to find any tests with the name, %s, on swarm '
                      'server' % test_name)
      continue
    return json.loads(result)

  raise Failure(
      'Error: Unable to find any tests with the name, %s, on swarm server'
      % test_name)


def retrieve_results(base_url, test_key, timeout, should_stop):
  """Retrieves results for a single test_key."""
  assert isinstance(timeout, float), timeout
  params = [('r', test_key)]
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
  should_stop = threading_utils.Bit()
  results_remaining = len(test_keys)
  with threading_utils.ThreadPool(number_threads, number_threads, 0) as pool:
    try:
      for test_key in test_keys:
        pool.add_task(
            0, retrieve_results, swarm_base_url, test_key, timeout, should_stop)
      while shards_remaining and results_remaining:
        result = pool.get_one_result()
        results_remaining -= 1
        if not result:
          # Failed to retrieve one key.
          logging.error('Failed to retrieve the results for a swarm key')
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
    '--isolate-server', manifest.isolate_server,
  ]
  if manifest.verbose or manifest.profile:
    # Have it print the profiling section.
    run_cmd.append('--verbose')
  manifest.add_task('Run Test', run_cmd)

  # Clean up
  manifest.add_task('Clean Up', ['python', cleanup_script_name])


def archive(isolated, isolate_server, algo, verbose):
  """Archives a .isolated and all the dependencies on the CAC."""
  tempdir = None
  try:
    logging.info('archive(%s, %s)', isolated, isolate_server)
    cmd = [
      sys.executable,
      os.path.join(ROOT_DIR, 'isolate.py'),
      'archive',
      '--outdir', isolate_server,
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
    swarming, isolate_server, file_hash_or_isolated, test_name, shards,
    dimensions, env, working_dir, verbose, profile, priority, algo):
  """Processes the manifest file and send off the swarming test request.

  Optionally archives an .isolated file.
  """
  if file_hash_or_isolated.endswith('.isolated'):
    file_hash = archive(
        file_hash_or_isolated, isolate_server, algo, verbose)
    if not file_hash:
      tools.report_error('Archival failure %s' % file_hash_or_isolated)
      return 1
  elif isolateserver.is_valid_hash(file_hash_or_isolated, algo):
    file_hash = file_hash_or_isolated
  else:
    tools.report_error('Invalid hash %s' % file_hash_or_isolated)
    return 1
  try:
    manifest = Manifest(
        isolate_server=isolate_server,
        isolated_hash=file_hash,
        test_name=test_name,
        shards=shards,
        dimensions=dimensions,
        env=env,
        working_dir=working_dir,
        verbose=verbose,
        profile=profile,
        priority=priority,
        algo=algo)
  except ValueError as e:
    tools.report_error('Unable to process %s: %s' % (test_name, e))
    return 1

  chromium_setup(manifest)

  # Zip up relevant files.
  print('Zipping up files...')
  if not manifest.zip_and_upload():
    return 1

  # Send test requests off to swarm.
  print('Sending test requests to swarm.')
  print('Server: %s' % swarming)
  print('Job name: %s' % test_name)
  test_url = swarming + '/test'
  manifest_text = manifest.to_json()
  result = net.url_read(test_url, data={'request': manifest_text})
  if not result:
    tools.report_error(
        'Failed to send test for %s\n%s' % (test_name, test_url))
    return 1
  try:
    json.loads(result)
  except (ValueError, TypeError) as e:
    msg = '\n'.join((
        'Failed to send test for %s' % test_name,
        'Manifest: %s' % manifest_text,
        'Bad response: %s' % result,
        str(e)))
    tools.report_error(msg)
    return 1
  return 0


def trigger(
    swarming,
    isolate_server,
    tasks,
    task_prefix,
    working_dir,
    dimensions,
    env,
    verbose,
    profile,
    priority):
  """Sends off the hash swarming test requests."""
  highest_exit_code = 0
  for (file_hash, test_name, shards, test_filter) in tasks:
    task_env = env.copy()
    # These flags are googletest specific.
    if test_filter and test_filter != '*':
      task_env['GTEST_FILTER'] = test_filter
    if int(shards) > 1:
      task_env['GTEST_SHARD_INDEX'] = '%(instance_index)s'
      task_env['GTEST_TOTAL_SHARDS'] = '%(num_instances)s'
    # TODO(maruel): It should first create a request manifest object, then pass
    # it to a function to zip, archive and trigger.
    exit_code = process_manifest(
        swarming=swarming,
        isolate_server=isolate_server,
        file_hash_or_isolated=file_hash,
        test_name=task_prefix + test_name,
        shards=int(shards),
        dimensions=dimensions,
        env=task_env,
        working_dir=working_dir,
        verbose=verbose,
        profile=profile,
        priority=priority,
        algo=hashlib.sha1)
    highest_exit_code = max(highest_exit_code, exit_code)
  return highest_exit_code


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


def collect(url, test_name, timeout, decorate):
  """Retrieves results of a Swarming job."""
  test_keys = get_test_keys(url, test_name)
  if not test_keys:
    raise Failure('No test keys to get results with.')

  exit_code = None
  for _index, output in yield_results(url, test_keys, timeout, None):
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
  parser.server_group.add_option(
      '-I', '--isolate-server',
      metavar='URL', default='',
      help='Isolate server to use')

  parser.filter_group = tools.optparse.OptionGroup(parser, 'Filtering slaves')
  parser.filter_group.add_option(
      '-o', '--os', default=sys.platform,
      help='Swarm OS image to request. Should be one of the valid sys.platform '
           'values like darwin, linux2 or win32 default: %default.')
  parser.filter_group.add_option(
      '-d', '--dimensions', default=[], action='append', nargs=2,
      dest='dimensions', metavar='FOO bar',
      help='dimension to filter on')
  parser.add_option_group(parser.filter_group)

  parser.task_group = tools.optparse.OptionGroup(parser, 'Task properties')
  parser.task_group.add_option(
      '-w', '--working_dir', default='swarm_tests',
      help='Working directory on the swarm slave side. default: %default.')
  parser.task_group.add_option(
      '-e', '--env', default=[], action='append', nargs=2, metavar='FOO bar',
      help='environment variables to set')
  parser.task_group.add_option(
      '-T', '--task-prefix', default='',
      help='Prefix to give the swarm test request. default: %default')
  parser.task_group.add_option(
      '--priority', type='int', default=100,
      help='The lower value, the more important the task is')
  parser.add_option_group(parser.task_group)

  parser.group_logging.add_option(
      '--profile', action='store_true',
      default=bool(os.environ.get('ISOLATE_DEBUG')),
      help='Have run_isolated.py print profiling info')


def process_trigger_options(parser, options):
  options.isolate_server = options.isolate_server.rstrip('/')
  if not options.isolate_server:
    parser.error('--isolate-server is required.')
  if options.os in ('', 'None'):
    # Use the current OS.
    options.os = sys.platform
  if not options.os in PLATFORM_MAPPING_SWARMING:
    parser.error('Invalid --os option.')


def add_collect_options(parser):
  parser.server_group.add_option(
      '-t', '--timeout',
      type='float',
      default=DEFAULT_SHARD_WAIT_TIME,
      help='Timeout to wait for result, set to 0 for no timeout; default: '
           '%default s')
  parser.group_logging.add_option(
      '--decorate', action='store_true', help='Decorate output')


@subcommand.usage('test_name')
def CMDcollect(parser, args):
  """Retrieves results of a Swarming job.

  The result can be in multiple part if the execution was sharded. It can
  potentially have retries.
  """
  add_collect_options(parser)
  (options, args) = parser.parse_args(args)
  if not args:
    parser.error('Must specify one test name.')
  elif len(args) > 1:
    parser.error('Must specify only one test name.')

  try:
    return collect(options.swarming, args[0], options.timeout, options.decorate)
  except Failure as e:
    tools.report_error(e)
    return 1


@subcommand.usage('[hash|isolated ...]')
def CMDrun(parser, args):
  """Triggers a job and wait for the results.

  Basically, does everything to run command(s) remotely.
  """
  add_trigger_options(parser)
  add_collect_options(parser)
  options, args = parser.parse_args(args)

  if not args:
    parser.error('Must pass at least one .isolated file or its hash (sha1).')
  process_trigger_options(parser, options)

  success = []
  for arg in args:
    logging.info('Triggering %s', arg)
    try:
      dimensions = {
        'os': PLATFORM_MAPPING_SWARMING[options.os],
      }
      dimensions.update(dict(options.dimensions))
      result = trigger(
          swarming=options.swarming,
          isolate_server=options.isolate_server,
          tasks=[(arg, os.path.basename(arg), '1', '')],
          dimensions=dimensions,
          env=dict(options.env),
          task_prefix=options.task_prefix,
          working_dir=options.working_dir,
          verbose=options.verbose,
          profile=options.profile,
          priority=options.priority)
    except Failure as e:
      result = e.args[0]
    if result:
      tools.report_error('Failed to trigger %s: %s' % (arg, result))
    else:
      success.append(os.path.basename(arg))

  if not success:
    tools.report_error('Failed to trigger any job.')
    return result

  code = 0
  for arg in success:
    logging.info('Collecting %s', arg)
    try:
      new_code = collect(
          options.swarming,
          options.task_prefix + arg,
          options.timeout,
          options.decorate)
      code = max(code, new_code)
    except Failure as e:
      code = max(code, 1)
      tools.report_error(e)
  return code


def CMDtrigger(parser, args):
  """Triggers Swarm request(s).

  Accepts one or multiple --task requests, with either the hash (sha1) of a
  .isolated file already uploaded or the path to an .isolated file to archive,
  packages it if needed and sends a Swarm manifest file to the Swarm server.
  """
  add_trigger_options(parser)
  parser.task_group.add_option(
      '--task', nargs=4, action='append', default=[], dest='tasks',
      help='Task to trigger. The format is '
           '(hash|isolated, test_name, shards, test_filter). This may be '
           'used multiple times to send multiple hashes jobs. If an isolated '
           'file is specified instead of an hash, it is first archived.')
  (options, args) = parser.parse_args(args)

  if args:
    parser.error('Unknown args: %s' % args)
  process_trigger_options(parser, options)
  if not options.tasks:
    parser.error('At least one --task is required.')
  dimensions = {
    'os': PLATFORM_MAPPING_SWARMING[options.os],
  }
  dimensions.update(dict(options.dimensions))

  try:
    return trigger(
        swarming=options.swarming,
        isolate_server=options.isolate_server,
        tasks=options.tasks,
        task_prefix=options.task_prefix,
        dimensions=dimensions,
        env=dict(options.env),
        working_dir=options.working_dir,
        verbose=options.verbose,
        profile=options.profile,
        priority=options.priority)
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
        metavar='URL', default='',
        help='Swarming server to use')
    self.add_option_group(self.server_group)

  def parse_args(self, *args, **kwargs):
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    options.swarming = options.swarming.rstrip('/')
    if not options.swarming:
      self.error('--swarming is required.')
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
