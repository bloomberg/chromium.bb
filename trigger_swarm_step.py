#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# run_slavelastic.py: Runs a test based off of a slavelastic manifest file.

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


DESCRIPTION = """This script takes a slavelastic manifest file, packages it,
and sends a swarm manifest file to the swarm server.  This is expected to be
called as a build step with the cwd as the parent of the src/ directory.
"""

CLEANUP_SCRIPT_NAME = 'swarm_cleanup.py'
CLEANUP_SCRIPT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   CLEANUP_SCRIPT_NAME)

WINDOWS_SCRIPT_NAME = 'kill_processes.py'
WINDOWS_SCRIPT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   WINDOWS_SCRIPT_NAME)

HANDLE_EXE = 'handle.exe'
HANDLE_EXE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               '..', '..', 'third_party', 'psutils',
                               HANDLE_EXE)

RUN_TEST_NAME = 'run_test_from_archive.py'
RUN_TEST_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             RUN_TEST_NAME)


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
    self.data_server_retrieval = (
        switches.data_server.rstrip('/') + '/content/retrieve?hash_key=')
    self.data_server_storage = (
        switches.data_server.rstrip('/') + '/content/store')
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
      zip_file.write(WINDOWS_SCRIPT_PATH, WINDOWS_SCRIPT_NAME)
      zip_file.write(HANDLE_EXE_PATH, HANDLE_EXE)

    zip_file.close()
    print 'Zipping completed, time elapsed: %f' % (time.time() - start_time)

    zip_memory_file.flush()
    zip_contents = zip_memory_file.getvalue()
    zip_memory_file.close()

    try:
      self.zip_file_hash = hashlib.sha1(zip_contents).hexdigest()
      url = (self.data_server_storage + '?' +
             urllib.urlencode({'hash_key': self.zip_file_hash}))

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
         '--remote', self.data_server_retrieval , '-v'])

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

    # Construct test case
    test_case = {
      'test_case_name': self.test_name,
      'data': [
        self.data_server_retrieval + urllib.quote(self.zip_file_hash),
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
      'cleanup': 'data',
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
  try:
    result = urllib2.urlopen(test_url, manifest_text).read()

    # Check that we can read the output as a JSON string
    json.loads(result)
  except (ValueError, TypeError, urllib2.URLError) as e:
    print 'Failed to send test for ' + test_name
    print e
    return 1

  return 0


def main():
  """Packages up a Slavelastic test and send it to swarm.  Receive output from
  all shards and print it to stdout.

  Args
    slavelastic manifest file
    number of shards
    ...
  """
  # Parses arguments
  parser = optparse.OptionParser(
      usage='%prog [options]',
      description=DESCRIPTION)
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
    parser.error('Unknown args, ' + args)

  if not options.os_image:
    parser.error('Must specify an os image')
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
