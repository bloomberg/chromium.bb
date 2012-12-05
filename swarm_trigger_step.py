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
import socket
import StringIO
import sys
import time
import urllib
import urllib2
import zipfile


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_PATH = os.path.join(ROOT_DIR, 'tools')
BUILD_ROOT_PATH = os.path.join(ROOT_DIR, '..', '..')

# TODO(maruel): This shouldn't be necessary here.
CLEANUP_SCRIPT_NAME = 'swarm_cleanup.py'
CLEANUP_SCRIPT_PATH = os.path.join(TOOLS_PATH, CLEANUP_SCRIPT_NAME)

# TODO(maruel): This script shouldn't be necessary on swarm slave. Swarm slaves
# need to make sure they don't have zombie processes left by themselves. Figure
# out a way to make this cleaner.
WINDOWS_SCRIPT_NAME = 'kill_processes.py'
WINDOWS_SCRIPT_PATH = os.path.join(
    BUILD_ROOT_PATH, 'scripts', 'slave', WINDOWS_SCRIPT_NAME)

HANDLE_EXE = 'handle.exe'
HANDLE_EXE_PATH = os.path.join(
    BUILD_ROOT_PATH, 'third_party', 'psutils', HANDLE_EXE)

RUN_TEST_NAME = 'run_isolated.py'
RUN_TEST_PATH = os.path.join(ROOT_DIR, RUN_TEST_NAME)


class Manifest(object):
  def __init__(self, manifest_hash, test_name, shards, test_filter, switches):
    """Populates a manifest object.
      Args:
        manifest_hash - The manifest's sha-1 that the slave is going to fetch.
        test_name - The name to give the test request.
        shards - The number of swarm shards to request.
        test_filter - The gtest filter to apply when running the test.
        switches - An object with properties to apply to the test request.
    """
    platform_mapping =  {
      'darwin': 'Mac',
      'cygwin': 'Windows',
      'linux2': 'Linux',
      'win32': 'Windows'
      }

    self.manifest_hash = manifest_hash
    self.test_filter = test_filter
    self.shards = shards

    self.tasks = []
    self.target_platform = platform_mapping[switches.os_image]
    self.working_dir = switches.working_dir
    self.test_name = test_name
    base_url = switches.data_server.rstrip('/')
    self.data_server_retrieval = base_url + '/content/retrieve/default/'
    self.data_server_storage = base_url + '/content/store/default/'
    self.data_server_has = base_url + '/content/contains/default'
    self.zip_file_hash = ''

  def add_task(self, task_name, actions, time_out=600):
    """Appends a new task to the swarm manifest file."""
    self.tasks.append({
          'test_name': task_name,
          'action': actions,
          'time_out': time_out
    })

  def zip(self):
    """Zip up all the files necessary to run a shard."""
    start_time = time.time()

    zip_memory_file = StringIO.StringIO()
    zip_file = zipfile.ZipFile(zip_memory_file, 'w')

    zip_file.write(RUN_TEST_PATH, RUN_TEST_NAME)
    zip_file.write(CLEANUP_SCRIPT_PATH, CLEANUP_SCRIPT_NAME)

    if self.target_platform == 'Windows':
      # TODO(maruel): Users with src.git checkout don't have these files.
      zip_file.write(WINDOWS_SCRIPT_PATH, WINDOWS_SCRIPT_NAME)
      zip_file.write(HANDLE_EXE_PATH, HANDLE_EXE)

    zip_file.close()
    print 'Zipping completed, time elapsed: %f' % (time.time() - start_time)

    zip_memory_file.flush()
    zip_contents = zip_memory_file.getvalue()
    zip_memory_file.close()

    try:
      self.zip_file_hash = hashlib.sha1(zip_contents).hexdigest()

      response = urllib2.urlopen(self.data_server_has,
                                 data=self.zip_file_hash).read()
      if response[0] == chr(1):
        print 'Zip file already on server, no need to reupload.'
        return True
      print 'Zip file not on server, starting uploading.'

      url = self.data_server_storage + self.zip_file_hash + '?priority=0'

      request = urllib2.Request(url, data=zip_contents)
      request.add_header('Content-Type', 'application/octet-stream')
      request.add_header('Content-Length', len(zip_contents))

      urllib2.urlopen(request)
    except urllib2.URLError as e:
      print 'Failed to upload the zip file\n%s' % str(e)
      return False

    return True

  def to_json(self):
    """Export the current configuration into a swarm-readable manifest file"""
    self.add_task(
        'Run Test',
        ['python', RUN_TEST_NAME, '--hash', self.manifest_hash,
         '--remote', self.data_server_retrieval + '-gzip', '-v'])

    # Clean up
    self.add_task('Clean Up', ['python', CLEANUP_SCRIPT_NAME])

    # Call kill_processes.py if on windows
    if self.target_platform == 'Windows':
      self.add_task('Kill Processes',
          ['python', WINDOWS_SCRIPT_NAME,
           '--handle_exe', HANDLE_EXE])

    # This separation of vlans isn't required anymore, but it is
    # still a useful separation to keep.
    hostname = socket.gethostname().lower().split('.', 1)[0]
    vlan = ''
    if hostname.endswith('-m1'):
      vlan = 'm1'
    elif hostname.endswith('m4'):
      vlan = 'm4'
    else:
      vlan = 'm4'

    # Construct test case
    test_case = {
      'test_case_name': self.test_name,
      'data': [
        [self.data_server_retrieval + urllib.quote(self.zip_file_hash),
         'swarm_data.zip'],
      ],
      'tests': self.tasks,
      'env_vars': {
        'GTEST_FILTER': self.test_filter,
        'GTEST_SHARD_INDEX': '%(instance_index)s',
        'GTEST_TOTAL_SHARDS': '%(num_instances)s',
      },
      'configurations': [
        {
          'min_instances': self.shards,
          'config_name': self.target_platform,
          'dimensions': {
            'os': self.target_platform,
            'vlan': vlan
          },
        },
      ],
      'working_dir': self.working_dir,
      'restart_on_failure': True,
      'cleanup': 'root',
    }

    return json.dumps(test_case)


