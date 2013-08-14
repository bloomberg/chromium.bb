#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Triggers a Swarm request based off of a .isolated file.

This script takes a .isolated file, packages it, and sends a Swarm manifest file
to the Swarm server.  This is expected to be called as a build step with the cwd
as the parent of the src/ directory.
"""

import hashlib
import json
import optparse
import os
import StringIO
import sys
import time
import urllib
import zipfile

import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_PATH = os.path.join(ROOT_DIR, 'tools')


PLATFORM_MAPPING = {
  'cygwin': 'Windows',
  'darwin': 'Mac',
  'linux2': 'Linux',
  'win32': 'Windows',
}


class Failure(Exception):
  pass


class Manifest(object):
  def __init__(
      self, manifest_hash, test_name, shards, test_filter, os_image,
      working_dir, data_server, verbose, profile, priority):
    """Populates a manifest object.
      Args:
        manifest_hash - The manifest's sha-1 that the slave is going to fetch.
        test_name - The name to give the test request.
        shards - The number of swarm shards to request.
        test_filter - The gtest filter to apply when running the test.
        os_image - OS to run on.
        working_dir - Relative working directory to start the script.
        data_server - isolate server url.
        verbose - if True, have the slave print more details.
        profile - if True, have the slave print more timing data.
        priority - int between 0 and 1000, lower the higher priority
    """
    self.manifest_hash = manifest_hash
    self._test_name = test_name
    self._shards = shards
    self._test_filter = test_filter
    self._target_platform = PLATFORM_MAPPING[os_image]
    self._working_dir = working_dir

    base_url = data_server.rstrip('/')
    self.data_server_retrieval = base_url + '/content/retrieve/default/'
    self._data_server_storage = base_url + '/content/store/default/'
    self._data_server_has = base_url + '/content/contains/default'
    self._data_server_get_token = base_url + '/content/get_token'

    self.verbose = bool(verbose)
    self.profile = bool(profile)
    self.priority = priority

    self._zip_file_hash = ''
    self._tasks = []
    self._files = {}
    self._token_cache = None

  def _token(self):
    if not self._token_cache:
      result = run_isolated.url_open(self._data_server_get_token)
      if not result:
        # TODO(maruel): Implement authentication.
        raise Failure('Failed to get token, need authentication')
      # Quote it right away, so creating the urls is simpler.
      self._token_cache = urllib.quote(result.read())
    return self._token_cache

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

  def add_file(self, source_path, rel_path):
    self._files[source_path] = rel_path

  def zip_and_upload(self):
    """Zips up all the files necessary to run a shard and uploads to Swarming
    master.
    """
    assert not self._zip_file_hash
    start_time = time.time()

    zip_memory_file = StringIO.StringIO()
    zip_file = zipfile.ZipFile(zip_memory_file, 'w')

    for source, relpath in self._files.iteritems():
      zip_file.write(source, relpath)

    zip_file.close()
    print 'Zipping completed, time elapsed: %f' % (time.time() - start_time)

    zip_memory_file.flush()
    zip_contents = zip_memory_file.getvalue()
    zip_memory_file.close()

    self._zip_file_hash = hashlib.sha1(zip_contents).hexdigest()

    response = run_isolated.url_open(
        self._data_server_has + '?token=%s' % self._token(),
        data=self._zip_file_hash,
        content_type='application/octet-stream')
    if response is None:
      print >> sys.stderr, (
          'Unable to query server for zip file presence, aborting.')
      return False

    if response.read(1) == chr(1):
      print 'Zip file already on server, no need to reupload.'
      return True

    print 'Zip file not on server, starting uploading.'

    url = '%s%s?priority=0&token=%s' % (
        self._data_server_storage, self._zip_file_hash, self._token())
    response = run_isolated.url_open(
        url, data=zip_contents, content_type='application/octet-stream')
    if response is None:
      print >> sys.stderr, 'Failed to upload the zip file: %s' % url
      return False

    return True

  def to_json(self):
    """Exports the current configuration into a swarm-readable manifest file.

    This function doesn't mutate the object.
    """
    test_case = {
      'test_case_name': self._test_name,
      'data': [
        [self.data_server_retrieval + urllib.quote(self._zip_file_hash),
         'swarm_data.zip'],
      ],
      'tests': self._tasks,
      'env_vars': {},
      'configurations': [
        {
          'min_instances': self._shards,
          'config_name': self._target_platform,
          'dimensions': {
            'os': self._target_platform,
          },
        },
      ],
      'working_dir': self._working_dir,
      'restart_on_failure': True,
      'cleanup': 'root',
      'priority': self.priority,
    }

    # These flags are googletest specific.
    if self._test_filter and self._test_filter != '*':
      test_case['env_vars']['GTEST_FILTER'] = self._test_filter
    if self._shards > 1:
      test_case['env_vars']['GTEST_SHARD_INDEX'] = '%(instance_index)s'
      test_case['env_vars']['GTEST_TOTAL_SHARDS'] = '%(num_instances)s'

    return json.dumps(test_case, separators=(',',':'))


def chromium_setup(manifest):
  """Sets up the commands to run.

  Highly chromium specific.
  """
  cleanup_script_name = 'swarm_cleanup.py'
  cleanup_script_path = os.path.join(TOOLS_PATH, cleanup_script_name)
  run_test_name = 'run_isolated.py'
  run_test_path = os.path.join(ROOT_DIR, run_test_name)

  manifest.add_file(run_test_path, run_test_name)
  manifest.add_file(cleanup_script_path, cleanup_script_name)
  run_cmd = [
    'python', run_test_name,
    '--hash', manifest.manifest_hash,
    '--remote', manifest.data_server_retrieval.rstrip('/') + '-gzip/',
  ]
  if manifest.verbose or manifest.profile:
    # Have it print the profiling section.
    run_cmd.append('--verbose')
  manifest.add_task('Run Test', run_cmd)

  # Clean up
  manifest.add_task('Clean Up', ['python', cleanup_script_name])


def process_manifest(
    file_sha1, test_name, shards, test_filter, os_image, working_dir,
    data_server, swarm_url, verbose, profile, priority):
  """Process the manifest file and send off the swarm test request."""
  try:
    manifest = Manifest(
        file_sha1, test_name, shards, test_filter, os_image, working_dir,
        data_server, verbose, profile, priority)
  except ValueError as e:
    print >> sys.stderr, 'Unable to process %s: %s' % (test_name, e)
    return 1

  chromium_setup(manifest)

  # Zip up relevent files
  print "Zipping up files..."
  if not manifest.zip_and_upload():
    return 1

  # Send test requests off to swarm.
  print('Sending test requests to swarm.')
  print('Server: %s' % swarm_url)
  print('Job name: %s' % test_name)
  test_url = swarm_url.rstrip('/') + '/test'
  manifest_text = manifest.to_json()
  result = run_isolated.url_open(test_url, data={'request': manifest_text})
  if not result:
    print >> sys.stderr, 'Failed to send test for %s\n%s' % (
        test_name, test_url)
    return 1
  try:
    json.load(result)
  except (ValueError, TypeError) as e:
    print >> sys.stderr, 'Failed to send test for %s' % test_name
    print >> sys.stderr, 'Manifest: %s' % manifest_text
    print >> sys.stderr, str(e)
    return 1
  return 0


def main(argv):
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog [options]', description=sys.modules[__name__].__doc__)
  parser.add_option('-w', '--working_dir', default='swarm_tests',
                    help='Desired working direction on the swarm slave side. '
                    'Defaults to %default.')
  parser.add_option('-o', '--os_image',
                    help='Swarm OS image to request.')
  parser.add_option('-u', '--swarm-url', default='http://localhost:8080',
                    help='Specify the url of the Swarm server. '
                    'Defaults to %default')
  parser.add_option('-d', '--data-server',
                    help='The server where all the test data is stored.')
  parser.add_option('-t', '--test-name-prefix', default='',
                    help='Specify the prefix to give the swarm test request. '
                    'Defaults to %default')
  parser.add_option('--run_from_hash', nargs=4, action='append', default=[],
                    help='Specify a hash to run on swarm. The format is '
                    '(hash, hash_test_name, shards, test_filter). This may be '
                    'used multiple times to send multiple hashes.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Print verbose logging')
  parser.add_option('--profile', action='store_true',
                    default=bool(os.environ.get('ISOLATE_DEBUG')),
                    help='Have run_isolated.py print profiling info')
  parser.add_option('--priority', type='int', default=100,
                    help='The lower value, the more important the task is')
  (options, args) = parser.parse_args(argv)

  if args:
    parser.error('Unknown args: %s' % args)

  if not options.os_image or options.os_image == 'None':
    # This means the Try Server/user wants to use the current OS.
    options.os_image = sys.platform
  if not options.data_server:
    parser.error('Must specify the data directory')

  if not options.run_from_hash:
    parser.error('At least one --run_from_hash is required.')

  highest_exit_code = 0
  try:
    # Send off the hash swarm test requests.
    for (file_sha1, test_name, shards, testfilter) in options.run_from_hash:
      exit_code = process_manifest(
          file_sha1,
          options.test_name_prefix + test_name,
          int(shards),
          testfilter,
          options.os_image,
          options.working_dir,
          options.data_server,
          options.swarm_url,
          options.verbose,
          options.profile,
          options.priority)
      highest_exit_code = max(highest_exit_code, exit_code)
  except Failure as e:
    print >> sys.stderr, e.args[0]
    highest_exit_code = max(1, highest_exit_code)
  return highest_exit_code


if __name__ == '__main__':
  sys.exit(main(None))
