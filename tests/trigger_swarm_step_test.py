#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import os
import StringIO
import sys
import unittest

#import test_env  # pylint: disable=W0403,W0611

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
import trigger_swarm_step as run_slavelastic

FILE_NAME = "test.isolated"
FILE_HASH = hashlib.sha1(FILE_NAME).hexdigest()
TEST_NAME = "unit_tests"
TEST_FILTER = '*'
CLEANUP_SCRIPT_NAME = 'swarm_cleanup.py'

ENV_VARS = {
    'GTEST_FILTER': TEST_FILTER,
    'GTEST_SHARD_INDEX': '%(instance_index)s',
    'GTEST_TOTAL_SHARDS': '%(num_instances)s'
}

class Options(object):
  def __init__(self, working_dir="swarm_tests", shards=1,
               os_image='win32', swarm_url='http://localhost:8080',
               data_server='http://localhost:8081'):
    self.working_dir = working_dir
    self.shards = shards
    self.os_image = os_image
    self.swarm_url = swarm_url
    self.data_server = data_server


def GenerateExpectedJSON(options):
  platform_mapping =  {
    'cygwin': 'Windows',
    'darwin': 'Mac',
    'linux2': 'Linux',
    'win32': 'Windows'
  }

  retrieval_url = options.data_server + '/content/retrieve?hash_key='

  expected = {
    'test_case_name': TEST_NAME,
    'data': [[retrieval_url, 'swarm_data.zip']],
    'tests' : [
      {
        'action': [
          'python', run_slavelastic.RUN_TEST_NAME,
          '--hash', FILE_HASH,
          '--remote', retrieval_url,
          '-v'
        ],
        'test_name': 'Run Test',
        'time_out': 600
      },
      {
        'action' : [
            'python', CLEANUP_SCRIPT_NAME
        ],
        'test_name': 'Clean Up',
        'time_out': 600
      }
    ],
    'env_vars': ENV_VARS,
    'configurations': [
      {
        'min_instances': options.shards,
        'config_name': platform_mapping[options.os_image],
        'dimensions': {
          'os': platform_mapping[options.os_image],
          'vlan': ''
        },
      },
    ],
    'working_dir': options.working_dir,
    'cleanup': 'data'
  }

  if options.os_image == 'win32':
    expected['tests'].append(
      {
        'action': [
          'python',
          'kill_processes.py',
          '--handle_exe',
          'handle.exe'
        ],
        'test_name': 'Kill Processes',
        'time_out': 600
      }
    )

  return expected


class MockZipFile(object):
  def __init__(self, filename, mode):
    pass

  def write(self, source, dest=None):
    pass

  def close(self):
    pass

def MockUrlOpen(_url_or_request, _body=None):
  return StringIO.StringIO('{}')


class ManifestTest(unittest.TestCase):
  def test_basic_manifest(self):
    options = Options(shards=2)
    manifest = run_slavelastic.Manifest(FILE_HASH, TEST_NAME, options.shards,
                                        '*', options)

    manifest_json = json.loads(manifest.to_json())

    expected = GenerateExpectedJSON(options)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux(self):
    """A basic linux manifest test to ensure that windows specific values
       aren't used.
    """

    options = Options(os_image='linux2')
    manifest = run_slavelastic.Manifest(FILE_HASH, TEST_NAME, options.shards,
                                        '*', options)

    manifest_json = json.loads(manifest.to_json())

    expected = GenerateExpectedJSON(options)
    self.assertEqual(expected, manifest_json)

  def test_process_manifest_success(self):
    run_slavelastic.zipfile.ZipFile = MockZipFile
    run_slavelastic.urllib2.urlopen = MockUrlOpen

    options = Options()
    self.assertEqual(
        0, run_slavelastic.ProcessManifest(FILE_HASH, TEST_NAME, options.shards,
                                           '*', options))


if __name__ == '__main__':
  unittest.main()