def ProcessManifest(file_sha1, test_name, shards, test_filter, options):
  """Process the manifest file and send off the swarm test request."""
  manifest = Manifest(file_sha1, test_name, shards, test_filter, options)

  # Zip up relevent files
  print "Zipping up files..."
  if not manifest.zip():
    return 1

  # Send test requests off to swarm.
  print 'Sending test requests to swarm'
  test_url = options.swarm_url + '/test'
  manifest_text = manifest.to_json()
  result = None
  try:
    result = urllib2.urlopen(test_url, manifest_text).read()

    # Check that we can read the output as a JSON string
    json.loads(result)
  except (ValueError, TypeError, urllib2.URLError) as e:
    print 'Failed to send test for ' + test_name
    print 'Manifest: %s' % manifest_text
    print e
    if result:
      print result
    return 1

  return 0


def main():
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
  (options, args) = parser.parse_args()

  if args:
    parser.error('Unknown args: %s' % args)

  if not options.os_image or options.os_image == 'None':
    # This means the Try Server/user wants to use the current OS.
    options.os_image = sys.platform
  if not options.data_server:
    parser.error('Must specify the data directory')

  # Send off the hash swarm test requests.
  highest_exit_code = 0
  for (file_sha1, test_name, shards, testfilter) in options.run_from_hash:
    try:
      highest_exit_code = max(highest_exit_code,
                              ProcessManifest(
                                  file_sha1,
                                  options.test_name_prefix + test_name,
                                  int(shards),
                                  testfilter,
                                  options))
    except ValueError:
      print ('Unable to process %s because integer not given for shard count' %
             test_name)

  return highest_exit_code


if __name__ == '__main__':
  sys.exit(main())
