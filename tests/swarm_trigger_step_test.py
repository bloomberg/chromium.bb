#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
FILE_HASH = u'brodoyouevenhash'
TEST_NAME = u'unit_tests'
STDOUT_FOR_TRIGGER_LEN = 188


def chromium_tasks(retrieval_url):
  return [
    {
      u'action': [
        u'python', u'run_isolated.py',
        u'--hash', FILE_HASH,
        u'--remote', retrieval_url + u'default-gzip/',
      ],
      u'decorate_output': False,
      u'test_name': u'Run Test',
      u'time_out': 600,
    },
    {
      u'action' : [
          u'python', u'swarm_cleanup.py',
      ],
      u'decorate_output': False,
      u'test_name': u'Clean Up',
      u'time_out': 600,
    }
  ]


def generate_expected_json(
    shards,
    os_image,
    working_dir,
    data_server,
    profile):
  retrieval_url = data_server + '/content/retrieve/'
  os_value = unicode(swarm_trigger_step.PLATFORM_MAPPING[os_image])
  expected = {
    u'cleanup': u'root',
    u'configurations': [
      {
        u'config_name': os_value,
        u'dimensions': {
          u'os': os_value,
        },
        u'min_instances': shards,
      },
    ],
    u'data': [[retrieval_url + u'default/', u'swarm_data.zip']],
    u'env_vars': {},
    u'restart_on_failure': True,
    u'test_case_name': TEST_NAME,
    u'tests': chromium_tasks(retrieval_url),
    u'working_dir': unicode(working_dir),
    u'priority': 101,
  }
  if shards > 1:
    expected[u'env_vars'][u'GTEST_SHARD_INDEX'] = u'%(instance_index)s'
    expected[u'env_vars'][u'GTEST_TOTAL_SHARDS'] = u'%(num_instances)s'
  if profile:
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
    manifest = swarm_trigger_step.Manifest(
        manifest_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=2,
        test_filter='*',
        os_image='win32',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        verbose=False,
        profile=False,
        priority=101)

    swarm_trigger_step.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=2,
        os_image='win32',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        profile=False)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux(self):
    """A basic linux manifest test to ensure that windows specific values
       aren't used.
    """
    manifest = swarm_trigger_step.Manifest(
        manifest_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        verbose=False,
        profile=False,
        priority=101)

    swarm_trigger_step.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=1,
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        profile=False)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux_profile(self):
    manifest = swarm_trigger_step.Manifest(
        manifest_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        verbose=False,
        profile=True,
        priority=101)

    swarm_trigger_step.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=1,
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        profile=True)
    self.assertEqual(expected, manifest_json)

  def test_process_manifest_success(self):
    self.mock(swarm_trigger_step.run_isolated, 'url_open', MockUrlOpenNoZip)

    result = swarm_trigger_step.process_manifest(
        file_sha1=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        swarm_url='http://localhost:8082',
        verbose=False,
        profile=False,
        priority=101)
    self.assertEqual(0, result)

    # Just assert it printed enough, since it contains variable output.
    out = sys.stdout.getvalue()
    self.assertTrue(len(out) > STDOUT_FOR_TRIGGER_LEN)
    self.assertTrue('Zip file not on server, starting uploading.' in out)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_process_manifest_success_zip_already_uploaded(self):
    self.mock(swarm_trigger_step.run_isolated, 'url_open', MockUrlOpenHasZip)

    result = swarm_trigger_step.process_manifest(
        file_sha1=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        os_image='linux2',
        working_dir='swarm_tests',
        data_server='http://localhost:8081',
        swarm_url='http://localhost:8082',
        verbose=False,
        profile=False,
        priority=101)
    self.assertEqual(0, result)

    # Just assert it printed enough, since it contains variable output.
    out = sys.stdout.getvalue()
    self.assertTrue(len(out) > STDOUT_FOR_TRIGGER_LEN)
    self.assertTrue('Zip file already on server, no need to reupload.' in out)
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
