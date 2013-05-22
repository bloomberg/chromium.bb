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

import auto_stub

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
import swarm_trigger_step

FILE_NAME = u'test.isolated'
FILE_HASH = unicode(hashlib.sha1(FILE_NAME).hexdigest())
TEST_NAME = u'unit_tests'
CLEANUP_SCRIPT_NAME = u'swarm_cleanup.py'
STDOUT_FOR_TRIGGER_LEN = 188


class Options(object):
  def __init__(self, working_dir=u'swarm_tests', shards=1,
               os_image='win32', swarm_url=u'http://localhost:8080',
               data_server=u'http://localhost:8081'):
    self.working_dir = working_dir
    self.shards = shards
    self.os_image = os_image
    self.swarm_url = swarm_url
    self.data_server = data_server
    self.verbose = False
    self.profile = False


def GenerateExpectedJSON(options):
  platform_mapping =  {
    'cygwin': u'Windows',
    'darwin': u'Mac',
    'linux2': u'Linux',
    'win32': u'Windows'
  }

  retrieval_url = options.data_server + '/content/retrieve/'

  expected = {
    u'cleanup': u'root',
    u'configurations': [
      {
        u'config_name': platform_mapping[options.os_image],
        u'dimensions': {
          u'os': platform_mapping[options.os_image],
          u'vlan': u'm4',
        },
        u'min_instances': options.shards,
      },
    ],
    u'data': [[retrieval_url + u'default/', u'swarm_data.zip']],
    u'env_vars': {},
    u'restart_on_failure': True,
    u'test_case_name': TEST_NAME,
    u'tests' : [
      {
        u'action': [
          u'python', unicode(swarm_trigger_step.RUN_TEST_NAME),
          u'--hash', FILE_HASH,
          u'--remote', retrieval_url + u'default-gzip/',
        ],
        u'decorate_output': True,
        u'test_name': u'Run Test',
        u'time_out': 600,
      },
      {
        u'action' : [
            u'python', CLEANUP_SCRIPT_NAME,
        ],
        u'decorate_output': True,
        u'test_name': u'Clean Up',
        u'time_out': 600,
      }
    ],
    u'working_dir': options.working_dir,
  }
  if options.shards > 1:
    expected[u'env_vars'][u'GTEST_SHARD_INDEX'] = u'%(instance_index)s'
    expected[u'env_vars'][u'GTEST_TOTAL_SHARDS'] = u'%(num_instances)s'
  if options.profile:
    expected[u'tests'][0][u'action'].append(u'--verbose')
  return expected


class MockZipFile(object):
  def __init__(self, filename, mode):
    pass

  def write(self, source, dest=None):
    pass

  def close(self):
    pass


def MockUrlOpen(url, _data, has_return_value):
  if '/content/contains' in url:
    return StringIO.StringIO(has_return_value)
  return StringIO.StringIO('{}')


def MockUrlOpenHasZip(url, data=None, content_type=None):
  assert content_type in (None, 'application/json', 'application/octet-stream')
  return MockUrlOpen(url, data, has_return_value=chr(1))


def MockUrlOpenNoZip(url, data=None, content_type=None):
  assert content_type in (None, 'application/json', 'application/octet-stream')
  return MockUrlOpen(url, data, has_return_value=chr(0))


class ManifestTest(auto_stub.TestCase):
  def setUp(self):
    self.mock(swarm_trigger_step.socket, 'gethostname', lambda: 'vm1-m4')
    self.mock(swarm_trigger_step.time, 'sleep', lambda x: None)
    self.mock(swarm_trigger_step.zipfile, 'ZipFile', MockZipFile)
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def tearDown(self):
    if not self.has_failed():
      self._check_output('', '')
    super(ManifestTest, self).tearDown()

  def _check_output(self, out, err):
    self.assertEqual(out, sys.stdout.getvalue())
    self.assertEqual(err, sys.stderr.getvalue())

    # Flush their content by mocking them again.
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def test_basic_manifest(self):
    options = Options(shards=2)
    manifest = swarm_trigger_step.Manifest(
        FILE_HASH, TEST_NAME, options.shards, '*', options)

    manifest_json = json.loads(manifest.to_json())

    expected = GenerateExpectedJSON(options)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux(self):
    """A basic linux manifest test to ensure that windows specific values
       aren't used.
    """
    options = Options(os_image='linux2')
    manifest = swarm_trigger_step.Manifest(
        FILE_HASH, TEST_NAME, options.shards, '*', options)

    manifest_json = json.loads(manifest.to_json())

    expected = GenerateExpectedJSON(options)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux_profile(self):
    options = Options(os_image='linux2')
    options.profile = True
    manifest = swarm_trigger_step.Manifest(
        FILE_HASH, TEST_NAME, options.shards, '*', options)

    manifest_json = json.loads(manifest.to_json())

    expected = GenerateExpectedJSON(options)
    self.assertEqual(expected, manifest_json)

  def test_process_manifest_success(self):
    swarm_trigger_step.run_isolated.url_open = MockUrlOpenNoZip
    options = Options()
    self.assertEqual(
        0,
        swarm_trigger_step.ProcessManifest(
            FILE_HASH, TEST_NAME, options.shards, '*', options))
    self.assertTrue(len(sys.stdout.getvalue()) > STDOUT_FOR_TRIGGER_LEN)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_process_manifest_success_zip_already_uploaded(self):
    swarm_trigger_step.run_isolated.url_open = MockUrlOpenHasZip
    options = Options()
    self.assertEqual(
        0,
        swarm_trigger_step.ProcessManifest(
            FILE_HASH, TEST_NAME, options.shards, '*', options))
    self.assertTrue(len(sys.stdout.getvalue()) > STDOUT_FOR_TRIGGER_LEN)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_no_dir(self):
    try:
      swarm_trigger_step.main([])
      self.fail()
    except SystemExit as e:
      self.assertEqual(2, e.code)
      self._check_output(
          '',
          'Usage: swarm_trigger_step_test.py [options]\n\n'
          'swarm_trigger_step_test.py: error: Must specify the data '
          'directory\n')

  def test_no_request(self):
    try:
      swarm_trigger_step.main(['-d', '.'])
      self.fail()
    except SystemExit as e:
      self.assertEqual(2, e.code)
      self._check_output(
          '',
          'Usage: swarm_trigger_step_test.py [options]\n\n'
          'swarm_trigger_step_test.py: error: At least one --run_from_hash is '
          'required.\n')


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
